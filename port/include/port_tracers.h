#ifndef PORT_TRACERS_H
#define PORT_TRACERS_H
#include <ultra64.h>

/* Tags only: bondtypes.h clashes with the platform headers video.c uses. */
struct BeamRecord;
struct coord3d;
struct ChrRecord;

/* Tracer Strength (FORK-062): bullet tracers drawn wider, longer, with a soft
 * glow and a minimum on-screen width so long shots stay visible. 100% is the
 * original tracer exactly. Drawing only: the tracer timing, hits and the game
 * RNG are untouched. Covers every tracer (player, guard, multiplayer, autogun)
 * except the Moonraker laser and watch laser beams. */
#define PORT_TRACER_STRENGTH_MIN 100
#define PORT_TRACER_STRENGTH_MAX 400
#define PORT_TRACER_STRENGTH_DEFAULT 200
/* Shotgun Tracers (FORK-063): one thinner tracer per pellet, on by default. */
#define PORT_SHOTGUN_TRACERS_DEFAULT 1
#define PORT_PELLET_TRACER_SCALE 0.6f /* pellet width and glow vs a bullet tracer */
/* Decorative Pellets (FORK-063): a guard's single shotgun shot also draws
 * four fake pellet tracers in a cone around it, on by default. */
#define PORT_DECORATIVE_PELLETS_DEFAULT 1
#define PORT_DECORATIVE_PELLET_CONE_TAN 0.0524f /* tan 3 degrees: the fan's half-angle */

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

/* Shotgun Tracers' pellet beam, set up and timed like the native bullet
 * tracer (CapBeamLengthAndDecideIfRendered, gunAdvanceBeamTimer) but with
 * `random01` from a private generator, so the game RNG is never drawn. */
void portPelletBeamInit(struct BeamRecord *beam, s32 item, const struct coord3d *start, const struct coord3d *end, f32 random01);
void portPelletBeamAdvance(struct BeamRecord *beam, f32 fields);
/* As gunCreateBeamForHand's multiplayer check: TRUE when the shots from `a`
 * and from `b` to `hit` point within 5 degrees of each other. */
s32 portPelletSameDirection(const struct coord3d *a, const struct coord3d *b, const struct coord3d *hit);
/* A decorative pellet's end: the shot from `from` to `to` turned within the
 * cone (radius01 from its axis, turn01 around it), keeping its length. */
void portPelletDecorativeEnd(const struct coord3d *from, const struct coord3d *to, f32 radius01, f32 turn01,
                             struct coord3d *out);

/* Game hooks. */
s32 portTracerOptionGet(const char *key, s32 *value);
s32 portTracerOptionSet(const char *key, s32 value);
s32 portShotgunTracersEnabled(void);
/* Decorative Pellets is on and Shotgun Tracers, which it extends, is too. */
s32 portDecorativePelletsEnabled(void);
/* The current setting's look for a tracer of `item`; FALSE when it is the
 * original. Shotgun pellet tracers always get a look, PORT_PELLET_TRACER_SCALE
 * as wide and as bright in glow as a bullet tracer. */
s32 portTracerLook(s32 item, PortTracerLook *out);

/* Shotgun Tracers hooks (port_pellet_tracers.c), for the current player. */
/* chrprop.c: pellet `pellet` of a blast has just set the hand's tracer target. */
void portPelletTracerRecord(s32 hand, s32 pellet);
/* gunfire.c: start this blast's pellet beams from the drawn `muzzle`. */
void portPelletTracersCreate(s32 hand, s32 item, const struct coord3d *muzzle, f32 nearDepth);
/* chrprop.c: advance `player`'s pellet beams one tick. */
void portPelletTracersAdvance(s32 player);
/* gunfire.c: draw the hand's live pellet beams. */
Gfx *portPelletTracersDraw(Gfx *gdl, s32 hand);
/* propobj.c: draw multiplayer `player`'s pellet beams from their third-person gun. */
Gfx *portPelletTracersDrawOther(Gfx *gdl, s32 player);
/* chraction.c: a guard's shotgun shot from `from` to `to`. */
void portChrShotgunTracer(const struct ChrRecord *chr, s32 hand, s32 item, const struct coord3d *from, const struct coord3d *to);
/* chrprop.c / propobj.c: advance and draw that character's shotgun beams. */
void portChrShotgunTracersAdvance(const struct ChrRecord *chr);
Gfx *portChrShotgunTracersDraw(Gfx *gdl, const struct ChrRecord *chr);

#endif
