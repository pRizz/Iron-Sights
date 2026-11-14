/* Explosive Rounds (FORK-078): the game hooks. The setting is in
 * port_explosive_rounds.c and the rules in port_explosive_rounds.h. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/explosion.h"
#include "port_explosive_rounds.h"

extern s32 get_is_ramrom_flag(void);
extern s32 get_cur_playernum(void);
extern PropRecord *getCurrentPlayerProp(void);
extern Mtxf *currentPlayerGetViewToWorldMtxf(void);
extern void mtx4TransformVecInPlace(Mtxf *matrix, struct coord3d *vector);

static PortRoundQueue roundQueue;

static s32 roundFreeSlots(void)
{
    const s32 cap = portExplosionCap();
    s32 i, free = 0;

    if (g_ExplosionBuffer == NULL) return 0;
    for (i = 0; i < cap && i < EXPLOSION_BUFFER_LEN; i++) {
        if (g_ExplosionBuffer[i].prop == NULL) free++;
    }
    return free;
}

/* GE_ROUNDS_LOG=1 prints each blast and each one held back for the slots. */
static s32 roundLogOn(void)
{
    static s32 on = -1;

    if (on < 0) {
        const char *e = getenv("GE_ROUNDS_LOG");
        on = (e != NULL && e[0] == '1');
    }
    return on;
}

static void roundCreate(PortRoundBlast *blast)
{
    const s32 free = roundFreeSlots();

    if (!portExplosiveRoundsSlotAllowed(free)) {
        if (roundLogOn()) printf("ge: explosive rounds: type %d held back, %d slot(s) free\n", (int) blast->type, (int) free);
        return;
    }
    if (blast->stan == NULL && getCurrentPlayerProp() != NULL) {
        blast->stan = getCurrentPlayerProp()->stan; /* kept by the blast's prop and its smoke */
    }
    if (roundLogOn()) {
        printf("ge: explosive rounds: type %d at (%.0f,%.0f,%.0f) room %d\n", (int) blast->type,
               blast->pos.x, blast->pos.y, blast->pos.z, (int) blast->rooms[0]);
    }
    /* No source prop, so no scorch and the smoke rises at the blast; the
     * shooting player is credited, as the native stray-shot damage does. */
    explosionCreate(NULL, &blast->pos, blast->stan, (s16) blast->type, 0, get_cur_playernum(), blast->rooms, 0);
}

static void roundOffer(PortRoundBlast *blast)
{
    if (portRoundQueueOffer(&roundQueue, blast)) roundCreate(blast);
}

static void roundSetRoom(PortRoundBlast *blast, s32 room)
{
    blast->rooms[0] = (u8) room;
    blast->rooms[1] = 0xFF;
}

/* A prop's room list, or the player's when the prop has none. */
static void roundCopyRooms(PortRoundBlast *blast, PropRecord *prop)
{
    PropRecord *source = (prop->rooms[0] != 0xFF) ? prop : getCurrentPlayerProp();
    s32 i;

    for (i = 0; i < PROPRECORD_STAN_ROOM_LEN - 1 && source != NULL && source->rooms[i] != 0xFF; i++) {
        blast->rooms[i] = source->rooms[i];
    }
    blast->rooms[i] = 0xFF;
}

void portExplosiveRoundsPlayerWall(coord3d *pos, StandTile *stan, s32 room)
{
    PortRoundBlast blast;

    blast.type = portExplosiveRoundsType(get_is_ramrom_flag() != 0, PORT_ROUND_BY_PLAYER);
    if (blast.type < 0 || pos == NULL) return;
    blast.pos = *pos;
    blast.stan = stan;
    roundSetRoom(&blast, room);
    roundOffer(&blast);
}

void portExplosiveRoundsPlayerHits(struct ShotData *shot)
{
    PortRoundBlast blast;
    BulletHit *last = NULL;
    coord3d view;
    f32 len, scale;
    s32 k;

    blast.type = portExplosiveRoundsType(get_is_ramrom_flag() != 0, PORT_ROUND_BY_PLAYER);
    if (blast.type < 0) return;

    for (k = 0; k < 10; k++) {
        BulletHit *hit = &shot->hits[k];
        s32 type;

        if (hit->prop == NULL) continue;
        type = hit->prop->type;
        if (type != PROP_TYPE_CHR && type != PROP_TYPE_VIEWER && type != PROP_TYPE_OBJ
            && type != PROP_TYPE_WEAPON && type != PROP_TYPE_DOOR) {
            continue;
        }
        if (last == NULL || hit->dist > last->dist) last = hit;
    }
    if (last == NULL || shot->viewDir.z == 0.0f) return;

    /* The view-space hit point, as chrHandleBulletHit computes it, moved
     * towards the camera by the standoff. */
    view.x = shot->viewOrigin.x - ((last->dist * shot->viewDir.x) / shot->viewDir.z);
    view.y = shot->viewOrigin.y - ((last->dist * shot->viewDir.y) / shot->viewDir.z);
    view.z = shot->viewOrigin.z - last->dist;
    len = sqrtf((view.x * view.x) + (view.y * view.y) + (view.z * view.z));
    scale = (len > PORT_EXPLOSIVE_ROUNDS_STANDOFF) ? 1.0f - (PORT_EXPLOSIVE_ROUNDS_STANDOFF / len) : 0.0f;
    blast.pos.x = view.x * scale;
    blast.pos.y = view.y * scale;
    blast.pos.z = view.z * scale;
    mtx4TransformVecInPlace(currentPlayerGetViewToWorldMtxf(), &blast.pos);

    blast.stan = last->prop->stan;
    roundCopyRooms(&blast, last->prop);
    roundOffer(&blast);
}

void portExplosiveRoundsShotBegin(void)
{
    portRoundQueueBegin(&roundQueue);
}

void portExplosiveRoundsShotEnd(void)
{
    PortRoundBlast blast;

    if (portRoundQueueEnd(&roundQueue, &blast)) roundCreate(&blast);
}

void portExplosiveRoundsGuardImpact(coord3d *pos, StandTile *stan)
{
    PortRoundBlast blast;

    blast.type = portExplosiveRoundsType(get_is_ramrom_flag() != 0, PORT_ROUND_BY_GUARD);
    if (blast.type < 0 || pos == NULL || stan == NULL) return;
    blast.pos = *pos;
    blast.stan = stan;
    roundSetRoom(&blast, stan->room);
    roundCreate(&blast);
}
