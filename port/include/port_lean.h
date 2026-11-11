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

s32 portLeanOptionGet(const char *key, s32 *value);
s32 portLeanOptionSet(const char *key, s32 value);
/* bondviewProcessInput: the current player's lean controls this tick. */
s32 portLeanInput(void);

#endif
