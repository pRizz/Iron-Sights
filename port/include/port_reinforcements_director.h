/* Reinforcements (FORK-081): the director's rules. The engine side is in
 * port_reinforcements_director.c; the level settings in port_reinforcements.h.
 *
 * Once Bond has been noticed (a guard saw him within Rare's 10-second memory,
 * or the alarm is on), the director keeps sending guards for 20 more seconds:
 * one per directorTicks while fewer than directorCap of its guards are alive
 * and Rare's 10 spare character slots stay free. Each comes from a waypoint
 * pad (so it starts on the navigation network) that:
 * - lies 600 to 6000 units from Bond across the floor and within 400 up or down,
 * - has a waypoint route to Bond's nearest waypoint of 2 to 14 steps,
 * - is off screen (its room not drawn, or out of the view) and
 * - has no character within 60 units nor Bond within 300.
 * Of up to ten candidates tried, the one with the shortest route wins. The
 * guard wears the body and guns of the nearest placed soldier, gets Rare's
 * clone AI (run to Bond, then act as a standard guard) and never clones. */
#ifndef PORT_REINFORCEMENTS_DIRECTOR_H
#define PORT_REINFORCEMENTS_DIRECTOR_H

#include <ultra64.h>
#include "port_reinforcements.h"

#define PORT_DIRECTOR_NOTICED_TICKS (20 * PORT_REINFORCEMENTS_TICKS)
#define PORT_DIRECTOR_TRY_TICKS PORT_REINFORCEMENTS_TICKS   /* one pad search a second at most */
#define PORT_DIRECTOR_MIN_DIST 600.0f
#define PORT_DIRECTOR_MAX_DIST 6000.0f
#define PORT_DIRECTOR_MAX_DY 400.0f
#define PORT_DIRECTOR_MIN_HOPS 2
#define PORT_DIRECTOR_MAX_HOPS 14
#define PORT_DIRECTOR_CANDIDATES 10
/* Room an added spawn keeps around it. Rare's own test (chrAdjustPosForSpawn)
 * is a 20-unit circle against standing characters only. */
#define PORT_SPAWN_CLEARANCE 60.0f
#define PORT_SPAWN_CLEARANCE_Y 200.0f
#define PORT_SPAWN_BOND_CLEARANCE 300.0f

/* Is the director sending? lastNoticed < 0: Bond has not been noticed. */
static inline s32 portDirectorActive(s32 lastNoticed, s32 now)
{
    return lastNoticed >= 0 && now - lastNoticed < PORT_DIRECTOR_NOTICED_TICKS;
}

/* May it send one now? lastSpawn < 0: it has not sent one this stage. */
static inline s32 portDirectorDue(const PortReinforcementRules *rules, s32 active, s32 living,
    s32 lastSpawn, s32 now, s32 freeChrs)
{
    if (rules->directorTicks <= 0 || !active || living >= rules->directorCap) return FALSE;
    if (freeChrs < portReinforcementsFreeChrsNeeded(TRUE)) return FALSE;
    return lastSpawn < 0 || now - lastSpawn >= rules->directorTicks;
}

/* A pad's offset from Bond: near enough to matter, far enough to be unseen. */
static inline s32 portDirectorPadInRange(f32 dx, f32 dy, f32 dz)
{
    const f32 flat = dx * dx + dz * dz;
    if (dy > PORT_DIRECTOR_MAX_DY || dy < -PORT_DIRECTOR_MAX_DY) return FALSE;
    return flat >= PORT_DIRECTOR_MIN_DIST * PORT_DIRECTOR_MIN_DIST
        && flat <= PORT_DIRECTOR_MAX_DIST * PORT_DIRECTOR_MAX_DIST;
}

/* waypointFindRoute's step count from the pad to Bond's waypoint. */
static inline s32 portDirectorRouteInRange(s32 hops)
{
    return hops >= PORT_DIRECTOR_MIN_HOPS && hops <= PORT_DIRECTOR_MAX_HOPS;
}

/* Is something at this offset from a spawn point too close to it? */
static inline s32 portSpawnTooClose(f32 dx, f32 dy, f32 dz, f32 radius)
{
    if (dy > PORT_SPAWN_CLEARANCE_Y || dy < -PORT_SPAWN_CLEARANCE_Y) return FALSE;
    return dx * dx + dz * dz < radius * radius;
}

#endif
