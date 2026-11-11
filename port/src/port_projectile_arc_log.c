/* Projectile Arc (FORK-069), the GE_ARCDBG log: for each grenade round,
 * rocket, tank shell or hand grenade fired, the drawn arc's launch and impact against the round's
 * real launch, the flight model's impact from that real launch, and where
 * the round really explodes (distances in 3D and along the ground). The
 * game lifts a blast a few units off the floor, so the ground distance is
 * the one to read. scripts/port/audit_projectile_arc.ps1 drives it. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "port_projectile_arc.h"

extern void *memset(void *, int, size_t);
extern s32 get_cur_playernum(void);
extern s32 g_ClockTimer;

typedef struct ArcShot {
    ObjectRecord *obj;
    s32 player;
    s32 pending;        /* the model prediction waits for the next tick */
    s32 item;
    s32 inBetween;      /* fired on an in-between tick (g_ClockTimer 0) */
    PortArcLaunch actual;
    s32 shownValid;
    PortArcLaunch shownLaunch;
    s32 shownImpact;
    coord3d shown;      /* where the drawn arc ended */
    s32 modelImpact;
    coord3d model;      /* the flight model from the round's real launch */
} ArcShot;

static ArcShot sShots[8];
static s32 sShotNext;

s32 portProjectileArcDebug(void)
{
    static s32 dbg = -1;

    if (dbg < 0) dbg = getenv("GE_ARCDBG") != NULL;
    return dbg;
}

static f32 arcDist(const coord3d *a, const coord3d *b)
{
    return sqrtf((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z));
}

static f32 arcDistXZ(const coord3d *a, const coord3d *b)
{
    return sqrtf((a->x - b->x) * (a->x - b->x) + (a->z - b->z) * (a->z - b->z));
}

/* The drawn arc this round was fired along: the slot of its kind whose
 * launch lies nearest (with a weapon in each hand, each has one). */
static const PortArcPath *arcShownFor(const PortArcLaunch *actual, PortArcLaunch *shownLaunch)
{
    const PortArcPath *best = NULL;
    f32 bestDist = 0.0f;
    s32 slot;

    for (slot = 0; slot < PORT_ARC_SLOTS; slot++)
    {
        PortArcLaunch launch;
        const PortArcPath *path = portProjectileArcLast(slot, &launch);
        f32 d;

        if (path == NULL || launch.kind != actual->kind) continue;
        d = arcDist(&launch.pos, &actual->pos);
        if (best == NULL || d < bestDist)
        {
            best = path;
            bestDist = d;
            *shownLaunch = launch;
        }
    }
    return best;
}

/* gunSpawnGLGrenade / gunFireTankShell / generate_player_thrown_grenade,
 * once the round is in the world. A hand grenade's leaving the hand also
 * ends its throw's arc. */
void portProjectileArcShotFired(ObjectRecord *obj, s32 item)
{
    const PortArcPath *shown;
    ArcShot *shot;

    if (item == ITEM_GRENADE) portProjectileArcGrenadeThrown();
    if (!portProjectileArcDebug() || obj == NULL || !(obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)) return;
    if (item != ITEM_TANKSHELLS && item != ITEM_GRENADELAUNCH && item != ITEM_ROCKETLAUNCH && item != ITEM_GRENADE
        && item != ITEM_THROWKNIFE && item != ITEM_REMOTEMINE && item != ITEM_TIMEDMINE && item != ITEM_PROXIMITYMINE)
    {
        return;
    }

    shot = &sShots[sShotNext++ & 7];
    memset(shot, 0, sizeof(*shot));
    shot->obj = obj;
    shot->player = get_cur_playernum();
    shot->pending = TRUE;
    shot->item = item;
    shot->inBetween = g_ClockTimer <= 0;
    shot->actual.kind = (item == ITEM_GRENADELAUNCH) ? PORT_ARC_GRENADE
                      : (item == ITEM_ROCKETLAUNCH) ? PORT_ARC_ROCKET
                      : (item == ITEM_GRENADE) ? PORT_ARC_THROWN
                      : (item == ITEM_THROWKNIFE) ? PORT_ARC_KNIFE
                      : (item == ITEM_TANKSHELLS) ? PORT_ARC_SHELL : PORT_ARC_MINE;
    shot->actual.pos = obj->runtime_pos;
    shot->actual.vel = obj->projectile->speed;
    shot->actual.accel = obj->projectile->unk10;
    portProjectileArcFlightFor(&shot->actual);
    if (item == ITEM_GRENADE) shot->actual.maxFields = (f32) ((WeaponObjRecord *) obj)->timer + 1.0f;
    shown = arcShownFor(&shot->actual, &shot->shownLaunch);
    if (shown != NULL)
    {
        shot->shownValid = TRUE;
        shot->shownImpact = shown->impact;
        shot->shown = shown->end;
    }
}

/* A knife or mine that has stuck or come to rest (its projectile freed):
 * where, against the predictions. */
static void arcLogStopped(ArcShot *shot)
{
    const coord3d *at = &shot->obj->runtime_pos;

    printf("arcdbg: stopped at (%.1f,%.1f,%.1f) shown_err=%.1f shown_xz=%.1f model_err=%.1f model_xz=%.1f\n",
           at->x, at->y, at->z,
           shot->shownImpact ? arcDist(&shot->shown, at) : -1.0f,
           shot->shownImpact ? arcDistXZ(&shot->shown, at) : -1.0f,
           shot->modelImpact ? arcDist(&shot->model, at) : -1.0f,
           shot->modelImpact ? arcDistXZ(&shot->model, at) : -1.0f);
    fflush(stdout);
    shot->obj = NULL;
}

void portProjectileArcLogTick(void)
{
    s32 i;

    for (i = 0; i < 8; i++)
    {
        ArcShot *shot = &sShots[i];

        if (shot->obj != NULL && !shot->pending && shot->player == get_cur_playernum()
            && (shot->actual.kind == PORT_ARC_KNIFE || shot->actual.kind == PORT_ARC_MINE)
            && !(shot->obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
        {
            arcLogStopped(shot);
        }
    }

    for (i = 0; i < 8; i++)
    {
        ArcShot *shot = &sShots[i];
        PortArcPath path;

        if (shot->obj == NULL || !shot->pending || shot->player != get_cur_playernum()) continue;
        shot->pending = FALSE;
        if (portProjectileArcPredict(&shot->actual, &path))
        {
            shot->modelImpact = path.impact;
            shot->model = path.end;
        }
        printf("arcdbg: fired item=%d in_between=%d pos=(%.1f,%.1f,%.1f) vel=(%.2f,%.2f,%.2f)",
               shot->item, shot->inBetween,
               shot->actual.pos.x, shot->actual.pos.y, shot->actual.pos.z,
               shot->actual.vel.x, shot->actual.vel.y, shot->actual.vel.z);
        if (shot->shownValid)
        {
            printf(" shown_launch_err=%.2f (%+.2f,%+.2f,%+.2f) shown_vel_err=%.3f shown_end=(%.1f,%.1f,%.1f) impact=%d",
                   arcDist(&shot->shownLaunch.pos, &shot->actual.pos),
                   shot->shownLaunch.pos.x - shot->actual.pos.x, shot->shownLaunch.pos.y - shot->actual.pos.y,
                   shot->shownLaunch.pos.z - shot->actual.pos.z, arcDist(&shot->shownLaunch.vel, &shot->actual.vel),
                   shot->shown.x, shot->shown.y, shot->shown.z, shot->shownImpact);
        }
        printf(" model_end=(%.1f,%.1f,%.1f) impact=%d\n", shot->model.x, shot->model.y, shot->model.z, shot->modelImpact);
        fflush(stdout);
    }
}

/* objectTick, as a fired round explodes. Distances are along the ground
 * (xz) and in 3D; the game lifts the blast a few units off the floor. */
void portProjectileArcExploded(PropRecord *prop)
{
    const coord3d *at;
    s32 i;

    if (!portProjectileArcDebug() || prop == NULL) return;
    for (i = 0; i < 8; i++)
    {
        ArcShot *shot = &sShots[i];

        if (shot->obj == NULL || shot->obj != prop->obj) continue;
        /* propExplode puts the blast at the round's own position; the prop's
         * stays where the round last stood over the stan. */
        at = &prop->obj->runtime_pos;
        printf("arcdbg: exploded at (%.1f,%.1f,%.1f) shown_err=%.1f shown_xz=%.1f model_err=%.1f model_xz=%.1f\n",
               at->x, at->y, at->z,
               shot->shownImpact ? arcDist(&shot->shown, at) : -1.0f,
               shot->shownImpact ? arcDistXZ(&shot->shown, at) : -1.0f,
               shot->modelImpact ? arcDist(&shot->model, at) : -1.0f,
               shot->modelImpact ? arcDistXZ(&shot->model, at) : -1.0f);
        fflush(stdout);
        shot->obj = NULL;
    }
}
