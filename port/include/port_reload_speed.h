/* Reload Speed (FORK-071): the player's reload runs faster, up to instant.
 *
 * The inherited reload (gunfire.c) is four first-person hand states: START
 * checks there is something to load, LOWER tilts the gun down (16 fields at
 * NTSC), SWAP plays the cock sound (17 fields and at least two ticks), and
 * RAISE refills the magazine on its first tick and brings the gun back up
 * (23 fields), about 0.93 s in all; the gun cannot fire until RAISE ends.
 * The pose is computed from each state's field timer, so advancing that
 * timer by a whole multiple of the fields plays the same motion faster.
 * Instant finishes every reload state on the tick it starts, through the
 * native refill and cock sound, so the gun fires again with no dip.
 *
 * Only player reloads: guards reload through their own animations. Weapon
 * switches and the watch keep their speed. Demo playback keeps the native
 * reload, since recorded input expects it. */
#ifndef PORT_RELOAD_SPEED_H
#define PORT_RELOAD_SPEED_H

#include <ultra64.h>
#include <bondconstants.h>

#define PORT_RELOAD_SPEED_INSTANT 0 /* midas.ini reload_speed=0 */
#define PORT_RELOAD_SPEED_DEFAULT 1 /* the original speed */
#define PORT_RELOAD_SPEED_MAX 10

/* The stored setting for any midas.ini or menu value: 0 (Instant) or a
 * multiplier from 1 to PORT_RELOAD_SPEED_MAX. */
static inline s32 portReloadSpeedClamp(s32 value)
{
    if (value < PORT_RELOAD_SPEED_INSTANT) return PORT_RELOAD_SPEED_DEFAULT;
    return value > PORT_RELOAD_SPEED_MAX ? PORT_RELOAD_SPEED_MAX : value;
}

static inline s32 portReloadStateTimed(s32 state)
{
    return state == GUN_ANIM_STATE_RELOAD_LOWER || state == GUN_ANIM_STATE_RELOAD_SWAP
        || state == GUN_ANIM_STATE_RELOAD_RAISE;
}

/* Fields a hand's state timer advances this tick: `clockTimer` scaled by the
 * multiplier while it reloads, unchanged otherwise. Instant needs no
 * scaling, since its reload states end on the tick they start. */
static inline s32 portReloadSpeedFieldsFor(s32 setting, s32 demo, s32 state, s32 clockTimer)
{
    if (demo || setting <= PORT_RELOAD_SPEED_DEFAULT || !portReloadStateTimed(state)) return clockTimer;
    return clockTimer * setting;
}

/* Whether a reload state ends on this tick whatever its timer says. */
static inline s32 portReloadInstantFor(s32 setting, s32 demo)
{
    return !demo && setting == PORT_RELOAD_SPEED_INSTANT;
}

/* Game side (port_reload_speed.c). */
s32 portReloadSpeedOptionGet(const char *key, s32 *value);
s32 portReloadSpeedOptionSet(const char *key, s32 value);
/* gunfire.c: the fields `state`'s timer advances this tick. */
s32 portReloadSpeedFields(s32 state, s32 clockTimer);
/* gunfire.c: Instant is on and the reload ends now. */
s32 portReloadInstant(void);

#endif
