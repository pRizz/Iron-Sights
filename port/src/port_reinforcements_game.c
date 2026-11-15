/* Reinforcements (FORK-081): the game hooks. The setting is in
 * port_reinforcements.c and the rules in port_reinforcements.h.
 *
 * A side table per character slot remembers which guards the level placed,
 * which characters are clones and whose, and when each guard last called
 * one. Slots are cleared when the level loads and when a character's slot is
 * freed (chrpropCleanupForRemoval). GE_REINFORCEMENTS_LOG=1 prints each
 * stage's level, every placed guard and every clone; the director
 * (port_reinforcements_director.c) logs its own guards. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondaicommands.h>
#include <bondtypes.h>
#include "game/chr.h"
#include "game/chraction.h"
#include "game/chrai.h"
#include "port_reinforcements.h"
#include "port_reinforcements_director.h"

extern s32 get_is_ramrom_flag(void);
extern PropRecord *getCurrentPlayerProp(void);
extern s32 g_GlobalTimer;
extern s32 chrGetNumFree(void); /* chr.c */

static s32 stageLevel = PORT_REINFORCEMENTS_ORIGINAL;
static PortReinforcementRules stageRules;
static u8 slotPlaced[PORT_REINFORCEMENTS_MAX_SLOTS];
static u8 slotClone[PORT_REINFORCEMENTS_MAX_SLOTS];
static s16 slotParent[PORT_REINFORCEMENTS_MAX_SLOTS];
static s32 slotLastClone[PORT_REINFORCEMENTS_MAX_SLOTS];

/* The clone command in progress (CloneBegin to CloneEnd). */
static ChrRecord *cloneSource;
static s32 cloneExtra;
static s32 cloneBesideSeen;
/* The director's spawn in progress (portReinforcementsSetDirectorSpawn). */
static s32 directorSpawn;

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

static void clearSlot(s32 i)
{
    slotPlaced[i] = FALSE;
    slotClone[i] = FALSE;
    slotParent[i] = -1;
    slotLastClone[i] = -1;
}

static s32 slotOf(ChrRecord *chr)
{
    s32 i;
    if (chr == NULL || g_ChrSlots == NULL) return -1;
    i = (s32)(chr - g_ChrSlots);
    return (i >= 0 && i < g_NumChrSlots && i < PORT_REINFORCEMENTS_MAX_SLOTS) ? i : -1;
}

static s32 inTryCloneList(void *ailist)
{
    static void *tryClone;
    if (tryClone == NULL) tryClone = ailistFindById(GAILIST_TRY_CLONE_SEND_OR_RUN_TO_BOND);
    return ailist != NULL && ailist == tryClone;
}

static s32 livingClones(s32 source)
{
    s32 i, count = 0;
    for (i = 0; i < g_NumChrSlots && i < PORT_REINFORCEMENTS_MAX_SLOTS; i++)
    {
        if (slotParent[i] == source && g_ChrSlots[i].model != NULL && !chrIsDead(&g_ChrSlots[i])) count++;
    }
    return count;
}

static s32 eligible(ChrRecord *chr, s32 slot)
{
    PortReinforcementGuard guard;
    if (slot < 0) return FALSE;
    guard.placed = slotPlaced[slot];
    guard.isClone = slotClone[slot];
    guard.cloneFlag = (chr->chrflags & CHRFLAG_CLONE) != 0;
    guard.invincible = (chr->chrflags & CHRFLAG_INVINCIBLE) != 0;
    guard.soldierBody = portReinforcementsSoldierBody(chr->bodynum);
    return portReinforcementsEligible(&stageRules, &guard);
}

static s32 cloneDue(s32 slot)
{
    return portReinforcementsCloneDue(&stageRules, livingClones(slot), slotLastClone[slot], g_GlobalTimer);
}

/* Floor-plane distance to the nearest other live character (log only). */
static f32 nearestChr(ChrRecord *chr)
{
    f32 best = 1.0e9f;
    s32 i;
    for (i = 0; i < g_NumChrSlots; i++)
    {
        ChrRecord *other = &g_ChrSlots[i];
        f32 dx, dz;
        if (other == chr || other->model == NULL || other->prop == NULL || chr->prop == NULL) continue;
        dx = other->prop->pos.f[0] - chr->prop->pos.f[0];
        dz = other->prop->pos.f[2] - chr->prop->pos.f[2];
        if (sqrtf(dx * dx + dz * dz) < best) best = sqrtf(dx * dx + dz * dz);
    }
    return best;
}

static s32 offScreen(ChrRecord *chr)
{
    return chr->prop != NULL && !(chr->prop->flags & PROPFLAG_ONSCREEN);
}

s32 portReinforcementsBeginStage(s32 placedGuards)
{
    s32 i;
    stageLevel = portReinforcementsStageLevel(portReinforcementsSetting(), placedGuards, get_is_ramrom_flag() != 0);
    stageRules = portReinforcementRulesFor(stageLevel);
    for (i = 0; i < PORT_REINFORCEMENTS_MAX_SLOTS; i++) clearSlot(i);
    cloneSource = NULL;
    directorSpawn = FALSE;
    portReinforcementsDirectorReset();
    if (logEnabled() && placedGuards > 0)
    {
        fprintf(stderr, "[reinforcements] stage level %d: %d placed guards, %d extra slots\n",
            stageLevel, placedGuards, stageRules.extraChrs);
    }
    return stageRules.extraChrs;
}

s32 portReinforcementsStageExtraChrs(void)
{
    return stageRules.extraChrs;
}

void portReinforcementsMarkPlaced(ChrRecord *chr)
{
    const s32 slot = slotOf(chr);
    if (slot < 0) return;
    slotPlaced[slot] = TRUE;
    if (logEnabled() && chr->prop != NULL)
    {
        fprintf(stderr, "[reinforcements] placed chr %d slot %d body %d at (%.0f, %.0f, %.0f)%s%s\n",
            chr->chrnum, slot, chr->bodynum, chr->prop->pos.f[0], chr->prop->pos.f[1], chr->prop->pos.f[2],
            (chr->chrflags & CHRFLAG_CLONE) ? ", clone flag" : "",
            eligible(chr, slot) ? ", may clone" : "");
    }
}

void portReinforcementsChrFreed(ChrRecord *chr)
{
    const s32 slot = slotOf(chr);
    s32 i;
    if (slot < 0) return;
    clearSlot(slot);
    portReinforcementsDirectorChrFreed(slot);
    for (i = 0; i < PORT_REINFORCEMENTS_MAX_SLOTS; i++)
    {
        if (slotParent[i] == slot) slotParent[i] = -1;
    }
}

/* IFMyCloneDoesNotExist inside Rare's clone list: for a guard these rules
 * cover, "no clone" means another one may come now. Returns FALSE to leave
 * the command to Rare's check. */
s32 portReinforcementsCloneCheck(ChrRecord *self, void *ailist, s32 *slotOpen)
{
    const s32 slot = slotOf(self);
    if (stageLevel == PORT_REINFORCEMENTS_ORIGINAL || !inTryCloneList(ailist) || !eligible(self, slot)) return FALSE;
    *slotOpen = cloneDue(slot);
    return TRUE;
}

/* IFIveNotBeenSeen inside Rare's clone list, for a guard Bond has seen:
 * Relentless lets it call a clone while it is off screen. */
s32 portReinforcementsTreatAsUnseen(ChrRecord *self, void *ailist)
{
    const s32 slot = slotOf(self);
    if (!stageRules.afterSeen || !inTryCloneList(ailist) || !eligible(self, slot)) return FALSE;
    return offScreen(self) && cloneDue(slot);
}

/* The clone command: a guard without Rare's clone flag may clone too. */
s32 portReinforcementsMayClone(ChrRecord *chr)
{
    const s32 slot = slotOf(chr);
    if (stageLevel == PORT_REINFORCEMENTS_ORIGINAL || !eligible(chr, slot)) return FALSE;
    return cloneDue(slot);
}

void portReinforcementsCloneBegin(ChrRecord *chr)
{
    ChrRecord *rareClone;
    s32 rareWould;
    cloneSource = chr;
    cloneExtra = FALSE;
    cloneBesideSeen = FALSE;
    if (stageLevel == PORT_REINFORCEMENTS_ORIGINAL) return;
    /* The clone Rare makes: flagged, unseen, and its one clone gone. */
    rareClone = chrFindById(chr, chr->chrnum + CHR_CLONED_NUM_START);
    rareWould = (chr->chrflags & CHRFLAG_CLONE) && !(chr->chrflags & CHRFLAG_HAS_BEEN_ON_SCREEN)
        && (rareClone == NULL || rareClone->model == NULL);
    cloneExtra = !rareWould;
    cloneBesideSeen = stageRules.afterSeen && (chr->chrflags & CHRFLAG_HAS_BEEN_ON_SCREEN) && offScreen(chr);
}

void portReinforcementsCloneEnd(ChrRecord *chr, void *cloneprop)
{
    const s32 source = slotOf(chr);
    PropRecord *prop = (PropRecord *)cloneprop;
    if (prop != NULL && prop->chr != NULL)
    {
        const s32 clone = slotOf(prop->chr);
        if (clone >= 0)
        {
            slotClone[clone] = TRUE;
            slotParent[clone] = (s16)source;
        }
        if (source >= 0) slotLastClone[source] = g_GlobalTimer;
        if (logEnabled())
        {
            fprintf(stderr, "[reinforcements] t=%d chr %d (body %d) cloned into slot %d%s%s; %d living clones, %d free slots, nearest chr %.0f\n",
                g_GlobalTimer, chr->chrnum, chr->bodynum, clone, cloneExtra ? ", extra" : "", cloneBesideSeen ? ", after seen" : "",
                source >= 0 ? livingClones(source) : 0, chrGetNumFree(), nearestChr(prop->chr));
        }
    }
    cloneSource = NULL;
    cloneExtra = FALSE;
    cloneBesideSeen = FALSE;
}

/* chrSpawnAtChr: a Relentless clone of a seen, off-screen guard. */
s32 portReinforcementsSpawnBesideSeen(ChrRecord *chr)
{
    return cloneSource != NULL && cloneSource == chr && cloneBesideSeen;
}

/* chrSpawnAtCoord's free-slot test. */
s32 portReinforcementsFreeChrsForSpawn(void)
{
    return portReinforcementsFreeChrsNeeded(directorSpawn || (cloneSource != NULL && cloneExtra));
}

/* chrSpawnAtCoord's added test, for this feature's spawns only: no character,
 * living or fallen, within 60 units, and for the director no Bond within 300.
 * Rare's chrAdjustPosForSpawn checks a 20-unit circle against standing
 * characters, so a clone could land inside a dying guard or brush another. */
s32 portReinforcementsSpawnClear(struct coord3d *pos)
{
    PropRecord *bond;
    s32 i;
    if (!directorSpawn && (cloneSource == NULL || stageLevel == PORT_REINFORCEMENTS_ORIGINAL)) return TRUE;
    for (i = 0; i < g_NumChrSlots; i++)
    {
        ChrRecord *other = &g_ChrSlots[i];
        if (other->model == NULL || other->prop == NULL) continue;
        if (portSpawnTooClose(other->prop->pos.f[0] - pos->f[0], other->prop->pos.f[1] - pos->f[1],
                other->prop->pos.f[2] - pos->f[2], PORT_SPAWN_CLEARANCE)) return FALSE;
    }
    bond = directorSpawn ? getCurrentPlayerProp() : NULL;
    return bond == NULL || !portSpawnTooClose(bond->pos.f[0] - pos->f[0], bond->pos.f[1] - pos->f[1],
        bond->pos.f[2] - pos->f[2], PORT_SPAWN_BOND_CLEARANCE);
}

PortReinforcementRules portReinforcementsStageRules(void)
{
    return stageRules;
}

/* A placed soldier the director may dress its guards as. */
s32 portReinforcementsSlotIsSource(s32 slot)
{
    if (slot < 0 || slot >= g_NumChrSlots || slot >= PORT_REINFORCEMENTS_MAX_SLOTS) return FALSE;
    return slotPlaced[slot] && portReinforcementsSoldierBody(g_ChrSlots[slot].bodynum)
        && !(g_ChrSlots[slot].chrflags & CHRFLAG_INVINCIBLE);
}

/* A director guard never clones (it is not placed, and is marked a clone). */
void portReinforcementsMarkDirectorGuard(ChrRecord *chr)
{
    const s32 slot = slotOf(chr);
    if (slot >= 0) slotClone[slot] = TRUE;
}

void portReinforcementsSetDirectorSpawn(s32 on)
{
    directorSpawn = on;
}
