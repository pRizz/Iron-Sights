/* Projectile Arc (FORK-069), the game's side of the prediction: what the
 * level says about each step of a round's flight, and each player's path.
 * port_projectile_arc_launch.c takes the launch; port_projectile_arc_draw.c
 * draws the result.
 *
 * portProjectileArcNoteLaunch takes the launch each frame at the end of
 * gunUpdateAndFireBothHands, once the gun is posed. portProjectileArcTick
 * then tests the flight with the queries a real round
 * uses each tick (handles_projectile_motion): the level's triangles in the
 * rooms along the step and the visible rooms, then objects and characters,
 * with the player's own collision off as for its own round. The collision
 * scratch state those queries leave is put back afterwards. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/bg.h"
#include "game/chrai.h"
#include "game/stan.h"
#include "game/player.h"
#include "game/gun.h"
#include "game/lv.h"
#include "game/matrixmath.h"
#include "game/stanintersection.h"
#include "port_projectile_arc.h"

/* include/ puts the N64 C headers ahead of the C library's, and they
 * declare only a few of these. */
extern int strcmp(const char *, const char *);
extern void *memset(void *, int, size_t);
extern float fabsf(float);
extern float fmodf(float, float);
extern float ceilf(float);

extern bool projectileFindCollidingProp(PropRecord *ignoreProp, coord3d *worldRayStart, coord3d *worldRayEnd, u32 cdtypes,
                                        coord3d *outHitPos, coord3d *outHitNormal, s32 *rooms);
extern void sub_GAME_7F03D058(PropRecord *prop, bool unset);
extern s32 portTankTurretWeaponHeld(void);
extern coord3d *bondviewGetCurrentPlayersPosition3(void);
extern s32 get_cur_playernum(void);

/* Collision scratch state that the next real query expects to find as the
 * last real query left it (see gunWeaponFovClearFrac). */
extern StandTile *stanSavedColl_tile;
extern s32 stanSavedColl_pointI;
extern struct coord2d stanSavedColl_pntA;
extern struct coord2d stanSavedColl_pntB;
extern s32 D_800413BC;
extern PropRecord *g_portBlockProp;
extern s32 g_portBlockSrc;
extern s32 g_portBlockTick;
extern f32 g_portBlockTop, g_portBlockBot, g_portBlockLo, g_portBlockHi;
extern s32 g_portBlockGate;
extern PropRecord *D_80030B0C;
extern s32 bodypartshot;
extern struct Model *g_CurrentProjectileModel;
extern struct ModelNode *dword_CODE_bss_80075B74;
extern coord3d flt_CODE_bss_80075B78;
extern coord3d flt_CODE_bss_80075B88;

typedef struct ArcScratch {
    StandTile *tile;
    s32 pointI;
    struct coord2d pntA, pntB;
    f32 someMin;
    PropRecord *posData;
    s32 flag;
    PropRecord *bprop;
    s32 bsrc, btick, bgate;
    f32 btop, bbot, blo, bhi;
    PropRecord *hitProp;
    s32 hitPart;
    struct Model *hitModel;
    struct ModelNode *hitNode;
    coord3d hitDir, hitDirView;
} ArcScratch;

static void scratchSave(ArcScratch *s)
{
    s->tile = stanSavedColl_tile;       s->pointI = stanSavedColl_pointI;
    s->pntA = stanSavedColl_pntA;       s->pntB = stanSavedColl_pntB;
    s->someMin = stanSavedColl_someMin; s->posData = stanSavedColl_posData;
    s->flag = D_800413BC;
    s->bprop = g_portBlockProp; s->bsrc = g_portBlockSrc; s->btick = g_portBlockTick; s->bgate = g_portBlockGate;
    s->btop = g_portBlockTop;   s->bbot = g_portBlockBot; s->blo = g_portBlockLo;     s->bhi = g_portBlockHi;
    s->hitProp = D_80030B0C;    s->hitPart = bodypartshot;
    s->hitModel = g_CurrentProjectileModel; s->hitNode = dword_CODE_bss_80075B74;
    s->hitDir = flt_CODE_bss_80075B78;      s->hitDirView = flt_CODE_bss_80075B88;
}

static void scratchRestore(const ArcScratch *s)
{
    stanSavedColl_tile = s->tile;       stanSavedColl_pointI = s->pointI;
    stanSavedColl_pntA = s->pntA;       stanSavedColl_pntB = s->pntB;
    stanSavedColl_someMin = s->someMin; stanSavedColl_posData = s->posData;
    D_800413BC = s->flag;
    g_portBlockProp = s->bprop; g_portBlockSrc = s->bsrc; g_portBlockTick = s->btick; g_portBlockGate = s->bgate;
    g_portBlockTop = s->btop;   g_portBlockBot = s->bbot; g_portBlockLo = s->blo;     g_portBlockHi = s->bhi;
    D_80030B0C = s->hitProp;    bodypartshot = s->hitPart;
    g_CurrentProjectileModel = s->hitModel; dword_CODE_bss_80075B74 = s->hitNode;
    flt_CODE_bss_80075B78 = s->hitDir;      flt_CODE_bss_80075B88 = s->hitDirView;
}

/* What a real round carries between ticks: its rooms (Projectile.unkCC) and
 * the stan tile under it. */
typedef struct ArcWorldCtx {
    u8 rooms[8];
    StandTile *tile;
    coord3d tilePos;
} ArcWorldCtx;

static void roomsCommit(ArcWorldCtx *ctx, const u8 *roomSet)
{
    s32 i = 0;

    if (roomSet[0] != 0xff)
    {
        do
        {
            ctx->rooms[i] = roomSet[i];
            i++;
        }
        while (roomSet[i] != 0xff && i != 7);
    }
    ctx->rooms[i] = 0xff;
}

static s32 inBox1(f32 a, f32 b, f32 v)
{
    return (a <= b) ? (a <= v && v <= b) : (b <= v && v <= a);
}

/* The nearest level triangle on a..b among the rooms listed. */
static s32 levelHit(coord3d *a, coord3d *b, s32 *roomNums, coord3d *pos, coord3d *normal)
{
    f32 best = 0.0f;
    s32 found = FALSE;
    s32 i;

    for (i = 0; roomNums[i] >= 0; i++)
    {
        HitThing h;
        f32 scale;
        f32 d;

        if (!bgTestBulletHitBackground(a, b, roomNums[i], &h)) continue;
        scale = get_room_data_float2();
        h.hitpos.x *= scale;
        h.hitpos.y *= scale;
        h.hitpos.z *= scale;
        if (!inBox1(a->x, b->x, h.hitpos.x) || !inBox1(a->y, b->y, h.hitpos.y) || !inBox1(a->z, b->z, h.hitpos.z)) continue;
        if (h.hitpos.x == a->x && h.hitpos.y == a->y && h.hitpos.z == a->z) continue;
        d = (h.hitpos.x - a->x) * (h.hitpos.x - a->x) + (h.hitpos.y - a->y) * (h.hitpos.y - a->y)
          + (h.hitpos.z - a->z) * (h.hitpos.z - a->z);
        if (found && d >= best) continue;
        best = d;
        found = TRUE;
        *pos = h.hitpos;
        *normal = h.normal;
        if (normal->x != 0.0f || normal->y != 0.0f || normal->z != 0.0f)
        {
            guNormalize(&normal->x, &normal->y, &normal->z);
        }
        else
        {
            normal->x = 0.0f; normal->y = 0.0f; normal->z = 1.0f;
        }
    }
    return found;
}

static s32 arcSegment(void *vctx, const coord3d *from, const coord3d *to, PortArcHit *hit)
{
    ArcWorldCtx *ctx = (ArcWorldCtx *) vctx;
    coord3d a = *from;
    coord3d b = *to;
    coord3d endpos = *to;
    coord3d levelPos, levelNormal, propPos, propNormal;
    s32 roomNums[121];
    u8 roomSet[8];
    s32 roomCount = 0;
    s32 level;

    roomSet[0] = 0xff;
    bgFindRoomsAlongSegment(&a, &b, ctx->rooms, roomSet, roomNums, &roomCount, 20);
    if (roomCount > 20) roomCount = 20;
    roomCount += bgCopyVisibleRoomsToList(&roomNums[roomCount], 100);
    roomNums[roomCount] = -1;

    level = levelHit(&a, &b, roomNums, &levelPos, &levelNormal);
    if (level)
    {   /* the objects test stops just short of the wall, as the game's does */
        f32 len = sqrtf((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z));
        f32 factor = (len > 0.1f) ? 0.1f / len : 0.5f;

        endpos.x = levelPos.x - factor * (b.x - a.x);
        endpos.y = levelPos.y - factor * (b.y - a.y);
        endpos.z = levelPos.z - factor * (b.z - a.z);
    }

    if (projectileFindCollidingProp(NULL, &a, &endpos, CDTYPE_ALL & ~CDTYPE_BG, &propPos, &propNormal, roomNums))
    {
        hit->pos = propPos;
        hit->normal = propNormal;
        hit->prop = TRUE;
        hit->chr = D_80030B0C != NULL && (D_80030B0C->type == PROP_TYPE_CHR || D_80030B0C->type == PROP_TYPE_VIEWER);
        return TRUE;
    }

    if (level)
    {
        roomSet[0] = 0xff;
        roomCount = 0;
        bgFindRoomsAlongSegment(&a, &endpos, ctx->rooms, roomSet, roomNums, &roomCount, 20);
        roomsCommit(ctx, roomSet);
        hit->pos = levelPos;
        hit->normal = levelNormal;
        hit->prop = FALSE;
        hit->chr = FALSE;
        return TRUE;
    }

    roomsCommit(ctx, roomSet);
    return FALSE;
}

static s32 arcFloorBelow(void *vctx, const coord3d *pos, f32 *y)
{
    ArcWorldCtx *ctx = (ArcWorldCtx *) vctx;
    StandTile *tile = ctx->tile;

    if (tile == NULL
        || walkTilesBetweenPoints_NoCallback(&tile, ctx->tilePos.x, ctx->tilePos.z, pos->x, pos->z) != TRUE
        || tile == NULL)
    {
        coord3d p = *pos;

        tile = stanFindTileBelowPos(&p, ctx->rooms, NULL);
        if (tile == NULL)
        {
            ctx->tile = NULL;
            return FALSE;
        }
    }
    ctx->tile = tile;
    ctx->tilePos = *pos;
    *y = stanGetPositionYValue(tile, pos->x, pos->z);
    return TRUE;
}

/* objTryMovePropWithCollision for a settled object sliding from..to: the
 * stan line and the object's width at the end, against walls, objects,
 * doors and characters; blocked, it moves to its width short of the edge it
 * met (getCollisionEdge_maybe) when that much is clear, else stays. */
static s32 arcSlide(void *vctx, const coord3d *from, coord3d *to, f32 width)
{
    const s32 cdtypes = CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;
    ArcWorldCtx *ctx = (ArcWorldCtx *) vctx;
    StandTile *tile;
    coord3d edgeA, edgeB, ray;
    coord2d edgeStart, edgeEnd, dir;
    f32 y, frac, px, pz;

    if (!arcFloorBelow(vctx, from, &y)) return TRUE;   /* nothing under it to stop it */
    tile = ctx->tile;
    stanResetHits();
    if (stanTestLineUnobstructed(&tile, from->x, from->z, to->x, to->z, cdtypes, 0.0f, 1.0f, 0.0f, 1.0f) && tile != NULL
        && stanTestVolume(&tile, to->x, to->z, width, 0x1f, 0.0f, 1.0f) < 0)
    {
        return TRUE;
    }
    getCollisionEdge_maybe(&edgeA, &edgeB);
    ray.x = width;
    ray.y = from->x;
    ray.z = from->z;
    edgeStart.x = edgeA.x;
    edgeStart.y = edgeA.z;
    edgeEnd.x = edgeB.x;
    edgeEnd.y = edgeB.z;
    dir.x = to->x - from->x;
    dir.y = to->z - from->z;
    frac = calculateRayToSegmentIntersectionNormalized(&ray, &edgeStart, &edgeEnd, &dir);
    px = from->x + dir.x * frac * 0.99000001f;
    pz = from->z + dir.y * frac * 0.99000001f;
    tile = ctx->tile;
    if (stanTestLineUnobstructed(&tile, from->x, from->z, px, pz, cdtypes, 0.0f, 1.0f, 0.0f, 1.0f) && tile != NULL
        && stanTestVolume(&tile, px, pz, width, cdtypes, 0.0f, 1.0f) < 0)
    {
        to->x = px;
        to->z = pz;
    }
    else
    {
        to->x = from->x;
        to->z = from->z;
    }
    return FALSE;
}

/* gunInitProjectileFromPlayer: the round enters the world at the muzzle when
 * the player can reach it, and its rooms start along that line. Returns
 * FALSE when the muzzle is behind something (the round would start from
 * the player and hit it straight away). */
static s32 arcWorldStart(const PortArcLaunch *launch, ArcWorldCtx *ctx)
{
    PropRecord *playerprop = getCurrentPlayerProp();
    f32 stanheight = bondviewGetPlayerStanHeight(g_CurrentPlayer);
    StandTile *tile = playerprop->stan;
    coord3d pos = launch->pos;
    f32 yhi = (pos.y < playerprop->pos.y) ? playerprop->pos.y : pos.y;
    f32 ylo = (pos.y < playerprop->pos.y) ? pos.y : playerprop->pos.y;
    u8 rooms[8];
    s32 traversed[0x14];
    s32 count = 0;

    if (tile == NULL) return FALSE;
    if (!stanTestLineUnobstructed(&tile, playerprop->pos.x, playerprop->pos.z, pos.x, pos.z, 0x1f,
                                  yhi - stanheight, ylo - stanheight, 0.0f, 1.0f) || tile == NULL)
    {
        return FALSE;
    }
    ctx->tile = tile;
    ctx->tilePos = pos;
    rooms[0] = (u8) bondviewGetCurrentPlayersRoom();
    rooms[1] = 0xff;
    memset(ctx->rooms, 0xff, sizeof(ctx->rooms));
    bgFindRoomsAlongSegment(bondviewGetCurrentPlayersPosition3(), &pos, rooms, ctx->rooms, traversed, &count, 0x14);
    return TRUE;
}

/* Rounds and thrown items already in the air fly along much the same path
 * as the next one, so the prediction would bounce off them; let it pass
 * through them as through PROPFLAG2_THROWTHROUGH objects. Returns a mask of
 * the g_Projectiles slots changed, for arcShowRounds. */
static u32 arcHideRounds(void)
{
    u32 changed = 0;
    s32 i;

    for (i = 0; i < PROJECTILES_ARR_MAX && i < 32; i++)
    {
        Projectile *p = &g_Projectiles[i];
        ObjectRecord *obj = p->obj;

        if ((p->flags & PROJECTILEFLAG_FREE) || !(p->flags & PROJECTILEFLAG_AIRBORNE) || obj == NULL) continue;
        if (obj->flags2 & PROPFLAG2_THROWTHROUGH) continue;
        obj->flags2 |= PROPFLAG2_THROWTHROUGH;
        changed |= 1u << i;
    }
    return changed;
}

static void arcShowRounds(u32 changed)
{
    s32 i;

    for (i = 0; i < PROJECTILES_ARR_MAX && i < 32; i++)
    {
        if (changed & (1u << i)) g_Projectiles[i].obj->flags2 &= ~PROPFLAG2_THROWTHROUGH;
    }
}

/* Whether the level, an object or a door stands between the eye and p (for
 * the Arc Impact Crosshair's dashed cross). Characters do not count: a guard
 * in front of an impact is no wall, and one the shot hits would block it.
 * The line stops 8 units short of p, off the surface it lands on. */
static s32 arcSightBlocked(const coord3d *eye, const coord3d *p)
{
    coord3d a = *eye;
    coord3d b;
    coord3d pos, normal;
    const coord3d d = { p->x - eye->x, p->y - eye->y, p->z - eye->z };
    const f32 len = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    s32 roomNums[121];
    u8 rooms[2];
    u8 roomSet[8];
    s32 roomCount = 0;

    if (!(len > 16.0f)) return FALSE;
    b.x = p->x - d.x / len * 8.0f;
    b.y = p->y - d.y / len * 8.0f;
    b.z = p->z - d.z / len * 8.0f;
    rooms[0] = (u8) bondviewGetCurrentPlayersRoom();
    rooms[1] = 0xff;
    roomSet[0] = 0xff;
    bgFindRoomsAlongSegment(&a, &b, rooms, roomSet, roomNums, &roomCount, 20);
    if (roomCount > 20) roomCount = 20;
    roomCount += bgCopyVisibleRoomsToList(&roomNums[roomCount], 100);
    roomNums[roomCount] = -1;
    if (levelHit(&a, &b, roomNums, &pos, &normal)) return TRUE;
    return projectileFindCollidingProp(NULL, &a, &b, CDTYPE_OBJS | CDTYPE_DOORS, &pos, &normal, roomNums);
}

/* Predict from the given launch, with the player's own collision off, the
 * rounds in the air ignored, and the collision scratch state kept. */
s32 portProjectileArcPredict(const PortArcLaunch *launch, PortArcPath *path)
{
    PropRecord *playerprop = getCurrentPlayerProp();
    ArcScratch scratch;
    ArcWorldCtx ctx;
    PortArcWorld world;
    u32 hidden;
    s32 ok;

    scratchSave(&scratch);
    sub_GAME_7F03D058(playerprop, 0);
    hidden = arcHideRounds();
    ok = arcWorldStart(launch, &ctx);
    if (ok)
    {
        world.ctx = &ctx;
        world.segment = arcSegment;
        world.floorBelow = arcFloorBelow;
        world.slide = arcSlide;
        if (launch->kind == PORT_ARC_ROCKET) portArcSimulateRocket(launch, &world, path);
        else if (launch->kind >= PORT_ARC_THROWN) portArcSimulateThrown(launch, &world, path);
        else portArcSimulate(launch, &world, path);
        path->ringCentre = path->end;
        path->ringNormal = path->endNormal;
        if (path->endChr && launch->kind != PORT_ARC_KNIFE)
        {   /* a blast on a character: its ring on the floor under them, not
             * standing up through them along the hit's normal */
            f32 floorY;

            if (arcFloorBelow(&ctx, &path->end, &floorY) && floorY <= path->end.y)
            {
                path->ringCentre.y = floorY;
                path->ringNormal = (coord3d) { 0.0f, 1.0f, 0.0f };
            }
        }
        path->endHidden = path->impact && portArcCrosshairEnabled()
                       && arcSightBlocked(bondviewGetCurrentPlayersPosition(), &path->end);
    }
    arcShowRounds(hidden);
    sub_GAME_7F03D058(playerprop, 1);
    scratchRestore(&scratch);
    return ok;
}

/* ---- Each player's prediction -------------------------------------- */

/* Each launch is taken where the game takes a real one: a grenade round's
 * at the end of gunUpdateAndFireBothHands, in the render pass once the gun
 * is posed (gunSpawnGLGrenade); a tank shell's right after that player's
 * propsTick, where chraiCheckUseHeldItems fires it (gunFireTankShell) and
 * the tank's model matrices are current (by the gun pass they are not).
 * The flight is predicted right after propsTick too, where real rounds test
 * theirs: a character's hit list (ChrRecord.field_20) is rebuilt by its tick
 * there and freed again while it draws, and the game's character test does
 * not check for a missing one. */
typedef struct ArcSlot {
    s32 launchValid;
    PortArcLaunch launch;
    s32 pathValid;
    PortArcPath path;
    PortArcLaunch predicted;   /* the launch the path was predicted from */
    s32 reused;                /* ticks the path has been kept since */
} ArcSlot;

/* While the aim holds still the path stays, refreshed every few ticks so a
 * door, guard or vehicle moving into it still shows. */
#define ARC_REUSE_TICKS 10

static s32 arcSameLaunch(const PortArcLaunch *a, const PortArcLaunch *b)
{
    return a->kind == b->kind
        && fabsf(a->pos.x - b->pos.x) < 0.5f && fabsf(a->pos.y - b->pos.y) < 0.5f && fabsf(a->pos.z - b->pos.z) < 0.5f
        && fabsf(a->accel.x - b->accel.x) < 0.001f && fabsf(a->accel.y - b->accel.y) < 0.001f
        && fabsf(a->accel.z - b->accel.z) < 0.001f
        && fabsf(a->vel.x - b->vel.x) < 0.01f && fabsf(a->vel.y - b->vel.y) < 0.01f && fabsf(a->vel.z - b->vel.z) < 0.01f
        && a->maxFields == b->maxFields    /* a hand grenade's fuse, while the trigger cooks it */
        && a->slideFriction == b->slideFriction
        && a->tickStep == b->tickStep
        && a->turnFields == b->turnFields;
}

static ArcSlot sArc[4][PORT_ARC_SLOTS];

static ArcSlot *arcSlot(s32 slot)
{
    s32 p = get_cur_playernum();

    return &sArc[(p >= 0 && p < 4) ? p : 0][(slot >= 0 && slot < PORT_ARC_SLOTS) ? slot : 0];
}

static void arcTakeLaunch(s32 slot)
{
    ArcSlot *st = arcSlot(slot);

    st->launchValid = portProjectileArcWanted() && portProjectileArcLaunchNow(slot, &st->launch);
    if (!st->launchValid) st->pathValid = FALSE;
}

/* gunUpdateAndFireBothHands, once the gun is posed and any round fired. */
void portProjectileArcNoteLaunch(void)
{
    s32 slot;

    if (portTankTurretWeaponHeld()) return;
    for (slot = 0; slot < PORT_ARC_SLOTS; slot++) arcTakeLaunch(slot);
}

static void arcPredictSlot(ArcSlot *st);

/* lv.c, for each player right after propsTick, where real rounds fly. */
void portProjectileArcTick(void)
{
    s32 slot;

    portProjectileArcLogTick();
    portProjectileArcNoteTickStep();
    if (portTankTurretWeaponHeld())
    {   /* the turret is slot 0; there is no second one */
        arcTakeLaunch(0);
        arcSlot(1)->launchValid = FALSE;
    }
    for (slot = 0; slot < PORT_ARC_SLOTS; slot++) arcPredictSlot(arcSlot(slot));
}

static void arcPredictSlot(ArcSlot *st)
{
    if (!portProjectileArcWanted() || !st->launchValid || !portProjectileArcPlayerReady())
    {
        st->pathValid = FALSE;
        return;
    }
    if (st->pathValid && st->reused < ARC_REUSE_TICKS && arcSameLaunch(&st->launch, &st->predicted))
    {
        st->reused++;
        return;
    }
    st->predicted = st->launch;
    st->reused = 0;
    if (portProjectileArcDebug())
    {   /* GE_ARCDBG: what a prediction costs, every 240 */
        static u64 total, worst;
        static s32 count, hiddenCount;
        u64 start = osGetTime();
        u64 spent;

        st->pathValid = portProjectileArcPredict(&st->launch, &st->path) && (st->path.legCount > 0 || st->path.pointCount > 1);
        spent = osGetTime() - start;
        hiddenCount += st->pathValid && st->path.endHidden;
        total += spent;
        if (spent > worst) worst = spent;
        if (++count == 240)
        {
            printf("arcdbg: prediction cost mean=%.3f ms worst=%.3f ms over %d, impact out of sight in %d\n",
                   (double) total / (count * 46875.0), (double) worst / 46875.0, count, hiddenCount);
            fflush(stdout);
            total = worst = 0;
            count = hiddenCount = 0;
        }
        return;
    }
    st->pathValid = portProjectileArcPredict(&st->launch, &st->path) && (st->path.legCount > 0 || st->path.pointCount > 1);
}

const PortArcPath *portProjectileArcLast(s32 slot, PortArcLaunch *launch)
{
    ArcSlot *st = arcSlot(slot);

    if (!st->launchValid || !st->pathValid) return NULL;
    if (launch != NULL) *launch = st->launch;
    return &st->path;
}
