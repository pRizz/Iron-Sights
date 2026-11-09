/* Tracer Strength (FORK-062). gunfire.c's tracer draw (sub_GAME_7F061E18)
 * asks for a look and applies it to the strip it builds; at 100% it draws the
 * native strip untouched. */
#include <math.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "port_tracers.h"

/* Above 100%, with s = percent / 100:
 *   width     s                          (6 world units at 100%)
 *   length    1 + (s - 1) / 2, at most 2 (the streak is 20% of the path at 100%)
 *   minimum   (s - 1) * 1.5 px of a 1080-line screen, as a fraction of height
 *   glow      2.5 times the core, alpha 90 * (s - 1) up to 160 */
#define TRACER_LENGTH_MAX 2.0f
#define TRACER_MIN_PX_PER_STEP 1.5f
#define TRACER_REFERENCE_LINES 1080.0f
#define TRACER_GLOW_WIDTH 2.5f
#define TRACER_GLOW_ALPHA_PER_STEP 90.0f
#define TRACER_GLOW_ALPHA_MAX 160.0f
#define TRACER_MODEL_PER_WORLD 10.0f
/* The native bullet tracer's numbers (CapBeamLengthAndDecideIfRendered). */
#define TRACER_MAX_PATH 10000.0f
#define TRACER_MIN_SPEED_PATH 500.0f
#define TRACER_SPEED_PER_PATH 0.2f
#define TRACER_MAX_STREAK 3000.0f

static s32 clampStrength(s32 percent)
{
    return percent < PORT_TRACER_STRENGTH_MIN ? PORT_TRACER_STRENGTH_MIN
         : percent > PORT_TRACER_STRENGTH_MAX ? PORT_TRACER_STRENGTH_MAX : percent;
}

void portTracerLookFor(s32 percent, PortTracerLook *out)
{
    const f32 extra = (f32) clampStrength(percent) / 100.0f - 1.0f;
    const f32 length = 1.0f + extra * 0.5f;
    const f32 alpha = TRACER_GLOW_ALPHA_PER_STEP * extra;

    out->width = 1.0f + extra;
    out->length = length > TRACER_LENGTH_MAX ? TRACER_LENGTH_MAX : length;
    out->minWidth = extra * TRACER_MIN_PX_PER_STEP / TRACER_REFERENCE_LINES;
    out->glowWidth = extra > 0.0f ? TRACER_GLOW_WIDTH : 0.0f;
    out->glowAlpha = (u8) (alpha > TRACER_GLOW_ALPHA_MAX ? TRACER_GLOW_ALPHA_MAX : alpha);
}

f32 portTracerMinWidthScale(f32 radius, f32 depth, f32 tanHalfFovY, const PortTracerLook *look)
{
    /* The screen spans 2 * depth * tan(fov / 2) world units vertically. */
    const f32 minRadius = look->minWidth * (depth > 0.0f ? depth : 0.0f) * tanHalfFovY * TRACER_MODEL_PER_WORLD;

    if (radius <= 0.0f || minRadius <= radius) return 1.0f;
    return minRadius / radius;
}

f32 portTracerViewDepth(f32 view[4][4], const f32 point[3])
{
    const f32 depth = -(point[0] * view[0][2] + point[1] * view[1][2] + point[2] * view[2][2] + view[3][2]);
    return depth > 0.0f ? depth : 0.0f;
}

f32 portTracerTanHalfFov(f32 fovyDegrees)
{
    const f32 half = fovyDegrees * (3.1415927f / 360.0f);
    const f32 c = cosf(half);
    return c > 0.0001f ? sinf(half) / c : 10000.0f;
}

static void stripCorner(Vtx *v, const f32 right[3], const f32 *maybeFlare, f32 side)
{
    s32 i;
    for (i = 0; i < 3; i++)
    {
        v->v.ob[i] = (s16) ((maybeFlare != NULL ? maybeFlare[i] : 0.0f) + right[i] * side);
    }
}

void portTracerShapeStrip(Vtx *vtx, const f32 right[3], const f32 flare[3], f32 startScale, f32 endScale)
{
    stripCorner(&vtx[0], right, NULL, startScale);
    stripCorner(&vtx[1], right, NULL, -startScale);
    stripCorner(&vtx[2], right, flare, 0.9f * endScale);
    stripCorner(&vtx[3], right, flare, -0.9f * endScale);
}

void portTracerGlowStrip(Vtx *glow, const Vtx *core, const f32 right[3], const f32 flare[3],
                         f32 startScale, f32 endScale, f32 widen, u8 alpha)
{
    s32 i;

    for (i = 0; i < 4; i++)
    {
        glow[i] = core[i];
        glow[i].v.cn[3] = (u8) ((core[i].v.cn[3] * alpha) / 255);
    }
    portTracerShapeStrip(glow, right, flare, startScale * widen, endScale * widen);
}

void portPelletBeamInit(BeamRecord *beam, s32 item, const coord3d *start, const coord3d *end, f32 random01)
{
    f32 dx = end->x - start->x;
    f32 dy = end->y - start->y;
    f32 dz = end->z - start->z;
    f32 len = sqrtf(dx * dx + dy * dy + dz * dz);
    f32 speedPath;

    memset(beam, 0, sizeof(*beam));
    beam->unk00 = -1;
    if (len <= 0.0f) return;

    beam->pos = *start;
    beam->delta.x = dx / len;
    beam->delta.y = dy / len;
    beam->delta.z = dz / len;
    if (len > TRACER_MAX_PATH) len = TRACER_MAX_PATH;

    speedPath = len < TRACER_MIN_SPEED_PATH ? TRACER_MIN_SPEED_PATH : len;
    beam->unk00 = 0;
    beam->item_id = (s8) item;
    beam->unk1c = len;
    beam->unk20 = TRACER_SPEED_PER_PATH * speedPath;
    beam->unk24 = beam->unk20 > TRACER_MAX_STREAK ? TRACER_MAX_STREAK : beam->unk20;
    beam->unk28 = (2.0f * random01 - 1.0f) * beam->unk20;
    if (beam->unk1c <= beam->unk28) beam->unk00 = -1;
}

void portPelletBeamAdvance(BeamRecord *beam, f32 fields)
{
    if (beam->unk00 < 0) return;
    beam->unk28 += beam->unk20 * fields;
    if (beam->unk1c <= beam->unk28)
    {
        beam->unk00 = -1;
        return;
    }
    if (beam->unk00 < 127) beam->unk00++;
}

s32 portPelletSameDirection(const coord3d *a, const coord3d *b, const coord3d *hit)
{
    f32 ax = hit->x - a->x, ay = hit->y - a->y, az = hit->z - a->z;
    f32 bx = hit->x - b->x, by = hit->y - b->y, bz = hit->z - b->z;
    const f32 la = sqrtf(ax * ax + ay * ay + az * az);
    const f32 lb = sqrtf(bx * bx + by * by + bz * bz);

    if (la <= 0.0f || lb <= 0.0f) return FALSE;
    return (ax * bx + ay * by + az * bz) / (la * lb) >= 0.9961947f; /* cos 5 degrees */
}

void portPelletDecorativeEnd(const coord3d *from, const coord3d *to, f32 radius01, f32 turn01, coord3d *out)
{
    f32 d[3] = { to->x - from->x, to->y - from->y, to->z - from->z };
    const f32 len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    f32 u[3], v[3], n[3], ulen, r, a, nlen;
    s32 i;

    *out = *to;
    if (len <= 0.0f) return;
    for (i = 0; i < 3; i++) d[i] /= len;

    /* u: perpendicular to the shot (crossed with whichever axis it is least along). */
    if ((d[1] < 0.0f ? -d[1] : d[1]) < 0.9f) { u[0] = d[2]; u[1] = 0.0f; u[2] = -d[0]; }
    else                     { u[0] = 0.0f; u[1] = -d[2]; u[2] = d[1]; }
    ulen = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    for (i = 0; i < 3; i++) u[i] /= ulen;
    v[0] = d[1] * u[2] - d[2] * u[1];
    v[1] = d[2] * u[0] - d[0] * u[2];
    v[2] = d[0] * u[1] - d[1] * u[0];

    /* Uniform over the cone's disc: sqrt for the radius. */
    r = sqrtf(radius01 < 0.0f ? 0.0f : radius01 > 1.0f ? 1.0f : radius01) * PORT_DECORATIVE_PELLET_CONE_TAN;
    a = turn01 * 6.2831853f;
    for (i = 0; i < 3; i++) n[i] = d[i] + r * (cosf(a) * u[i] + sinf(a) * v[i]);
    nlen = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    out->x = from->x + n[0] / nlen * len;
    out->y = from->y + n[1] / nlen * len;
    out->z = from->z + n[2] / nlen * len;
}

/* Game shell. */
static s32 tracerStrength = PORT_TRACER_STRENGTH_DEFAULT;
static s32 shotgunTracers = PORT_SHOTGUN_TRACERS_DEFAULT;
static s32 decorativePellets = PORT_DECORATIVE_PELLETS_DEFAULT;

s32 portTracerOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "shotgun_tracers") == 0)
    {
        *value = shotgunTracers;
        return TRUE;
    }
    if (strcmp(key, "decorative_pellets") == 0)
    {
        *value = decorativePellets;
        return TRUE;
    }
    if (strcmp(key, "tracer_strength") != 0) return FALSE;
    *value = tracerStrength;
    return TRUE;
}

s32 portTracerOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "shotgun_tracers") == 0)
    {
        shotgunTracers = value != 0;
        return TRUE;
    }
    if (strcmp(key, "decorative_pellets") == 0)
    {
        decorativePellets = value != 0;
        return TRUE;
    }
    if (strcmp(key, "tracer_strength") != 0) return FALSE;
    tracerStrength = clampStrength(value);
    return TRUE;
}

s32 portShotgunTracersEnabled(void) { return shotgunTracers; }
s32 portDecorativePelletsEnabled(void) { return shotgunTracers && decorativePellets; }

s32 portTracerLook(s32 item, PortTracerLook *out)
{
    portTracerLookFor(PORT_TRACER_STRENGTH_MIN, out);
    /* The lasers' beams keep their own look. */
    if (item == ITEM_LASER || item == ITEM_WATCHLASER) return FALSE;
    portTracerLookFor(tracerStrength, out);
    if (item == ITEM_SHOTGUN || item == ITEM_AUTOSHOT)
    {   /* Only Shotgun Tracers draws these; five at once read best thinner. */
        out->width *= PORT_PELLET_TRACER_SCALE;
        out->glowAlpha = (u8) (out->glowAlpha * PORT_PELLET_TRACER_SCALE);
        return TRUE;
    }
    return tracerStrength != PORT_TRACER_STRENGTH_MIN;
}
