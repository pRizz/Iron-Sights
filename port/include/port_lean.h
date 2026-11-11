/* Leaning (FORK-073): Lean Left / Lean Right controls for the stock lean.
 *
 * The game already leans: MoveBond eases Bond's body 75 units to the side
 * (currentPlayerSetSwayTarget, with collision) while moveData's rLeanLeft /
 * rLeanRight is set, and the camera does not roll. Inherited, only the
 * strafe input in red-reticle aim mode and the N64 C buttons set them, so
 * ADS and hip fire could not lean. With Leaning on (the default), the held
 * Lean Left (keyboard Z) and Lean Right (C) controls set them too, in any
 * stance; left wins when both are held, as in the stock code. Not while noclip
 * flies (FORK-072) or the controls are not live. */
#ifndef PORT_LEAN_H
#define PORT_LEAN_H

#include <ultra64.h>

#define PORT_LEANING_DEFAULT 1

/* -1 = lean left, +1 = lean right, 0 = neither. */
static inline s32 portLeanFor(s32 enabled, s32 permitted, s32 flying, s32 left, s32 right)
{
    if (!enabled || !permitted || flying) return 0;
    if (left) return -1;
    return right ? 1 : 0;
}

/* Lean Tilt ([display] lean_tilt, degrees: 0 Off by default, 6 Subtle, 12
 * Strong; any whole number up to 20 in midas.ini): an option beyond the stock
 * lean, which has no roll. The camera rolls around its line of sight by that
 * many degrees at a full 75-unit lean, in proportion to how far the body has
 * actually moved (so it eases with the lean and stops where a wall stops
 * it), for every lean source. The look direction, and so the aim, is
 * unchanged. Not in demo playback or while noclip flies. */
#define PORT_LEAN_TILT_DEFAULT 0
#define PORT_LEAN_TILT_MAX 20
#define PORT_LEAN_FULL 75.0f /* currentPlayerSetSwayTarget's full lean */

static inline s32 portLeanTiltClamp(s32 value)
{
    return value < 0 ? 0 : value > PORT_LEAN_TILT_MAX ? PORT_LEAN_TILT_MAX : value;
}

/* Roll in degrees for a body offset of `lateral` units (negative = left);
 * negative rolls left. */
static inline f32 portLeanTiltDegFor(s32 setting, s32 demo, s32 flying, f32 lateral)
{
    f32 fraction = lateral / PORT_LEAN_FULL;

    if (demo || flying || setting <= 0) return 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    if (fraction < -1.0f) fraction = -1.0f;
    return fraction * (f32) setting;
}

s32 portLeanOptionGet(const char *key, s32 *value);
s32 portLeanOptionSet(const char *key, s32 value);
/* bondviewProcessInput: the current player's lean controls this tick. */
s32 portLeanInput(void);
/* bondviewMoveAnimationTick: roll the head basis (look stays; up turns around
 * it) by the current player's Lean Tilt. */
void portLeanTilt(f32 *look, f32 *up);

#endif
