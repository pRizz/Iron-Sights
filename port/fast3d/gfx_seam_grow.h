#ifndef GFX_SEAM_GROW_H
#define GFX_SEAM_GROW_H
/* Room Seams (FORK-086): Rare's room meshes have T-junctions whose middle
 * vertex sits up to about a unit off the long edge (measured on Dam: a floor
 * corner 0.65 units from the barrier foot it should meet), so a thin wedge of
 * the floor plane belongs to no triangle and the sky drawn behind the room
 * shows through it. The gap has a size in the world, so close up it is
 * several pixels wide.
 *
 * The renderer covers it with a backing copy of each opaque room triangle,
 * grown in its own plane by a small world distance and drawn just behind the
 * real surfaces, so it only shows where no surface was drawn. This header is
 * the grow: pure, no renderer state. */

#include <math.h>
#include <stdint.h>

/* An edge's key: its two corners' model positions (the room file's integer
 * coordinates), in either order. Edges two triangles share have equal keys;
 * an edge only one triangle has is where a seam can open. */
static inline uint64_t gfx_seam_mix(uint64_t x) { /* splitmix64's finalizer */
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}
static inline uint64_t gfx_seam_edge_key(const int16_t a[3], const int16_t b[3]) {
    const uint64_t pa = ((uint64_t) (uint16_t) a[0] << 32) | ((uint64_t) (uint16_t) a[1] << 16) | (uint16_t) a[2];
    const uint64_t pb = ((uint64_t) (uint16_t) b[0] << 32) | ((uint64_t) (uint16_t) b[1] << 16) | (uint16_t) b[2];
    const uint64_t lo = pa < pb ? pa : pb, hi = pa < pb ? pb : pa;
    return gfx_seam_mix(lo ^ gfx_seam_mix(hi + 0x9E3779B97F4A7C15ull));
}

/* Moves the edges of the 3D triangle p[0..2] named in `mask` (bit i: edge i,
 * corner i to corner i + 1) outward, within the triangle's plane, by `grow`
 * (same units as p); the other edges stay. Each corner goes to where its two
 * edge lines now meet, capped at three times `grow` from where it was so a
 * needle-thin corner does not spike. Writes the corners to out and returns 1,
 * or copies p to out and returns 0 for no edge, a triangle with (almost) no
 * area or a bad input. */
static inline int gfx_seam_grow_plane(const float p[3][3], float grow, unsigned mask, float out[3][3]) {
    float e[3][3], m[3][3], n[3], nl;
    const float cap = 3.0f * grow;
    int i, k;
    for (i = 0; i < 3; i++) {
        for (k = 0; k < 3; k++) {
            out[i][k] = p[i][k];
            e[i][k] = p[(i + 1) % 3][k] - p[i][k]; /* edge i: corner i to i + 1 */
        }
    }
    n[0] = e[0][1] * -e[2][2] - e[0][2] * -e[2][1]; /* (p1 - p0) x (p2 - p0) */
    n[1] = e[0][2] * -e[2][0] - e[0][0] * -e[2][2];
    n[2] = e[0][0] * -e[2][1] - e[0][1] * -e[2][0];
    nl = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (!(grow > 0.0f) || !(nl > 1e-6f) || (mask & 7u) == 0) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        /* edge x normal points out of the triangle, in its plane */
        float ml;
        m[i][0] = e[i][1] * n[2] - e[i][2] * n[1];
        m[i][1] = e[i][2] * n[0] - e[i][0] * n[2];
        m[i][2] = e[i][0] * n[1] - e[i][1] * n[0];
        ml = sqrtf(m[i][0] * m[i][0] + m[i][1] * m[i][1] + m[i][2] * m[i][2]);
        if (!(ml > 1e-9f)) {
            return 0;
        }
        for (k = 0; k < 3; k++) {
            m[i][k] /= ml;
        }
    }
    for (i = 0; i < 3; i++) {
        const int prev = (i + 2) % 3; /* edges prev and i meet at corner i */
        /* o = a m_prev + b m_i with o . m_prev = d_prev and o . m_i = d_i */
        const float dp = (mask >> prev) & 1u ? grow : 0.0f;
        const float di = (mask >> i) & 1u ? grow : 0.0f;
        const float c = m[prev][0] * m[i][0] + m[prev][1] * m[i][1] + m[prev][2] * m[i][2];
        const float s = 1.0f - c * c;
        float o[3], ol;
        for (k = 0; k < 3; k++) {
            o[k] = s > 1e-4f ? ((dp - c * di) * m[prev][k] + (di - c * dp) * m[i][k]) / s : 0.0f;
        }
        ol = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
        for (k = 0; k < 3; k++) {
            out[i][k] = p[i][k] + (ol > cap ? o[k] * cap / ol : o[k]);
        }
    }
    return 1;
}

/* A row-vector perspective projection P (clip = p x P, P[3] the translation)
 * makes clip z and w both linear in the point's depth along the view axis,
 * so z = alpha * w + beta for one alpha and beta. GoldenEye folds the camera's
 * rotation, offset and the level scale into P, which leaves that intact. */

/* clip w gained per world unit along the view axis */
static inline float gfx_seam_w_scale(const float (*P)[4]) {
    return sqrtf(P[0][3] * P[0][3] + P[1][3] * P[1][3] + P[2][3] * P[2][3]);
}

/* alpha: how clip z follows clip w */
static inline float gfx_seam_depth_ratio(const float (*P)[4]) {
    const float ww = P[0][3] * P[0][3] + P[1][3] * P[1][3] + P[2][3] * P[2][3];
    return ww > 1e-12f ? (P[0][2] * P[0][3] + P[1][2] * P[1][3] + P[2][2] * P[2][3]) / ww : 0.0f;
}

/* The near plane's clip w (where z = -w), or 0 for a projection without one */
static inline float gfx_seam_near_w(const float (*P)[4], float alpha) {
    const float beta = P[3][2] - alpha * P[3][3];
    return fabsf(1.0f + alpha) > 1e-6f && -beta / (1.0f + alpha) > 0.0f ? -beta / (1.0f + alpha) : 0.0f;
}

/* Clips one triangle of vertex records (stride floats each, clip x, y, z, w
 * first, then any attributes) to w >= wmin, the projection's near plane,
 * interpolating every float linearly as the rasterizer would. Writes 0, 3 or
 * 4 vertices of a convex polygon to out (room for 4 * stride floats) and
 * returns the count. With every corner in front, depth can be pushed back
 * without the near-plane clip moving the shape on screen. */
static inline int gfx_seam_clip_near(const float* in, int stride, float wmin, float* out) {
    int n = 0, i, k;
    for (i = 0; i < 3; i++) {
        const float* a = in + i * stride;
        const float* b = in + ((i + 1) % 3) * stride;
        const int ain = a[3] >= wmin, bin = b[3] >= wmin;
        if (ain) {
            for (k = 0; k < stride; k++) out[n * stride + k] = a[k];
            n++;
        }
        if (ain != bin) {
            const float t = (wmin - a[3]) / (b[3] - a[3]);
            for (k = 0; k < stride; k++) out[n * stride + k] = a[k] + (b[k] - a[k]) * t;
            out[n * stride + 3] = wmin;
            n++;
        }
    }
    return n == 3 || n == 4 ? n : 0;
}

/* The clip z of the point `back_w` (clip w units) further along the view
 * axis than the corner at clip (z, w), keeping that corner's w: the depth
 * test then sees it that far back while its screen position, perspective and
 * near-plane clip stay as they were. A corner at or behind the eye keeps z. */
static inline float gfx_seam_push_depth(float z, float w, float alpha, float back_w) {
    if (!(w > 1e-3f) || !(back_w > 0.0f)) {
        return z;
    }
    return alpha * w + (z - alpha * w) * w / (w + back_w);
}

#endif
