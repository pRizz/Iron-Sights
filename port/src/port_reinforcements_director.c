/* Reinforcements (FORK-081): the director. Rules and thresholds are in
 * port_reinforcements_director.h; the level settings in port_reinforcements.h.
 * GE_REINFORCEMENTS_LOG=1 logs each guard it sends: pad, route steps, distance
 * from Bond and the outfit it copied. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondaicommands.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/chr.h"
#include "game/chraction.h"
#include "game/chrai.h"
#include "game/math_atan2f.h"   /* include/math.h leaves atan2f undeclared */
#include "game/objecthandler.h"
#include "game/player.h"
#include "game/propobj.h"
#include "port_reinforcements.h"
#include "port_reinforcements_director.h"

extern s32 g_GlobalTimer;
extern s32 chrGetNumFree(void);
extern s32 chrIsPosOffScreen(coord3d *pos, StandTile *tile);
extern PropRecord *chrSpawnAtCoord(s32 bodynum, s32 headnum, coord3d *pos, StandTile *stan, f32 angle,
    AIListRecord *ailist, s32 spawnflags);
extern PropRecord *hatCreateForChr(ChrRecord *chr, s32 modelnum, u32 flags);
extern s32 waypointFindRoute(waypoint *from, waypoint *to, waypoint **arr, s32 arrlen);
extern u32 randomGetNext(void);

#define ROUTE_MAX 32
#define CANDIDATE_MAX 96

/* What a placed soldier wore: copied while it is alive, kept once it falls. */
typedef struct SourceGear
{
    s8 seen;
    s16 body;
    s16 modelR, modelL;   /* prop model numbers, -1 for none */
    s16 itemR, itemL;
    s8 dual;
    s16 hat;              /* -1 for none */
    coord3d placedAt;
} SourceGear;

static SourceGear sourceGear[PORT_REINFORCEMENTS_MAX_SLOTS];
static u8 slotDirector[PORT_REINFORCEMENTS_MAX_SLOTS];
static s32 lastNoticed = -1, lastSpawn = -1, lastTry = -1;

static s32 logEnabled(void)
{
    static s32 cached = -1;
    if (cached < 0)
    {
        const char *env = getenv("GE_REINFORCEMENTS_LOG");
        cached = env != NULL && env[0] == '1';
    }
    return cached;
}

void portReinforcementsDirectorReset(void)
{
    s32 i;
    for (i = 0; i < PORT_REINFORCEMENTS_MAX_SLOTS; i++)
    {
        sourceGear[i].seen = FALSE;
        slotDirector[i] = FALSE;
    }
    lastNoticed = lastSpawn = lastTry = -1;
}

void portReinforcementsDirectorChrFreed(s32 slot)
{
    if (slot >= 0 && slot < PORT_REINFORCEMENTS_MAX_SLOTS) slotDirector[slot] = FALSE;
}

static s32 slotCount(void)
{
    return g_NumChrSlots < PORT_REINFORCEMENTS_MAX_SLOTS ? g_NumChrSlots : PORT_REINFORCEMENTS_MAX_SLOTS;
}

static void noteGear(s32 slot, ChrRecord *chr)
{
    SourceGear *gear = &sourceGear[slot];
    PropRecord *right = chrGetEquippedWeaponProp(chr, GUNRIGHT);
    PropRecord *left = chrGetEquippedWeaponProp(chr, GUNLEFT);
    PropRecord *hat = chr->handle_positiondata_hat;
    if (!gear->seen) gear->placedAt = chr->prop->pos;
    gear->seen = TRUE;
    gear->body = chr->bodynum;
    gear->modelR = right ? right->weapon->obj : -1;
    gear->itemR = right ? right->weapon->weaponnum : -1;
    gear->modelL = left ? left->weapon->obj : -1;
    gear->itemL = left ? left->weapon->weaponnum : -1;
    gear->dual = right && left && right->weapon->dualweapon == left->weapon && left->weapon->dualweapon == right->weapon;
    gear->hat = hat ? hat->obj->obj : -1;
}

/* One pass over the guards: copy living soldiers' outfits, count the
 * director's guards and whether any guard has seen Bond recently. */
static s32 scanGuards(s32 *living)
{
    s32 i, noticed = FALSE;
    *living = 0;
    for (i = 0; i < slotCount(); i++)
    {
        ChrRecord *chr = &g_ChrSlots[i];
        if (chr->model == NULL || chr->prop == NULL || chrIsDead(chr)) continue;
        if (slotDirector[i]) (*living)++;
        if (chrSawTargetRecently(chr)) noticed = TRUE;
        if (portReinforcementsSlotIsSource(i) && chrGetEquippedWeaponProp(chr, GUNRIGHT) != NULL) noteGear(i, chr);
    }
    return noticed;
}

static f32 flatDistSq(coord3d *a, coord3d *b)
{
    const f32 dx = a->f[0] - b->f[0], dz = a->f[2] - b->f[2];
    return dx * dx + dz * dz;
}

/* Bond's waypoint: nearest across the floor, heavily penalising other floors. */
static waypoint *nearestWaypoint(coord3d *pos)
{
    waypoint *node = g_CurrentSetup.pathwaypoints, *best = NULL;
    f32 bestScore = 0.0f;
    if (node == NULL || g_CurrentSetup.pads == NULL) return NULL;
    for (; node->padID >= 0; node++)
    {
        PadRecord *pad = &g_CurrentSetup.pads[node->padID];
        const f32 dy = pad->pos.f[1] - pos->f[1];
        const f32 score = flatDistSq(&pad->pos, pos) + dy * dy * 16.0f;
        if (best == NULL || score < bestScore)
        {
            best = node;
            bestScore = score;
        }
    }
    return best;
}

static s32 padClear(PadRecord *pad, coord3d *bond)
{
    s32 i;
    if (portSpawnTooClose(bond->f[0] - pad->pos.f[0], bond->f[1] - pad->pos.f[1], bond->f[2] - pad->pos.f[2],
            PORT_SPAWN_BOND_CLEARANCE)) return FALSE;
    for (i = 0; i < g_NumChrSlots; i++)
    {
        ChrRecord *chr = &g_ChrSlots[i];
        if (chr->model == NULL || chr->prop == NULL) continue;
        if (portSpawnTooClose(chr->prop->pos.f[0] - pad->pos.f[0], chr->prop->pos.f[1] - pad->pos.f[1],
                chr->prop->pos.f[2] - pad->pos.f[2], PORT_SPAWN_CLEARANCE)) return FALSE;
    }
    return TRUE;
}

/* The best pad for the next guard, or NULL; *hopsOut gets its route steps. */
static waypoint *choosePad(coord3d *bond, s32 *hopsOut)
{
    waypoint *candidates[CANDIDATE_MAX];
    waypoint *route[ROUTE_MAX];
    waypoint *target = nearestWaypoint(bond), *node, *best = NULL;
    s32 count = 0, tried = 0, bestHops = 0, i;
    if (target == NULL) return NULL;
    for (node = g_CurrentSetup.pathwaypoints; node->padID >= 0 && count < CANDIDATE_MAX; node++)
    {
        PadRecord *pad = &g_CurrentSetup.pads[node->padID];
        if (pad->stan == NULL) continue;
        if (portDirectorPadInRange(pad->pos.f[0] - bond->f[0], pad->pos.f[1] - bond->f[1], pad->pos.f[2] - bond->f[2]))
        {
            candidates[count++] = node;
        }
    }
    for (i = count - 1; i > 0; i--)
    {
        const s32 j = (s32)(randomGetNext() % (u32)(i + 1));
        node = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = node;
    }
    for (i = 0; i < count && tried < PORT_DIRECTOR_CANDIDATES; i++)
    {
        PadRecord *pad = &g_CurrentSetup.pads[candidates[i]->padID];
        s32 hops;
        if (!chrIsPosOffScreen(&pad->pos, pad->stan) || !padClear(pad, bond)) continue;
        tried++;
        hops = waypointFindRoute(candidates[i], target, route, ROUTE_MAX) - 1;
        if (!portDirectorRouteInRange(hops)) continue;
        if (best == NULL || hops < bestHops)
        {
            best = candidates[i];
            bestHops = hops;
        }
    }
    *hopsOut = bestHops;
    return best;
}

static SourceGear *nearestGear(coord3d *pos)
{
    SourceGear *best = NULL;
    f32 bestDist = 0.0f;
    s32 i;
    for (i = 0; i < PORT_REINFORCEMENTS_MAX_SLOTS; i++)
    {
        SourceGear *gear = &sourceGear[i];
        if (!gear->seen) continue;
        if (best == NULL || flatDistSq(&gear->placedAt, pos) < bestDist)
        {
            best = gear;
            bestDist = flatDistSq(&gear->placedAt, pos);
        }
    }
    return best;
}

static void giveGear(ChrRecord *chr, SourceGear *gear)
{
    PropRecord *right = NULL, *left = NULL;
    if (gear->modelR >= 0) right = chrGiveWeapon(chr, gear->modelR, gear->itemR, 0);
    if (gear->modelL >= 0) left = chrGiveWeapon(chr, gear->modelL, gear->itemL, 0x10000000);
    if (gear->dual && right && left) propweaponSetDual(left->weapon, right->weapon);
    if (gear->hat >= 0) hatCreateForChr(chr, gear->hat, 0);
}

static void sendGuard(coord3d *bond)
{
    s32 hops = 0;
    waypoint *point = choosePad(bond, &hops);
    PadRecord *pad;
    SourceGear *gear;
    PropRecord *prop;
    f32 angle;
    if (point == NULL) return;
    pad = &g_CurrentSetup.pads[point->padID];
    gear = nearestGear(&pad->pos);
    if (gear == NULL) return;
    /* facing Bond, in atan2's -pi..pi as chrSpawnAtPad gives a pad's look */
    angle = atan2f(bond->f[0] - pad->pos.f[0], bond->f[2] - pad->pos.f[2]);
    portReinforcementsSetDirectorSpawn(TRUE);
    prop = chrSpawnAtCoord(gear->body, -1, &pad->pos, pad->stan, angle,
        ailistFindById(GAILIST_STANDARD_CLONE), 0);
    portReinforcementsSetDirectorSpawn(FALSE);
    if (prop == NULL || prop->chr == NULL) return;
    giveGear(prop->chr, gear);
    portReinforcementsMarkDirectorGuard(prop->chr);
    {
        const s32 slot = (s32)(prop->chr - g_ChrSlots);
        if (slot >= 0 && slot < PORT_REINFORCEMENTS_MAX_SLOTS) slotDirector[slot] = TRUE;
        if (logEnabled())
        {
            fprintf(stderr, "[reinforcements] t=%d director sent slot %d (body %d, gun %d) from pad %d: %d route steps, %.0f from Bond, %d free slots\n",
                g_GlobalTimer, slot, gear->body, gear->itemR, point->padID, hops,
                sqrtf(flatDistSq(&prop->pos, bond)), chrGetNumFree());
        }
    }
    lastSpawn = g_GlobalTimer;
}

void portReinforcementsDirectorTick(void)
{
    const PortReinforcementRules rules = portReinforcementsStageRules();
    PropRecord *bond;
    s32 living;
    if (rules.directorTicks <= 0 || g_ChrSlots == NULL || g_CurrentPlayer == NULL || g_CurrentPlayer->bonddead) return;
    bond = getCurrentPlayerProp();
    if (bond == NULL) return;
    if (scanGuards(&living) || alarmIsActive()) lastNoticed = g_GlobalTimer;
    if (!portDirectorDue(&rules, portDirectorActive(lastNoticed, g_GlobalTimer), living, lastSpawn,
            g_GlobalTimer, chrGetNumFree())) return;
    if (lastTry >= 0 && g_GlobalTimer - lastTry < PORT_DIRECTOR_TRY_TICKS) return;
    lastTry = g_GlobalTimer;
    sendGuard(&bond->pos);
}
