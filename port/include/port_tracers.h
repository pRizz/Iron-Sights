#ifndef PORT_TRACERS_H
#define PORT_TRACERS_H
#include <ultra64.h>

/* Tracer Strength (FORK-062): bullet tracers drawn wider, longer, with a soft
 * glow and a minimum on-screen width so long shots stay visible. 100% is the
 * original tracer exactly. Drawing only: the tracer timing, hits and the game
 * RNG are untouched. Covers every tracer (player, guard, multiplayer, autogun)
 * except the Moonraker laser and watch laser beams. */
#define PORT_TRACER_STRENGTH_MIN 100
#define PORT_TRACER_STRENGTH_MAX 400
#define PORT_TRACER_STRENGTH_DEFAULT 200

typedef struct PortTracerLook {
    f32 width;     /* multiplier on the native strip width */
    f32 length;    /* multiplier on the native streak length */
    f32 minWidth;  /* minimum full width as a fraction of screen height, 0 = none */
    f32 glowWidth; /* glow strip width relative to the core, 0 = no glow */
    u8 glowAlpha;  /* glow strip vertex alpha */
} PortTracerLook;

/* Pure core. */
void portTracerLookFor(s32 percent, PortTracerLook *out);
/* Factor (>= 1) that widens a strip of half-width `radius` (model units, ten
 * per world unit) at view depth `depth` (world units) to the look's minimum. */
f32 portTracerMinWidthScale(f32 radius, f32 depth, f32 tanHalfFovY, const PortTracerLook *look);
/* View depth (world units in front of the camera) of `point` under the
 * world-to-camera matrix `view`; 0 behind the camera. */
f32 portTracerViewDepth(f32 view[4][4], const f32 point[3]);
f32 portTracerTanHalfFov(f32 fovyDegrees);
/* Rewrite the four strip vertices' positions with per-end width scales, using
 * the native layout (0/1 at the start, 2/3 at the end at 0.9 width). */
void portTracerShapeStrip(Vtx *vtx, const f32 right[3], const f32 flare[3], f32 startScale, f32 endScale);
/* Fill `glow` from the core strip `core`: same texture coordinates, `widen`
 * times as wide, at `alpha`. */
void portTracerGlowStrip(Vtx *glow, const Vtx *core, const f32 right[3], const f32 flare[3],
                         f32 startScale, f32 endScale, f32 widen, u8 alpha);

/* Game hooks. */
s32 portTracerOptionGet(const char *key, s32 *value);
s32 portTracerOptionSet(const char *key, s32 value);
/* The current setting's look for a tracer of `item`; FALSE when it is the original. */
s32 portTracerLook(s32 item, PortTracerLook *out);

#endif
