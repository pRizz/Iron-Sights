/* Tracer Strength (FORK-062). gunfire.c's tracer draw (sub_GAME_7F061E18)
 * asks for a look and applies it to the strip it builds; at 100% it draws the
 * native strip untouched. */
#include <math.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
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

/* Game shell. */
static s32 tracerStrength = PORT_TRACER_STRENGTH_DEFAULT;

s32 portTracerOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "tracer_strength") != 0) return FALSE;
    *value = tracerStrength;
    return TRUE;
}

s32 portTracerOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "tracer_strength") != 0) return FALSE;
    tracerStrength = clampStrength(value);
    return TRUE;
}

s32 portTracerLook(s32 item, PortTracerLook *out)
{
    portTracerLookFor(PORT_TRACER_STRENGTH_MIN, out);
    /* The lasers' beams keep their own look. */
    if (item == ITEM_LASER || item == ITEM_WATCHLASER) return FALSE;
    if (tracerStrength == PORT_TRACER_STRENGTH_MIN) return FALSE;
    portTracerLookFor(tracerStrength, out);
    return TRUE;
}
