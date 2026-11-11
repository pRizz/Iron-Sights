/* Projectile Arc (FORK-069), the drawing: marching orange dashes along the
 * predicted flight and, where the round explodes, a ring on the surface it
 * hits where the blast does at least half its damage (outlined, faintly
 * filled and lifted with distance to stay visible far off), or where a
 * knife will strike a character, a see-through knife stuck in them, all drawn in the world
 * by gunRenderFirstPersonGunModels before the gun, under the world
 * projection with the native tracer's cloud render mode but depth-tested,
 * so walls in front of the path hide them. */
#include <math.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/player.h"
#include "game/dyn.h"
#include "game/lv.h"
#include "game/matrixmath.h"
#include "game/othermodemicrocode.h"
#include "game/image_bank.h"
#include "game/bondwalk2.h"
#include "game/explosion.h"
#include "port_projectile_arc.h"
#include "port_tracers.h"

/* include/ puts the N64 C headers ahead of the C library's, and they
 * declare only a few of these. */
extern int strcmp(const char *, const char *);
extern void *memset(void *, int, size_t);
extern float fabsf(float);
extern float fmodf(float, float);
extern float ceilf(float);

extern s16 viGetViewHeight(void);
extern s_explosiontype g_ExplosionTypes[];

#define ARC_BATCH_QUADS 4
#define ARC_MAX_DASHES  160
#define ARC_DASH        36.0f
#define ARC_GAP         24.0f
#define ARC_MARCH       1.5f    /* units per field the dashes move forward */
#define ARC_HALF_WIDTH  2.5f
#define ARC_MIN_PIXELS  2.0f    /* half-width on screen */

typedef struct ArcView {
    Mtxf *w2s;
    coord3d eye;
    f32 tanHalf;
    f32 viewH;
} ArcView;

/* Quads of one texture under one matrix anchored near them; vertices are
 * tenths of a unit from the anchor, as the native tracer stores them. */
typedef struct QuadBatch {
    Gfx *gdl;
    const ArcView *view;
    Vtx *vtx;
    s32 quads;
    coord3d anchor;
} QuadBatch;

static void batchFlush(QuadBatch *b)
{
    Mtx *mtx;
    Mtxf mtxf;
    s32 q;

    if (b->quads == 0) return;
    mtx = dynAllocateMatrix();
    matrix_4x4_set_identity_and_position(&b->anchor, &mtxf);
    matrix_scalar_multiply(0.1f, (f32 *) &mtxf);
    matrix_4x4_multiply_homogeneous_in_place(b->view->w2s, &mtxf);
    matrix_4x4_f32_to_s32(mtxf.m, (s32 (*)[4]) mtx);
    gSPMatrix(b->gdl++, osVirtualToPhysical(mtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPVertex(b->gdl++, osVirtualToPhysical(b->vtx), b->quads * 4, 0);
    for (q = 0; q < b->quads; q++)
    {
        s32 v = q * 4;
        gSP2Triangles(b->gdl++, v, v + 2, v + 3, 0, v, v + 3, v + 1, 0);
    }
    b->quads = 0;
    b->vtx = NULL;
}

static s32 nearAnchor(const coord3d *anchor, const coord3d *p)
{
    return fabsf(p->x - anchor->x) < 3000.0f && fabsf(p->y - anchor->y) < 3000.0f && fabsf(p->z - anchor->z) < 3000.0f;
}

/* corners: start +side, start -side, end +side, end -side. */
static void batchQuad(QuadBatch *b, const coord3d c[4], const s16 tc[4][2], const u8 rgba[4][4])
{
    s32 i;

    for (i = 0; i < 4 && b->quads > 0; i++)
    {
        if (!nearAnchor(&b->anchor, &c[i])) batchFlush(b);
    }
    if (b->quads == 0)
    {
        b->vtx = dynAllocateVertices(ARC_BATCH_QUADS * 4);
        b->anchor = c[0];
    }
    for (i = 0; i < 4; i++)
    {
        Vtx *v = &b->vtx[b->quads * 4 + i];

        v->v.ob[0] = (s16) ((c[i].x - b->anchor.x) * 10.0f);
        v->v.ob[1] = (s16) ((c[i].y - b->anchor.y) * 10.0f);
        v->v.ob[2] = (s16) ((c[i].z - b->anchor.z) * 10.0f);
        v->v.flag = 0;
        v->v.tc[0] = tc[i][0];
        v->v.tc[1] = tc[i][1];
        v->v.cn[0] = rgba[i][0];
        v->v.cn[1] = rgba[i][1];
        v->v.cn[2] = rgba[i][2];
        v->v.cn[3] = rgba[i][3];
    }
    if (++b->quads == ARC_BATCH_QUADS) batchFlush(b);
}

static f32 viewDepth(const ArcView *view, const coord3d *p)
{
    return portTracerViewDepth(view->w2s->m, p->f);
}

static void setRgba(u8 out[4], const u8 rgb[3], f32 alpha)
{
    out[0] = rgb[0];
    out[1] = rgb[1];
    out[2] = rgb[2];
    out[3] = (u8) (alpha < 0.0f ? 0.0f : alpha > 255.0f ? 255.0f : alpha);
}

/* A camera-facing strip from a to b with its texture across it. */
static void drawStrip(QuadBatch *b, const coord3d *pa, const coord3d *pb, f32 halfWidth, f32 minPixels,
                      const sImageTableEntry *image, const u8 rgb[3], f32 alphaA, f32 alphaB)
{
    coord3d dir = { pb->x - pa->x, pb->y - pa->y, pb->z - pa->z };
    coord3d mid = { (pa->x + pb->x) * 0.5f, (pa->y + pb->y) * 0.5f, (pa->z + pb->z) * 0.5f };
    coord3d toEye = { b->view->eye.x - mid.x, b->view->eye.y - mid.y, b->view->eye.z - mid.z };
    coord3d side = { dir.y * toEye.z - dir.z * toEye.y, dir.z * toEye.x - dir.x * toEye.z, dir.x * toEye.y - dir.y * toEye.x };
    f32 len = sqrtf(side.x * side.x + side.y * side.y + side.z * side.z);
    f32 wa = portArcScreenWidth(halfWidth, viewDepth(b->view, pa), b->view->tanHalf, b->view->viewH, minPixels);
    f32 wb = portArcScreenWidth(halfWidth, viewDepth(b->view, pb), b->view->tanHalf, b->view->viewH, minPixels);
    s16 s = (s16) (image->width << 5);
    s16 t = (s16) (image->height << 5);
    const s16 tc[4][2] = { { s, 0 }, { 0, 0 }, { s, t }, { 0, t } };
    coord3d c[4];
    u8 rgba[4][4];

    if (!(len > 0.0f)) return;
    side.x /= len; side.y /= len; side.z /= len;
    c[0] = (coord3d) { pa->x + side.x * wa, pa->y + side.y * wa, pa->z + side.z * wa };
    c[1] = (coord3d) { pa->x - side.x * wa, pa->y - side.y * wa, pa->z - side.z * wa };
    c[2] = (coord3d) { pb->x + side.x * wb, pb->y + side.y * wb, pb->z + side.z * wb };
    c[3] = (coord3d) { pb->x - side.x * wb, pb->y - side.y * wb, pb->z - side.z * wb };
    setRgba(rgba[0], rgb, alphaA);
    setRgba(rgba[1], rgb, alphaA);
    setRgba(rgba[2], rgb, alphaB);
    setRgba(rgba[3], rgb, alphaB);
    batchQuad(b, c, tc, rgba);
}

static const u8 sArcColour[3] = { 255, 150, 40 };   /* warm orange */

static Gfx *drawDashes(Gfx *gdl, const ArcView *view, const PortArcPath *path)
{
    PortArcDash dashes[ARC_MAX_DASHES];
    QuadBatch b = { NULL, view, NULL, 0, { 0.0f, 0.0f, 0.0f } };
    f32 phase = fmodf((f32) g_GlobalTimer * ARC_MARCH, ARC_DASH + ARC_GAP);
    s32 n = portArcDashes(path, ARC_DASH, ARC_GAP, phase, dashes, ARC_MAX_DASHES);
    s32 i;

    texSelect(&gdl, flareimage3, 4, 1, 2);
    b.gdl = gdl;
    for (i = 0; i < n; i++)
    {
        f32 len = sqrtf((dashes[i].b.x - dashes[i].a.x) * (dashes[i].b.x - dashes[i].a.x)
                      + (dashes[i].b.y - dashes[i].a.y) * (dashes[i].b.y - dashes[i].a.y)
                      + (dashes[i].b.z - dashes[i].a.z) * (dashes[i].b.z - dashes[i].a.z));

        drawStrip(&b, &dashes[i].a, &dashes[i].b, ARC_HALF_WIDTH, ARC_MIN_PIXELS, flareimage3, sArcColour,
                  230.0f * portArcFadeIn(dashes[i].distA), 230.0f * portArcFadeIn(dashes[i].distA + len));
    }
    batchFlush(&b);
    return b.gdl;
}

/* Two unit vectors spanning the plane with this normal. */
static void planeBasis(const coord3d *n, coord3d *u, coord3d *v)
{
    coord3d ref = (fabsf(n->y) < 0.9f) ? (coord3d) { 0.0f, 1.0f, 0.0f } : (coord3d) { 1.0f, 0.0f, 0.0f };
    f32 len;

    u->x = ref.y * n->z - ref.z * n->y;
    u->y = ref.z * n->x - ref.x * n->z;
    u->z = ref.x * n->y - ref.y * n->x;
    len = sqrtf(u->x * u->x + u->y * u->y + u->z * u->z);
    u->x /= len; u->y /= len; u->z /= len;
    v->x = n->y * u->z - n->z * u->y;
    v->y = n->z * u->x - n->x * u->z;
    v->z = n->x * u->y - n->y * u->x;
}

#define RING_SEGMENTS        40
#define RING_HALF_WIDTH      2.0f
#define RING_MIN_HALF_PIXELS 2.0f    /* as the dashes: at least 4 pixels wide */
#define RING_HALO_HALF_WIDTH 5.0f
#define RING_HALO_MIN_PIXELS 4.0f    /* the dark halo under it, twice as wide */
#define RING_FILL_ALPHA      45.0f
#define FENCE_HEIGHT         45.0f   /* world units up from the surface */
#define FENCE_MIN_PIXELS     18.0f   /* never shorter than this on screen */
#define FENCE_ALPHA          80.0f   /* at its foot, fading to nothing at its top */

static void ringPoint(const coord3d *c0, const coord3d *u, const coord3d *v, f32 radius, s32 i, coord3d *out)
{
    const f32 ang = (f32) i * (2.0f * M_PI_F / (f32) RING_SEGMENTS);

    out->x = c0->x + (u->x * cosf(ang) + v->x * sinf(ang)) * radius;
    out->y = c0->y + (u->y * cosf(ang) + v->y * sinf(ang)) * radius;
    out->z = c0->z + (u->z * cosf(ang) + v->z * sinf(ang)) * radius;
}

/* The faint fill, flat on the surface: a patch stays visible seen nearly
 * edge-on, where a line does not. */
static void ringFill(QuadBatch *b, const coord3d *c0, const coord3d *u, const coord3d *v, f32 radius)
{
    static const s16 flat[4][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
    u8 rgba[4][4];
    s32 i, k;

    for (k = 0; k < 4; k++) setRgba(rgba[k], sArcColour, RING_FILL_ALPHA);
    for (i = 0; i < RING_SEGMENTS; i++)
    {
        coord3d c[4];

        ringPoint(c0, u, v, radius, i, &c[0]);
        c[1] = *c0;
        ringPoint(c0, u, v, radius, i + 1, &c[2]);
        c[3] = *c0;
        batchQuad(b, c, flat, rgba);
    }
    batchFlush(b);
}

/* A see-through orange fence standing on the ring, along the surface
 * normal, fading out towards its top: it stands up out of the floor, so
 * unlike the ring it does not flatten when seen from afar. */
static void ringFence(QuadBatch *b, const coord3d *c0, const coord3d *u, const coord3d *v, const coord3d *normal,
                      f32 radius, f32 height)
{
    static const s16 flat[4][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
    u8 rgba[4][4];
    s32 i;

    setRgba(rgba[0], sArcColour, FENCE_ALPHA);
    setRgba(rgba[1], sArcColour, 0.0f);
    setRgba(rgba[2], sArcColour, FENCE_ALPHA);
    setRgba(rgba[3], sArcColour, 0.0f);
    for (i = 0; i < RING_SEGMENTS; i++)
    {
        coord3d c[4];

        ringPoint(c0, u, v, radius, i, &c[0]);
        ringPoint(c0, u, v, radius, i + 1, &c[2]);
        c[1] = (coord3d) { c[0].x + normal->x * height, c[0].y + normal->y * height, c[0].z + normal->z * height };
        c[3] = (coord3d) { c[2].x + normal->x * height, c[2].y + normal->y * height, c[2].z + normal->z * height };
        batchQuad(b, c, flat, rgba);
    }
    batchFlush(b);
}

/* The ring's line, as camera-facing strips like the dashes, so it keeps its
 * width on screen from any angle (a band flat on the floor thins to nothing
 * at its near and far sides when seen from afar). */
static void ringLine(QuadBatch *b, const coord3d *c0, const coord3d *u, const coord3d *v, f32 radius,
                     f32 halfWidth, f32 minPixels, const u8 rgb[3], f32 alpha)
{
    coord3d pa, pb;
    s32 i;

    ringPoint(c0, u, v, radius, 0, &pa);
    for (i = 1; i <= RING_SEGMENTS; i++)
    {
        ringPoint(c0, u, v, radius, i, &pb);
        drawStrip(b, &pa, &pb, halfWidth, minPixels, flareimage3, rgb, alpha, alpha);
        pa = pb;
    }
    batchFlush(b);
}

/* The ring on the impact surface where the blast still does at least half
 * its damage as it starts: half the standard explosion's starting reach
 * (explosion_range), as damage falls linearly to nothing at the reach, which
 * then grows to dmg_range. To stay visible far off it is drawn as boldly as
 * the dashes over a dark halo (it reads on light and dark floors), around a
 * faint orange fill, with a fading fence standing on it, and lifted off the
 * surface further with distance, where the depth test could no longer tell
 * it from the floor and broke it into dots. */
static Gfx *drawBlastRing(Gfx *gdl, const ArcView *view, const coord3d *centre, const coord3d *normal, s32 fence)
{
    static const u8 dark[3] = { 40, 18, 0 };
    const f32 radius = g_ExplosionTypes[EXPLOSION_DEF_STANDARD].explosion_range * 0.5f;
    const f32 depth = viewDepth(view, centre);
    const f32 lift = 2.0f + (depth > 0.0f ? depth * 0.004f : 0.0f);
    const f32 height = portArcScreenWidth(FENCE_HEIGHT, depth, view->tanHalf, view->viewH, FENCE_MIN_PIXELS);
    QuadBatch b = { NULL, view, NULL, 0, { 0.0f, 0.0f, 0.0f } };
    coord3d u, v, c0;

    c0.x = centre->x + normal->x * lift;
    c0.y = centre->y + normal->y * lift;
    c0.z = centre->z + normal->z * lift;
    planeBasis(normal, &u, &v);

    texSelect(&gdl, NULL, 4, 1, 0);               /* flat shade, the same cloud blend */
    b.gdl = gdl;
    ringFill(&b, &c0, &u, &v, radius);
    if (fence) ringFence(&b, &c0, &u, &v, normal, radius, height);
    gdl = b.gdl;

    texSelect(&gdl, flareimage3, 4, 1, 2);
    b.gdl = gdl;
    ringLine(&b, &c0, &u, &v, radius, RING_HALO_HALF_WIDTH, RING_HALO_MIN_PIXELS, dark, 170.0f);
    ringLine(&b, &c0, &u, &v, radius, RING_HALF_WIDTH, RING_MIN_HALF_PIXELS, sArcColour, 230.0f);
    return b.gdl;
}

#define BURST_RADIUS     24.0f   /* to a long spike's tip, world units */
#define BURST_MIN_SCREEN 0.045f  /* never smaller than this much of the view's height */
#define BURST_SPIKES     10
#define BURST_LIFT       0.6f    /* of its radius towards the eye, out of the body */

/* One star of the burst, flat to the camera: spikes alternately long and
 * short around a fan from the centre. */
static void burstStar(QuadBatch *b, const coord3d *c0, const coord3d *right, const coord3d *up, f32 radius,
                      f32 spin, const u8 rgb[3], f32 alpha)
{
    static const s16 flat[4][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
    u8 rgba[4][4];
    coord3d prev;
    s32 i, k;

    for (k = 0; k < 4; k++) setRgba(rgba[k], rgb, alpha);
    for (i = 0; i <= BURST_SPIKES * 2; i++)
    {
        const f32 ang = spin + (f32) i * (M_PI_F / (f32) BURST_SPIKES);
        const f32 r = radius * ((i & 1) ? 0.42f : ((i & 2) ? 0.72f : 1.0f));
        const coord3d p = { c0->x + (right->x * cosf(ang) + up->x * sinf(ang)) * r,
                            c0->y + (right->y * cosf(ang) + up->y * sinf(ang)) * r,
                            c0->z + (right->z * cosf(ang) + up->z * sinf(ang)) * r };

        if (i > 0)
        {
            const coord3d q[4] = { *c0, *c0, prev, p };

            batchQuad(b, q, flat, rgba);
        }
        prev = p;
    }
    batchFlush(b);
}

/* Where a round or rocket will explode on a character: a small orange
 * explosion in place of the ring's fence, a star of spikes around a lighter
 * core, flat to the camera, lifted towards it out of the body, and turning
 * and swelling slowly so it reads as alive. */
static Gfx *drawBurst(Gfx *gdl, const ArcView *view, const coord3d *at)
{
    static const u8 core[3] = { 255, 225, 130 };
    const f32 depth = viewDepth(view, at);
    const f32 pulse = 1.0f + 0.08f * sinf((f32) g_GlobalTimer * 0.2f);
    const f32 radius = portArcScreenWidth(BURST_RADIUS, depth, view->tanHalf, view->viewH,
                                         BURST_MIN_SCREEN * view->viewH) * pulse;
    const f32 spin = fmodf((f32) g_GlobalTimer * 0.01f, 2.0f * M_PI_F);
    QuadBatch b = { NULL, view, NULL, 0, { 0.0f, 0.0f, 0.0f } };
    coord3d sight = { at->x - view->eye.x, at->y - view->eye.y, at->z - view->eye.z };
    coord3d right, up, c0;
    f32 len = sqrtf(sight.x * sight.x + sight.y * sight.y + sight.z * sight.z);

    if (!(len > 0.0f)) return gdl;
    sight.x /= len; sight.y /= len; sight.z /= len;
    right = (coord3d) { -sight.z, 0.0f, sight.x };            /* sight x world up */
    len = sqrtf(right.x * right.x + right.z * right.z);
    right = (len > 0.001f) ? (coord3d) { right.x / len, 0.0f, right.z / len } : (coord3d) { 1.0f, 0.0f, 0.0f };
    up = (coord3d) { right.y * sight.z - right.z * sight.y, right.z * sight.x - right.x * sight.z,
                     right.x * sight.y - right.y * sight.x };
    c0 = (coord3d) { at->x - sight.x * radius * BURST_LIFT, at->y - sight.y * radius * BURST_LIFT,
                     at->z - sight.z * radius * BURST_LIFT };

    texSelect(&gdl, NULL, 4, 1, 0);               /* flat shade, the same cloud blend */
    b.gdl = gdl;
    burstStar(&b, &c0, &right, &up, radius, spin, sArcColour, 150.0f);
    burstStar(&b, &c0, &right, &up, radius * 0.5f, -spin * 1.7f, core, 190.0f);
    return b.gdl;
}

#define KNIFE_LENGTH     30.0f   /* tip to pommel, world units */
#define KNIFE_EMBED      0.2f    /* of its length inside the character */
#define KNIFE_MIN_SCREEN 0.06f   /* never shorter than this much of the view's height */
#define KNIFE_ALPHA      170.0f

/* The knife's outline from the tip back, as fractions of its length: where
 * each section starts along it and its half-width there. */
static const f32 sKnifeOutline[][2] = {
    { 0.00f, 0.000f }, { 0.12f, 0.065f }, { 0.55f, 0.055f },  /* blade */
    { 0.55f, 0.140f }, { 0.61f, 0.140f },                     /* guard */
    { 0.61f, 0.045f }, { 0.92f, 0.050f },                     /* grip */
    { 0.92f, 0.080f }, { 1.00f, 0.080f },                     /* pommel */
};

/* Where a knife will strike a character: a see-through orange knife stuck
 * in them, its tip in the body (hidden there by the depth test), facing the
 * camera flat, the blade lighter than the grip. */
static Gfx *drawStruckKnife(Gfx *gdl, const ArcView *view, const PortArcPath *path)
{
    static const s16 flat[4][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
    static const u8 grip[3] = { 205, 100, 20 };
    const f32 depth = viewDepth(view, &path->end);
    const f32 length = portArcScreenWidth(KNIFE_LENGTH, depth, view->tanHalf, view->viewH,
                                         KNIFE_MIN_SCREEN * view->viewH);
    QuadBatch b = { NULL, view, NULL, 0, { 0.0f, 0.0f, 0.0f } };
    coord3d sight = { path->end.x - view->eye.x, path->end.y - view->eye.y, path->end.z - view->eye.z };
    coord3d axis, side, tip;
    f32 len = sqrtf(sight.x * sight.x + sight.y * sight.y + sight.z * sight.z);
    u8 rgba[4][4];
    s32 i, k;

    if (!(len > 0.0f)) return gdl;
    sight.x /= len; sight.y /= len; sight.z /= len;
    portArcKnifeAxis(&path->endDir, &sight, &axis);
    side = (coord3d) { axis.y * sight.z - axis.z * sight.y, axis.z * sight.x - axis.x * sight.z,
                       axis.x * sight.y - axis.y * sight.x };
    len = sqrtf(side.x * side.x + side.y * side.y + side.z * side.z);
    if (!(len > 0.0f)) return gdl;
    side.x /= len; side.y /= len; side.z /= len;
    tip = (coord3d) { path->end.x + axis.x * length * KNIFE_EMBED, path->end.y + axis.y * length * KNIFE_EMBED,
                      path->end.z + axis.z * length * KNIFE_EMBED };

    texSelect(&gdl, NULL, 4, 1, 0);               /* flat shade, the same cloud blend */
    b.gdl = gdl;
    for (i = 0; i + 1 < (s32) (sizeof(sKnifeOutline) / sizeof(sKnifeOutline[0])); i++)
    {
        const f32 *a = sKnifeOutline[i];
        const f32 *c = sKnifeOutline[i + 1];
        coord3d q[4];

        if (c[0] <= a[0]) continue;               /* a step in width, not a section */
        for (k = 0; k < 4; k++)
        {
            const f32 *at = (k < 2) ? a : c;
            const f32 along = at[0] * length;
            const f32 w = ((k & 1) ? -at[1] : at[1]) * length;

            q[k].x = tip.x - axis.x * along + side.x * w;
            q[k].y = tip.y - axis.y * along + side.y * w;
            q[k].z = tip.z - axis.z * along + side.z * w;
            setRgba(rgba[k], (a[0] < 0.55f) ? sArcColour : grip, KNIFE_ALPHA);
        }
        batchQuad(&b, q, flat, rgba);
    }
    batchFlush(&b);
    return b.gdl;
}

/* Before the gun, in the world: the dashes and the ring, hidden by what
 * stands in front of them. */
Gfx *portProjectileArcRender(Gfx *gdl)
{
    const PortArcPath *paths[PORT_ARC_SLOTS];
    s32 rings[PORT_ARC_SLOTS];
    s32 strike = portProjectileArcAuditStrike();
    ArcView view;
    coord3d *eye;
    s32 slot;
    s32 any = FALSE;

    for (slot = 0; slot < PORT_ARC_SLOTS; slot++)
    {
        PortArcLaunch launch;

        paths[slot] = portProjectileArcLast(slot, &launch);
        rings[slot] = launch.kind != PORT_ARC_KNIFE;   /* a knife does not explode */
        any |= paths[slot] != NULL;
    }
    if (!any || !portProjectileArcEnabled()) return gdl;   /* or only the crosshair wants them */

    eye = bondviewGetCurrentPlayersPosition();
    view.w2s = camGetWorldToScreenMtxf();
    view.eye = *eye;
    view.tanHalf = portTracerTanHalfFov(getPlayer_c_perspfovy());
    view.viewH = (f32) viGetViewHeight();

    gSPClearGeometryMode(gdl++, G_CULL_BACK);
    for (slot = 0; slot < PORT_ARC_SLOTS; slot++)
    {
        const PortArcPath *path = paths[slot];
        s32 onChr;

        if (path == NULL) continue;
        onChr = path->impact && (path->endChr || strike);
        gdl = drawDashes(gdl, &view, path);
        if (path->impact && rings[slot])
        {   /* a blast on a character: the burst in place of the fence */
            gdl = drawBlastRing(gdl, &view, &path->ringCentre, &path->ringNormal, !onChr);
            if (path->endChr) gdl = drawBurst(gdl, &view, &path->end);
            else if (onChr)
            {   /* GE_ARC_AUDIT_STRIKE: as if a character stood there, chest high */
                const coord3d at = { path->end.x, path->end.y + 50.0f, path->end.z };

                gdl = drawBurst(gdl, &view, &at);
            }
        }
        else if (path->impact && path->endChr) gdl = drawStruckKnife(gdl, &view, path);
        else if (path->impact && !rings[slot] && path->pointCount > 1 && strike)
        {   /* GE_ARC_AUDIT_STRIKE: as if a character stood at the knife's end,
             * at chest height (on the floor, the floor would hide most of it) */
            PortArcPath shown = *path;
            const coord3d *a = &path->points[path->pointCount - 2];
            const coord3d d = { path->end.x - a->x, path->end.y - a->y, path->end.z - a->z };
            const f32 len = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);

            if (len > 0.0f)
            {
                shown.endDir = (coord3d) { d.x / len, d.y / len, d.z / len };
                shown.end.y += 50.0f;
                gdl = drawStruckKnife(gdl, &view, &shown);
            }
        }
    }
    return gdl;
}
