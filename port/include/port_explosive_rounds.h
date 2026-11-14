/* Explosive Rounds (FORK-078): a small blast where each bullet lands.
 *
 * Off by default. Visual adds a quick flash, a brief pale puff,
 * shrapnel and a small explosion sound at the impact without changing any
 * damage. Explosive makes that blast do splash damage: it hurts characters
 * (Bond included) and damages objects around the impact, so rounds can set
 * off barrels and other explosive props.
 *
 * Where a round lands:
 * - Bond's bullet that reaches a wall or floor: the point the native impact
 *   spark uses, 26 units off the surface (chrprop.c).
 * - Bond's bullet stopped by a guard, an object or a door: the farthest hit
 *   of that bullet, pulled 26 units back towards the camera.
 * - A guard's bullet: where the native spark appears (chraction.c), which
 *   includes another guard it strikes; a shot that hits Bond makes no blast,
 *   so no explosion fills the screen.
 * - A shotgun blast (five pellets) makes one blast, at the first pellet
 *   that lands.
 *
 * Explosive Rounds For chooses whose bullets explode: Everyone (default) or
 * Players Only. Demo playback keeps the native impacts, since recorded
 * input expects them. A round never takes the last explosion slots under
 * the explosion cap, so grenades, mines and props still explode with
 * Unlimited Explosions off. Rounds do not start the screen shake. */
#ifndef PORT_EXPLOSIVE_ROUNDS_H
#define PORT_EXPLOSIVE_ROUNDS_H

#include <ultra64.h>
#include <bondtypes.h>

#define PORT_EXPLOSIVE_ROUNDS_OFF 0
#define PORT_EXPLOSIVE_ROUNDS_VISUAL 1
#define PORT_EXPLOSIVE_ROUNDS_EXPLOSIVE 2
#define PORT_EXPLOSIVE_ROUNDS_DEFAULT PORT_EXPLOSIVE_ROUNDS_OFF
/* explosive_rounds_guards: 1 = everyone's rounds, 0 = players only */
#define PORT_EXPLOSIVE_ROUNDS_GUARDS_DEFAULT 1

/* The fork's rows after Rare's 22 in g_ExplosionTypes (explosion.c). The
 * player and guard Explosive rows are identical; the guard row only stops
 * a guard's blast from showing Bond's hit markers. */
#define PORT_EXPLOSION_ROUND_VISUAL 22
#define PORT_EXPLOSION_ROUND_PLAYER 23
#define PORT_EXPLOSION_ROUND_GUARD 24

/* Free explosion slots a round leaves for everything else. */
#define PORT_EXPLOSIVE_ROUNDS_RESERVE 2
/* How far a blast sits off the surface it hit, as the native spark does. */
#define PORT_EXPLOSIVE_ROUNDS_STANDOFF 26.0f

#define PORT_ROUND_BY_PLAYER 0
#define PORT_ROUND_BY_GUARD 1

static inline s32 portExplosiveRoundsClamp(s32 value)
{
    if (value < PORT_EXPLOSIVE_ROUNDS_OFF) return PORT_EXPLOSIVE_ROUNDS_OFF;
    return value > PORT_EXPLOSIVE_ROUNDS_EXPLOSIVE ? PORT_EXPLOSIVE_ROUNDS_EXPLOSIVE : value;
}

/* The explosion type a round makes, or -1 for none. */
static inline s32 portExplosiveRoundsTypeFor(s32 mode, s32 guards, s32 demo, s32 shooter)
{
    if (demo || mode == PORT_EXPLOSIVE_ROUNDS_OFF) return -1;
    if (shooter == PORT_ROUND_BY_GUARD && !guards) return -1;
    if (mode == PORT_EXPLOSIVE_ROUNDS_VISUAL) return PORT_EXPLOSION_ROUND_VISUAL;
    return shooter == PORT_ROUND_BY_GUARD ? PORT_EXPLOSION_ROUND_GUARD : PORT_EXPLOSION_ROUND_PLAYER;
}

/* Whether a round may take a slot, given the free slots under the cap. */
static inline s32 portExplosiveRoundsSlotAllowed(s32 freeSlots)
{
    return freeSlots > PORT_EXPLOSIVE_ROUNDS_RESERVE;
}

static inline s32 portExplosionIsRound(s32 type)
{
    return type >= PORT_EXPLOSION_ROUND_VISUAL && type <= PORT_EXPLOSION_ROUND_GUARD;
}

/* explosion.c: whether this type starts the screen shake when created.
 * Rare exempts the spark (1) and type 16; the fork also exempts rounds. */
static inline s32 portExplosionStartsShake(s32 type)
{
    return type != 0x10 && type != 1 && !portExplosionIsRound(type);
}

/* explosion.c: whether damage from this type shows the player's hit
 * marker. A guard's round damaging another guard is not Bond's hit. */
static inline s32 portExplosionShowsHitMarker(s32 type)
{
    return type != PORT_EXPLOSION_ROUND_GUARD;
}

/* One blast waiting to be created. */
typedef struct PortRoundBlast {
    coord3d pos;
    StandTile *stan;
    u8 rooms[PROPRECORD_STAN_ROOM_LEN];
    s32 type;
} PortRoundBlast;

/* A shotgun's pellets offer their blasts into this queue; the first one
 * waits for the end of the shot. Outside a shot every blast goes now. */
typedef struct PortRoundQueue {
    s32 deferring;
    s32 held;
    PortRoundBlast blast;
} PortRoundQueue;

static inline void portRoundQueueBegin(PortRoundQueue *queue)
{
    queue->deferring = TRUE;
    queue->held = FALSE;
}

/* TRUE when `blast` should be created now. */
static inline s32 portRoundQueueOffer(PortRoundQueue *queue, const PortRoundBlast *blast)
{
    if (!queue->deferring) return TRUE;
    if (!queue->held) {
        queue->blast = *blast;
        queue->held = TRUE;
    }
    return FALSE;
}

/* Ends a shot; TRUE with `out` set when a pellet left a blast. */
static inline s32 portRoundQueueEnd(PortRoundQueue *queue, PortRoundBlast *out)
{
    const s32 held = queue->deferring && queue->held;

    if (held) *out = queue->blast;
    queue->deferring = FALSE;
    queue->held = FALSE;
    return held;
}

/* Setting (port_explosive_rounds.c). */
s32 portExplosiveRoundsOptionGet(const char *key, s32 *value);
s32 portExplosiveRoundsOptionSet(const char *key, s32 value);
s32 portExplosiveRoundsType(s32 demo, s32 shooter);

/* Game hooks (port_explosive_rounds_game.c). */
/* chrprop.c: Bond's bullet reached a wall or floor at `pos` (already moved
 * off the surface) in `room`. */
void portExplosiveRoundsPlayerWall(coord3d *pos, StandTile *stan, s32 room);
/* chrprop.c: Bond's bullet did not reach a surface; a blast at its farthest
 * guard, object or door hit, if any. */
void portExplosiveRoundsPlayerHits(struct ShotData *shot);
/* chrprop.c: around a shotgun's pellets, for one blast per shot. */
void portExplosiveRoundsShotBegin(void);
void portExplosiveRoundsShotEnd(void);
/* chraction.c: a guard's bullet made its impact spark at `pos`. */
void portExplosiveRoundsGuardImpact(coord3d *pos, StandTile *stan);

#endif
