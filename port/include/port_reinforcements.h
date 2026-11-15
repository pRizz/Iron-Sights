/* Reinforcements (FORK-081): how many guards the level sends after Bond.
 *
 * Rare's only general reinforcement is the clone: a guard running the
 * standard guard AI (chraidata.c m_TryCloneSendOrRunToBond) that hears Bond
 * before Bond has seen it spawns one copy of itself beside it, which runs to
 * Bond while the original holds its post. Only guards whose setup sets the
 * clone flag do it (Archives, Runway, Surface 1, Bunker 1 and Bunker 2), and
 * each one keeps a single clone. Every spawn needs 3 free character slots,
 * and the pool holds the level's placed guards plus 10.
 *
 * Original keeps all of that. More and Relentless:
 * - let any placed guard with a soldier's body clone too (never named
 *   characters, civilians, scientists, invincible characters or clones);
 * - keep up to 2 (More) or 3 (Relentless) living clones per guard, the next
 *   one at most every 8 or 4 seconds while gunfire is still heard;
 * - add 12 or 24 character slots, with matching model slots, at stage load.
 *   A clone Rare would not have made only spawns while 10 slots stay free,
 *   so level scripts keep the room they always had for their own spawns.
 * Relentless also lets a guard Bond has already seen call one more clone
 * while it is off screen, so the clone never pops into view.
 *
 * Clones need a guard that hears Bond before Bond sees it, which open levels
 * such as Dam rarely give. More and Relentless therefore also run a director
 * (port_reinforcements_director.c): once a guard has seen Bond or the alarm is
 * on, it sends a guard every 20 or 10 seconds, up to 4 or 8 of its own alive,
 * from a waypoint pad Bond cannot see that has a route to him.
 *
 * Every added spawn, clone or director guard, keeps 60 units clear of other
 * characters (living or fallen), which Rare's spawn test alone does not.
 *
 * The level is read when a stage loads, so a change applies from the next
 * mission start. Demo playback and multiplayer keep Original. */
#ifndef PORT_REINFORCEMENTS_H
#define PORT_REINFORCEMENTS_H

#include <ultra64.h>
#include <bondconstants.h>

#define PORT_REINFORCEMENTS_ORIGINAL 0
#define PORT_REINFORCEMENTS_MORE 1
#define PORT_REINFORCEMENTS_RELENTLESS 2
#define PORT_REINFORCEMENTS_DEFAULT PORT_REINFORCEMENTS_ORIGINAL

/* Rare's pool margin and spawn headroom (initguards.c, chrSpawnAtCoord). */
#define PORT_REINFORCEMENTS_UPSTREAM_SPARE_CHRS 10
#define PORT_REINFORCEMENTS_UPSTREAM_FREE_CHRS 3
/* g_GlobalTimer ticks per second */
#define PORT_REINFORCEMENTS_TICKS 60
/* Side-table size; a slot past it is never a reinforcement source. */
#define PORT_REINFORCEMENTS_MAX_SLOTS 256

typedef struct PortReinforcementRules
{
    s32 extraChrs;      /* character slots added at stage load */
    s32 clonesPerGuard; /* living clones one guard may have (0: Rare's rule) */
    s32 cooldownTicks;  /* least time between one guard's clones */
    s32 afterSeen;      /* a seen guard off screen may still call one */
    s32 directorTicks;  /* director: least time between its guards (0: off) */
    s32 directorCap;    /* director: most of its guards alive at once */
} PortReinforcementRules;

static inline s32 portReinforcementsClamp(s32 level)
{
    if (level < PORT_REINFORCEMENTS_ORIGINAL) return PORT_REINFORCEMENTS_ORIGINAL;
    return level > PORT_REINFORCEMENTS_RELENTLESS ? PORT_REINFORCEMENTS_RELENTLESS : level;
}

static inline PortReinforcementRules portReinforcementRulesFor(s32 level)
{
    PortReinforcementRules rules = { 0, 0, 0, 0, 0, 0 };
    switch (portReinforcementsClamp(level))
    {
        case PORT_REINFORCEMENTS_MORE:
            rules.extraChrs = 12;
            rules.clonesPerGuard = 2;
            rules.cooldownTicks = 8 * PORT_REINFORCEMENTS_TICKS;
            rules.directorTicks = 20 * PORT_REINFORCEMENTS_TICKS;
            rules.directorCap = 4;
            break;
        case PORT_REINFORCEMENTS_RELENTLESS:
            rules.extraChrs = 24;
            rules.clonesPerGuard = 3;
            rules.cooldownTicks = 4 * PORT_REINFORCEMENTS_TICKS;
            rules.afterSeen = 1;
            rules.directorTicks = 10 * PORT_REINFORCEMENTS_TICKS;
            rules.directorCap = 8;
            break;
        default:
            break;
    }
    return rules;
}

/* The level a stage plays at: Original for multiplayer (no placed guards)
 * and demo playback, whose recorded input expects Rare's guards. */
static inline s32 portReinforcementsStageLevel(s32 setting, s32 placedGuards, s32 demo)
{
    return (placedGuards <= 0 || demo) ? PORT_REINFORCEMENTS_ORIGINAL : portReinforcementsClamp(setting);
}

/* Spare model slots for the added characters: one animated model each, and
 * plain models for up to two guns. */
static inline s32 portReinforcementsExtraAnimModels(s32 extraChrs) { return extraChrs; }
static inline s32 portReinforcementsExtraModels(s32 extraChrs) { return extraChrs * 2; }

/* Bodies a soldier wears: a clone of one is just another guard. Named
 * characters, Bond's outfits, civilians and scientists are left out. */
static inline s32 portReinforcementsSoldierBody(s32 body)
{
    switch (body)
    {
        case BODY_Jungle_Commando:
        case BODY_St_Petersburg_Guard:
        case BODY_Russian_Soldier:
        case BODY_Russian_Infantry:
        case BODY_Janus_Special_Forces:
        case BODY_Janus_Marine:
        case BODY_Russian_Commandant:
        case BODY_Siberian_Guard_1_Mishkin:
        case BODY_Naval_Officer:
        case BODY_Siberian_Special_Forces:
        case BODY_Helicopter_Pilot:
        case BODY_Siberian_Guard_2:
        case BODY_Arctic_Commando:
        case BODY_Moonraker_Elite_1_Male:
        case BODY_Moonraker_Elite_2_Female:
            return TRUE;
        default:
            return FALSE;
    }
}

/* What a guard is to the reinforcement rules. */
typedef struct PortReinforcementGuard
{
    s32 placed;        /* set up by the level, not spawned */
    s32 isClone;       /* made by a clone command */
    s32 cloneFlag;     /* the level's own clone flag (CHRFLAG_CLONE) */
    s32 invincible;
    s32 soldierBody;
} PortReinforcementGuard;

static inline s32 portReinforcementsEligible(const PortReinforcementRules *rules,
    const PortReinforcementGuard *guard)
{
    if (rules->clonesPerGuard <= 0 || !guard->placed || guard->isClone || guard->invincible) return FALSE;
    return guard->cloneFlag || guard->soldierBody;
}

/* May this guard call another clone now? Both sides of the cooldown are
 * g_GlobalTimer ticks; lastClone < 0 means it has not called one yet. */
static inline s32 portReinforcementsCloneDue(const PortReinforcementRules *rules,
    s32 livingClones, s32 lastClone, s32 now)
{
    if (livingClones >= rules->clonesPerGuard) return FALSE;
    return lastClone < 0 || now - lastClone >= rules->cooldownTicks;
}

/* Free character slots a spawn needs: Rare's 3, or for a clone Rare would
 * not have made, the 10 that Rare's pool kept spare for the level. */
static inline s32 portReinforcementsFreeChrsNeeded(s32 extraClone)
{
    return extraClone ? PORT_REINFORCEMENTS_UPSTREAM_SPARE_CHRS : PORT_REINFORCEMENTS_UPSTREAM_FREE_CHRS;
}

/* Settings (port_reinforcements.c). */
s32 portReinforcementsOptionGet(const char *key, s32 *value);
s32 portReinforcementsOptionSet(const char *key, s32 value);
s32 portReinforcementsSetting(void);

/* Game hooks (port_reinforcements_game.c). */
struct ChrRecord;
s32 portReinforcementsBeginStage(s32 placedGuards);   /* returns the slots to add */
s32 portReinforcementsStageExtraChrs(void);
void portReinforcementsMarkPlaced(struct ChrRecord *chr);
void portReinforcementsChrFreed(struct ChrRecord *chr);
s32 portReinforcementsCloneCheck(struct ChrRecord *self, void *ailist, s32 *slotOpen);
s32 portReinforcementsTreatAsUnseen(struct ChrRecord *self, void *ailist);
s32 portReinforcementsMayClone(struct ChrRecord *chr);
void portReinforcementsCloneBegin(struct ChrRecord *chr);
void portReinforcementsCloneEnd(struct ChrRecord *chr, void *cloneprop);
s32 portReinforcementsSpawnBesideSeen(struct ChrRecord *chr);
s32 portReinforcementsFreeChrsForSpawn(void);
struct coord3d;
s32 portReinforcementsSpawnClear(struct coord3d *pos);   /* chrSpawnAtCoord's added test */
/* For the director (port_reinforcements_director.c). */
PortReinforcementRules portReinforcementsStageRules(void);
s32 portReinforcementsSlotIsSource(s32 slot);   /* a placed guard whose outfit it may copy */
void portReinforcementsMarkDirectorGuard(struct ChrRecord *chr);
void portReinforcementsSetDirectorSpawn(s32 on);
void portReinforcementsDirectorTick(void);       /* lv.c, once per frame after propsTick */
void portReinforcementsDirectorReset(void);
void portReinforcementsDirectorChrFreed(s32 slot);

#endif
