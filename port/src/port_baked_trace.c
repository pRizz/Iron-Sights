/* FORK-082: Baked Lighting's CPU ray tracer.
 *
 * A bounding volume hierarchy over the level's opaque room triangles (object
 * median splits on the widest centroid axis, up to 4 triangles per leaf), then
 * per vertex a cosine-weighted hemisphere of rays (a Hammersley set rotated by
 * a per-vertex hash, so neighbours do not band) up to `radius`:
 *  - occlusion: each hit weighs 1 - t / radius, so near geometry darkens most;
 *  - bounce: the colour at each hit (the hit triangle's file colours,
 *    interpolated) times the same weight. The file colours already hold the
 *    level's light, bounces included (adding gathered light brightened
 *    Facility by half), so bounce only tints: the gathered colour at unit
 *    luminance, mixed in by bounce * the share of rays that hit.
 * In linear light (gamma 2.2): out = orig * (1 - ao * occlusion) *
 * mix(1, tint, bounce * hits). The workers are SDL threads pulling 64-vertex chunks;
 * without threads (or when none start) portBakeTraceStep runs time slices.
 * The probe reuses the BVH for the first-person viewmodel. */

/* SDL only: the game's include directories shadow the C library headers
 * (include/string.h, stdlib.h, math.h), which clash with SDL's on MinGW */
#include <SDL.h>
#include "port_baked_light.h"
#if defined(__APPLE__)
#include "port_baked_sort.h" /* MAC-002: macOS's qsort is not stable; glibc's is */
#endif

struct BvhNode {
    float lo[3], hi[3];
    int32_t first; /* leaf: first ordered triangle; interior: the right child (the left is next) */
    int32_t count; /* 0 = interior */
};

#define BAKE_CHUNK 64
#define BAKE_MAX_THREADS 8
#define PROBE_RAYS 64

static struct BvhNode* s_nodes;
static uint32_t s_nnodes;
static float* s_tri;       /* per ordered triangle: v0, e1, e2 */
static uint32_t* s_trivtx; /* per ordered triangle: its 3 global vertex indices */
static struct BakeLevel* s_lvl;
static struct BakeParams s_p;
static SDL_atomic_t s_next, s_done, s_cancel;
static SDL_Thread* s_threads[BAKE_MAX_THREADS];
static int s_nthreads;
static Uint64 s_t0;
static unsigned s_ms;
static float s_lin[256]; /* gamma 2.2 decode */
static float s_probe_dir[PROBE_RAYS][3];

/* ---- build ---------------------------------------------------------------- */

static float* s_cent; /* build only: triangle centroids */
static int s_axis;
#if defined(__APPLE__)
static uint32_t* s_sort_scratch; /* build only: portBakeStableSort's working space */
#endif

#if !defined(__APPLE__) /* macOS sorts with portBakeStableSort instead */
static int cmp_centroid(const void* a, const void* b) {
    const float ca = s_cent[*(const uint32_t*) a * 3 + s_axis], cb = s_cent[*(const uint32_t*) b * 3 + s_axis];
    return (ca > cb) - (ca < cb);
}
#endif

static void grow(float lo[3], float hi[3], const float* p) {
    for (int k = 0; k < 3; k++) {
        if (p[k] < lo[k]) lo[k] = p[k];
        if (p[k] > hi[k]) hi[k] = p[k];
    }
}

static uint32_t build(uint32_t* order, uint32_t first, uint32_t count) {
    const uint32_t ni = s_nnodes++;
    struct BvhNode* n = &s_nodes[ni];
    float clo[3] = { 1e30f, 1e30f, 1e30f }, chi[3] = { -1e30f, -1e30f, -1e30f };
    for (int k = 0; k < 3; k++) {
        n->lo[k] = 1e30f;
        n->hi[k] = -1e30f;
    }
    for (uint32_t i = first; i < first + count; i++) {
        const uint32_t* t = &s_lvl->tris[order[i] * 3];
        for (int v = 0; v < 3; v++) grow(n->lo, n->hi, &s_lvl->pos[t[v] * 3]);
        grow(clo, chi, &s_cent[order[i] * 3]);
    }
    int axis = 0;
    for (int k = 1; k < 3; k++) {
        if (chi[k] - clo[k] > chi[axis] - clo[axis]) axis = k;
    }
    if (count <= 4 || chi[axis] - clo[axis] < 1e-4f) {
        s_nodes[ni].first = (int32_t) first;
        s_nodes[ni].count = (int32_t) count;
        return ni;
    }
    s_axis = axis;
#if defined(__APPLE__)
    /* Equal centroids keep their order, as under glibc's qsort (port_baked_sort.h). */
    portBakeStableSort(order + first, count, s_sort_scratch, s_cent, axis);
#else
    SDL_qsort(order + first, count, sizeof(uint32_t), cmp_centroid);
#endif
    const uint32_t half = count / 2;
    build(order, first, half);
    const uint32_t right = build(order, first + half, count - half);
    s_nodes[ni].first = (int32_t) right;
    s_nodes[ni].count = 0;
    return ni;
}

static void build_bvh(void) {
    const uint32_t nt = s_lvl->ntris;
    uint32_t* order = (uint32_t*) SDL_malloc(sizeof(uint32_t) * (nt ? nt : 1));
    s_cent = (float*) SDL_malloc(sizeof(float) * 3 * (nt ? nt : 1));
    s_nodes = (struct BvhNode*) SDL_malloc(sizeof(struct BvhNode) * 2 * (nt ? nt : 1));
    s_tri = (float*) SDL_malloc(sizeof(float) * 9 * (nt ? nt : 1));
    s_trivtx = (uint32_t*) SDL_malloc(sizeof(uint32_t) * 3 * (nt ? nt : 1));
    s_nnodes = 0;
    for (uint32_t i = 0; i < nt; i++) {
        const uint32_t* t = &s_lvl->tris[i * 3];
        order[i] = i;
        for (int k = 0; k < 3; k++) {
            s_cent[i * 3 + k] =
                (s_lvl->pos[t[0] * 3 + k] + s_lvl->pos[t[1] * 3 + k] + s_lvl->pos[t[2] * 3 + k]) * (1.0f / 3.0f);
        }
    }
#if defined(__APPLE__)
    s_sort_scratch = (uint32_t*) SDL_malloc(sizeof(uint32_t) * (nt ? nt : 1));
#endif
    if (nt > 0) build(order, 0, nt);
#if defined(__APPLE__)
    SDL_free(s_sort_scratch);
    s_sort_scratch = NULL;
#endif
    for (uint32_t i = 0; i < nt; i++) {
        const uint32_t* t = &s_lvl->tris[order[i] * 3];
        const float *a = &s_lvl->pos[t[0] * 3], *b = &s_lvl->pos[t[1] * 3], *c = &s_lvl->pos[t[2] * 3];
        float* d = &s_tri[i * 9];
        for (int k = 0; k < 3; k++) {
            d[k] = a[k];
            d[3 + k] = b[k] - a[k];
            d[6 + k] = c[k] - a[k];
        }
        SDL_memcpy(&s_trivtx[i * 3], t, sizeof(uint32_t) * 3);
    }
    SDL_free(order);
    SDL_free(s_cent);
    s_cent = NULL;
}

/* ---- trace ---------------------------------------------------------------- */

struct Hit {
    float t, u, v;
    uint32_t tri;
};

static int slab(const struct BvhNode* n, const float o[3], const float inv[3], float tmax) {
    float t0 = 0.0f, t1 = tmax;
    for (int k = 0; k < 3; k++) {
        float a = (n->lo[k] - o[k]) * inv[k], b = (n->hi[k] - o[k]) * inv[k];
        if (a > b) {
            const float s = a;
            a = b;
            b = s;
        }
        if (a > t0) t0 = a;
        if (b < t1) t1 = b;
        if (t0 > t1) return 0;
    }
    return 1;
}

static void hit_tri(uint32_t i, const float o[3], const float d[3], struct Hit* h) {
    const float* T = &s_tri[i * 9];
    const float *e1 = T + 3, *e2 = T + 6;
    const float p[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
    const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (SDL_fabsf(det) < 1e-12f) return;
    const float inv = 1.0f / det;
    const float s[3] = { o[0] - T[0], o[1] - T[1], o[2] - T[2] };
    const float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
    if (u < 0.0f || u > 1.0f) return;
    const float q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
    const float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
    if (v < 0.0f || u + v > 1.0f) return;
    const float t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
    if (t > 1e-3f && t < h->t) {
        h->t = t;
        h->u = u;
        h->v = v;
        h->tri = i;
    }
}

/* nearest hit within tmax; 0 = none */
static int trace(const float o[3], const float d[3], float tmax, struct Hit* h) {
    if (s_nnodes == 0) return 0;
    float inv[3];
    for (int k = 0; k < 3; k++) inv[k] = SDL_fabsf(d[k]) > 1e-12f ? 1.0f / d[k] : (d[k] < 0.0f ? -1e30f : 1e30f);
    uint32_t stack[64];
    int sp = 0;
    uint32_t ni = 0;
    h->t = tmax;
    for (;;) {
        const struct BvhNode* n = &s_nodes[ni];
        if (slab(n, o, inv, h->t)) {
            if (n->count > 0) {
                for (int32_t i = 0; i < n->count; i++) hit_tri((uint32_t) (n->first + i), o, d, h);
            } else if (sp < 64) {
                stack[sp++] = (uint32_t) n->first;
                ni = ni + 1;
                continue;
            }
        }
        if (sp == 0) break;
        ni = stack[--sp];
    }
    return h->t < tmax;
}

/* the hit's file colour (lin) or its baked / original ratio (ratio != 0) */
static void hit_colour(const struct Hit* h, int ratio, float out[3]) {
    const uint32_t* v = &s_trivtx[h->tri * 3];
    const float w[3] = { 1.0f - h->u - h->v, h->u, h->v };
    for (int k = 0; k < 3; k++) {
        float a = 0.0f;
        for (int j = 0; j < 3; j++) {
            const float o = s_lin[s_lvl->orig[v[j] * 3 + k]];
            a += w[j] * (ratio ? (o > 1e-4f ? s_lin[s_lvl->baked[v[j] * 3 + k]] / o : 1.0f) : o);
        }
        out[k] = a;
    }
}

/* ---- bake ----------------------------------------------------------------- */

static float radical_inverse(uint32_t b) {
    b = (b << 16u) | (b >> 16u);
    b = ((b & 0x55555555u) << 1u) | ((b & 0xAAAAAAAAu) >> 1u);
    b = ((b & 0x33333333u) << 2u) | ((b & 0xCCCCCCCCu) >> 2u);
    b = ((b & 0x0F0F0F0Fu) << 4u) | ((b & 0xF0F0F0F0u) >> 4u);
    b = ((b & 0x00FF00FFu) << 8u) | ((b & 0xFF00FF00u) >> 8u);
    return (float) b * 2.3283064365386963e-10f;
}

static uint8_t encode(float lin) {
    const float g = SDL_powf(lin < 0.0f ? 0.0f : lin, 1.0f / 2.2f) * 255.0f + 0.5f;
    return (uint8_t) (g > 255.0f ? 255.0f : g);
}

static void bake_vertex(uint32_t i) {
    const float* p = &s_lvl->pos[i * 3];
    const float* n = &s_lvl->nrm[i * 3];
    uint8_t* out = &s_lvl->baked[i * 3];
    const uint8_t* orig = &s_lvl->orig[i * 3];
    if (n[0] == 0.0f && n[1] == 0.0f && n[2] == 0.0f) {
        SDL_memcpy(out, orig, 3);
        return;
    }
    /* an orthonormal basis around n (Duff et al.) */
    const float sign = n[2] >= 0.0f ? 1.0f : -1.0f;
    const float a = -1.0f / (sign + n[2]), b = n[0] * n[1] * a;
    const float t[3] = { 1.0f + sign * n[0] * n[0] * a, sign * b, -sign * n[0] };
    const float s[3] = { b, sign + n[1] * n[1] * a, -n[1] };
    const float eps = s_p.radius * 0.01f + 0.5f;
    const float o[3] = { p[0] + n[0] * eps, p[1] + n[1] * eps, p[2] + n[2] * eps };
    const float rot = (float) ((i * 2654435761u) >> 8) * (1.0f / 16777216.0f);
    float occl = 0.0f, gather[3] = { 0.0f, 0.0f, 0.0f };
    for (int k = 0; k < s_p.rays; k++) {
        const float u1 = ((float) k + 0.5f) / (float) s_p.rays;
        float u2 = radical_inverse((uint32_t) k) + rot;
        if (u2 >= 1.0f) u2 -= 1.0f;
        const float r = SDL_sqrtf(u1), phi = 6.2831853f * u2, z = SDL_sqrtf(1.0f - u1);
        const float lx = r * SDL_cosf(phi), ly = r * SDL_sinf(phi);
        const float d[3] = { t[0] * lx + s[0] * ly + n[0] * z, t[1] * lx + s[1] * ly + n[1] * z,
                             t[2] * lx + s[2] * ly + n[2] * z };
        struct Hit h;
        if (!trace(o, d, s_p.radius, &h)) continue;
        const float w = 1.0f - h.t / s_p.radius;
        float c[3];
        hit_colour(&h, 0, c);
        occl += w;
        for (int j = 0; j < 3; j++) gather[j] += c[j] * w;
    }
    const float hits = occl / (float) s_p.rays; /* also the occlusion */
    const float gl = gather[0] * 0.2126f + gather[1] * 0.7152f + gather[2] * 0.0722f;
    const float k = gl > 1e-5f ? s_p.bounce * hits : 0.0f;
    static int dbg = -1; /* GE_BAKE_DEBUG=1: the occlusion and tint alone, on white (tuning) */
    if (dbg < 0) dbg = SDL_getenv("GE_BAKE_DEBUG") != NULL;
    for (int j = 0; j < 3; j++) {
        const float tint = gl > 1e-5f ? gather[j] / gl : 1.0f;
        const float lin = (dbg ? 1.0f : s_lin[orig[j]]) * (1.0f - s_p.ao * hits) * (1.0f + (tint - 1.0f) * k);
        out[j] = encode(lin);
    }
}

static int run_chunk(void) {
    const int first = SDL_AtomicAdd(&s_next, BAKE_CHUNK);
    if ((uint32_t) first >= s_lvl->nverts || SDL_AtomicGet(&s_cancel)) return 0;
    uint32_t end = (uint32_t) first + BAKE_CHUNK;
    if (end > s_lvl->nverts) end = s_lvl->nverts;
    for (uint32_t i = (uint32_t) first; i < end; i++) bake_vertex(i);
    SDL_AtomicAdd(&s_done, (int) (end - (uint32_t) first));
    return 1;
}

static int SDLCALL worker(void* unused) {
    (void) unused;
    while (run_chunk()) {
    }
    return 0;
}

static void join_workers(void) {
    for (int i = 0; i < s_nthreads; i++) SDL_WaitThread(s_threads[i], NULL);
    s_nthreads = 0;
}

void portBakeTraceStart(struct BakeLevel* lvl, const struct BakeParams* p) {
    portBakeTraceCancel();
    for (int i = 0; i < 256; i++) s_lin[i] = SDL_powf((float) i / 255.0f, 2.2f);
    for (int k = 0; k < PROBE_RAYS; k++) { /* a Fibonacci sphere */
        const float y = 1.0f - 2.0f * ((float) k + 0.5f) / (float) PROBE_RAYS, r = SDL_sqrtf(1.0f - y * y);
        const float phi = 2.39996323f * (float) k;
        s_probe_dir[k][0] = r * SDL_cosf(phi);
        s_probe_dir[k][1] = y;
        s_probe_dir[k][2] = r * SDL_sinf(phi);
    }
    s_lvl = lvl;
    s_p = *p;
    s_t0 = SDL_GetPerformanceCounter();
    build_bvh();
    SDL_AtomicSet(&s_next, 0);
    SDL_AtomicSet(&s_done, 0);
    SDL_AtomicSet(&s_cancel, 0);
    int want = SDL_GetCPUCount() - 2;
    if (want < 1) want = 1;
    if (want > BAKE_MAX_THREADS) want = BAKE_MAX_THREADS;
    if (SDL_getenv("GE_BAKE_THREADS")) want = SDL_atoi(SDL_getenv("GE_BAKE_THREADS"));
    for (s_nthreads = 0; s_nthreads < want; s_nthreads++) {
        s_threads[s_nthreads] = SDL_CreateThread(worker, "ge-bake", NULL);
        if (!s_threads[s_nthreads]) break;
    }
}

int portBakeTraceStep(unsigned budget_us) {
    if (!s_lvl) return 0;
    const int done = (uint32_t) SDL_AtomicGet(&s_done) >= s_lvl->nverts;
    if (s_nthreads == 0 && !done) {
        const Uint64 freq = SDL_GetPerformanceFrequency(), t0 = SDL_GetPerformanceCounter();
        while (run_chunk() && (SDL_GetPerformanceCounter() - t0) * 1000000u / freq < budget_us) {
        }
        if ((uint32_t) SDL_AtomicGet(&s_done) < s_lvl->nverts) return 0;
        /* sliced (the browser build): the time is wall time, frames included */
        s_ms = (unsigned) ((SDL_GetPerformanceCounter() - s_t0) * 1000u / freq);
        return 1;
    }
    if (done && s_nthreads > 0) {
        join_workers();
        s_ms = (unsigned) ((SDL_GetPerformanceCounter() - s_t0) * 1000u / SDL_GetPerformanceFrequency());
    } else if (done && s_ms == 0) {
        s_ms = (unsigned) ((SDL_GetPerformanceCounter() - s_t0) * 1000u / SDL_GetPerformanceFrequency());
    }
    return done;
}

void portBakeTraceCancel(void) {
    SDL_AtomicSet(&s_cancel, 1);
    join_workers();
    SDL_free(s_nodes);
    SDL_free(s_tri);
    SDL_free(s_trivtx);
    s_nodes = NULL;
    s_tri = NULL;
    s_trivtx = NULL;
    s_nnodes = 0;
    s_lvl = NULL;
    s_ms = 0;
}

float portBakeTraceProgress(void) {
    if (!s_lvl || s_lvl->nverts == 0) return 0.0f;
    return (float) SDL_AtomicGet(&s_done) / (float) s_lvl->nverts;
}

void portBakeLinTable(float lin[256]) {
    for (int i = 0; i < 256; i++) lin[i] = SDL_powf((float) i / 255.0f, 2.2f);
}

uint8_t portBakeEncode(float lin) {
    return encode(lin);
}

float portBakeSqrt(float v) {
    return SDL_sqrtf(v);
}

unsigned portBakeTraceMillis(void) {
    return s_ms;
}

void portBakeTraceProbe(const float pos[3], float out_rgb[3]) {
    out_rgb[0] = out_rgb[1] = out_rgb[2] = 1.0f;
    if (!s_lvl || s_nnodes == 0 || (uint32_t) SDL_AtomicGet(&s_done) < s_lvl->nverts) return;
    float wsum = 0.0f, ratio[3] = { 0.0f, 0.0f, 0.0f }, tint[3] = { 0.0f, 0.0f, 0.0f };
    for (int k = 0; k < PROBE_RAYS; k++) {
        struct Hit h;
        if (!trace(pos, s_probe_dir[k], s_p.radius, &h)) continue;
        const float w = 1.0f - h.t / s_p.radius;
        float r[3], c[3];
        hit_colour(&h, 1, r);
        hit_colour(&h, 0, c);
        wsum += w;
        for (int j = 0; j < 3; j++) {
            ratio[j] += r[j] * w;
            tint[j] += c[j] * w;
        }
    }
    if (wsum <= 1e-4f) return;
    const float near = wsum / (float) PROBE_RAYS; /* 0 open .. 1 enclosed */
    const float cover = near * 2.0f > 1.0f ? 1.0f : near * 2.0f;
    const float tl = (tint[0] * 0.2126f + tint[1] * 0.7152f + tint[2] * 0.0722f) / wsum;
    for (int j = 0; j < 3; j++) {
        const float r = 1.0f + (ratio[j] / wsum - 1.0f) * cover;           /* the surroundings' bake */
        const float t = tl > 1e-4f ? (tint[j] / wsum) / tl : 1.0f;         /* their colour */
        out_rgb[j] = r * (1.0f + (t - 1.0f) * 0.25f * cover) * (1.0f - 0.25f * near * s_p.ao);
    }
}
