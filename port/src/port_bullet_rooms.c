/* Wall bullet holes follow every drawn room that holds their wall (FORK-068). */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bg.h"
#include "game/explosion.h"
#include "port_bullet_rooms.h"

extern f32 sqrtf(f32);

/* The probe crosses the wall this far either side of the hole (world units);
 * only geometry right at the hole can answer it. */
#define PROBE_DIST 8.0f
#define ROOM_WORDS ((MAXROOMCOUNT + 31) / 32)

typedef struct {
    coord3d pos, normal;
    u8 wall;                    /* a wall hole with a usable normal */
    u32 tested[ROOM_WORDS];     /* rooms probed for this hole */
    u32 shared[ROOM_WORDS];     /* rooms with geometry at the hole */
} HoleRooms;

static HoleRooms holes[BULLET_IMPACT_BUFFER_LEN];
static coord3d pendingPos, pendingNormal;
static s32 pendingWall;

void portBulletImpactNoteWall(const coord3d *hitpos, const coord3d *normal)
{
    const f32 len = sqrtf(normal->x * normal->x + normal->y * normal->y + normal->z * normal->z);

    pendingWall = len > 1e-4f;
    pendingPos = *hitpos;
    if (pendingWall)
    {
        pendingNormal.x = normal->x / len;
        pendingNormal.y = normal->y / len;
        pendingNormal.z = normal->z / len;
    }
}

void portBulletImpactAssign(s32 slot, PropRecord *prop)
{
    HoleRooms *h;
    s32 k;

    if (slot >= 0 && slot < BULLET_IMPACT_BUFFER_LEN)
    {
        h = &holes[slot];
        /* Prop holes move with their prop and ignore rooms. */
        h->wall = (u8) (prop == NULL && pendingWall);
        h->pos = pendingPos;
        h->normal = pendingNormal;
        for (k = 0; k < ROOM_WORDS; k++)
        {
            h->tested[k] = 0;
            h->shared[k] = 0;
        }
    }
    pendingWall = 0;
}

static s32 holeInRoom(HoleRooms *h, s32 room)
{
    const u32 bit = 1u << (room & 31);
    const s32 word = room >> 5;

    if (!(h->tested[word] & bit))
    {
        coord3d from, to;
        HitThing hit;

        h->tested[word] |= bit;
        from.x = h->pos.x + h->normal.x * PROBE_DIST;
        from.y = h->pos.y + h->normal.y * PROBE_DIST;
        from.z = h->pos.z + h->normal.z * PROBE_DIST;
        to.x = h->pos.x - h->normal.x * PROBE_DIST;
        to.y = h->pos.y - h->normal.y * PROBE_DIST;
        to.z = h->pos.z - h->normal.z * PROBE_DIST;
        if (bgTestBulletHitBackground(&from, &to, room, &hit))
        {
            h->shared[word] |= bit;
        }
    }
    return (h->shared[word] & bit) != 0;
}

s32 portBulletImpactRoomDrawn(s32 slot, s32 room)
{
    s32 drawn[MAXROOMCOUNT];
    s32 numdrawn, i;
    HoleRooms *h;

    if (getROOMID_isRendered(room))
    {
        return 1;
    }
    if (slot < 0 || slot >= BULLET_IMPACT_BUFFER_LEN || !holes[slot].wall)
    {
        return 0;
    }
    h = &holes[slot];
    numdrawn = bgCopyDrawnRoomsToList(drawn, MAXROOMCOUNT);
    for (i = 0; i < numdrawn; i++)
    {
        if (drawn[i] > 0 && drawn[i] < MAXROOMCOUNT && holeInRoom(h, drawn[i]))
        {
            return 1;
        }
    }
    return 0;
}
