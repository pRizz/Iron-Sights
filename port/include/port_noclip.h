/* Noclip (FORK-072): Quake-style flight through walls, floors and doors.
 *
 * With Allow Noclip on, the Noclip action (keyboard N by default) toggles it
 * for the player whose control pressed it. While it is on, MoveBond takes
 * the walk step the head animation produces, tilts its forward part along
 * the view's pitch, scales it to 3x (12x with Walk / Sprint held), adds the
 * rise / fall controls (Space; Crouch Hold or Noclip Down) and moves Bond
 * there without the stan wall tests, object and door collision, or gravity.
 * Crouch Hold falls instead of crouching while flying. Each tick the highest floor tile below Bond becomes his tile, so
 * his room, the visible rooms and room objectives follow him; above no floor
 * he keeps the last one. Turning it off drops him onto the floor below, or,
 * with none (under the level or outside it), puts him back where he last
 * flew over one.
 *
 * Never in demo playback, in the tank or once dead. Off at each stage start. */
#ifndef PORT_NOCLIP_H
#define PORT_NOCLIP_H

#include <ultra64.h>
#include <bondtypes.h>

#define PORT_NOCLIP_ALLOW_DEFAULT 0
/* Flight speed as a multiple of the walk step (the walk animation itself stays
 * at its normal speed, so the head bob does not race): 3x, or 12x with
 * Walk / Sprint held. Sprint, Ludicrous Sprint and ADS speeds do not apply. */
#define PORT_NOCLIP_SPEED 3.0f
#define PORT_NOCLIP_SPRINT_SPEED 12.0f
/* Rise / fall speed at 1x, in units per 60 Hz field: about a full-stick run. */
#define PORT_NOCLIP_CLIMB 10.0f

static inline f32 portNoclipSpeedFor(s32 sprint)
{
    return sprint ? PORT_NOCLIP_SPRINT_SPEED : PORT_NOCLIP_SPEED;
}

/* Height gained this tick from the rise / fall controls: `vertical` is +1
 * (Noclip Up), -1 (Noclip Down or Crouch Hold) or 0. */
static inline f32 portNoclipClimbFor(s32 vertical, s32 sprint, f32 fields)
{
    return (f32) vertical * PORT_NOCLIP_CLIMB * portNoclipSpeedFor(sprint) * fields;
}

/* The fly step for a walk step `walk` (x/z; y is ignored): the part along the
 * facing (forwardX, forwardZ), a unit vector, tilts up by the view's pitch;
 * the sideways part stays level. */
static inline void portNoclipFlyStep(const coord3d *walk, f32 forwardX, f32 forwardZ, f32 sinPitch, f32 cosPitch,
                                     coord3d *out)
{
    const f32 along = walk->f[0] * forwardX + walk->f[2] * forwardZ;
    const f32 sideX = walk->f[0] - along * forwardX;
    const f32 sideZ = walk->f[2] - along * forwardZ;

    out->f[0] = sideX + along * cosPitch * forwardX;
    out->f[1] = along * sinPitch;
    out->f[2] = sideZ + along * cosPitch * forwardZ;
}

/* Whether a player flies this tick. */
static inline s32 portNoclipFliesFor(s32 allowed, s32 toggled, s32 demo, s32 inTank, s32 dead)
{
    return allowed && toggled && !demo && !inTank && !dead;
}

/* What a Noclip press does: 1 toggles, 0 tells the player to allow it first. */
static inline s32 portNoclipPressToggles(s32 allowed)
{
    return allowed != 0;
}

/* Game side (port_noclip.c). */
s32 portNoclipOptionGet(const char *key, s32 *value);
s32 portNoclipOptionSet(const char *key, s32 value);
/* player.c initBONDdataforPlayer: a new stage starts on foot. */
void portNoclipResetPlayer(s32 player);
/* MoveBond, after input: take a Noclip press, then start or end flight. */
void portNoclipTick(void);
/* MoveBond: the current player flies this tick. */
s32 portNoclipActive(void);
/* MoveBond: fly by the walk step instead of colliding. */
void portNoclipMove(const coord3d *walk);
/* portHudDraw: the NOCLIP label while the current player flies. */
Gfx *portNoclipDrawLabel(Gfx *g);

#endif
