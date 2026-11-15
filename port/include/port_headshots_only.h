/* Headshots Only (FORK-083): a solo mission modifier where only the
 * player's head hits hurt guards.
 *
 * Off by default. Set on each mission's difficulty page (a row beside
 * Reinforcements) or under Gameplay > Iron Sights > Enemies. The setting is
 * read when a solo stage loads, so a change made mid-mission applies from
 * the next one and a run is either all Headshots Only or not at all.
 *
 * With it on, a player's bullet or thrown knife that hits a guard anywhere
 * but the head does no damage: the guard flinches and notices, the hit still
 * counts in the accuracy figures and the debrief hit map, and the confirmed
 * hit sound still plays, but no hit marker shows and the guard is not
 * staggered. Hits on a hat or a held gun keep their stock handling (no
 * damage either way; a moonraker helmet already counts as the head).
 *
 * Melee (fists and the hunting knife, ITEM_FIST / ITEM_KNIFE) never reports
 * a head hit, so Headshots Melee decides it: Allowed (default) keeps stock
 * melee damage, so Bunker's cell and Archives' unarmed start stay playable;
 * Blocked makes melee do nothing too. Explosions are exempt (they cannot be
 * told apart from a guard's own grenade), as are guard-on-guard fire,
 * scripted damage and multiplayer. Demo playback stays stock.
 *
 * Completing a mission with it on, under the same rule the stock save uses
 * (Agent to 00 Agent, no cheats active), records a headshot completion and
 * best time per campaign folder in Data/Saves/headshots.ini
 * (port_headshots_records.c). The difficulty page marks those difficulties
 * with a crosshair-and-head icon, and while the mode is on its Best Time
 * column shows the headshot records. */
#ifndef PORT_HEADSHOTS_ONLY_H
#define PORT_HEADSHOTS_ONLY_H

#include <ultra64.h>
#include <bondconstants.h>
#include <bondaicommands.h>

#define PORT_HEADSHOTS_ONLY_DEFAULT 0
/* headshots_only_melee: 1 = melee keeps its damage, 0 = blocked */
#define PORT_HEADSHOTS_ONLY_MELEE_DEFAULT 1

/* Records: the save's four folders, the solo stages, Agent to 00 Agent (the
 * stock save keeps no 007 times, and its completion rule skips 007). */
#define PORT_HEADSHOTS_FOLDERS 4
#define PORT_HEADSHOTS_DIFFICULTIES 3
/* A record's seconds: 0 is none; the stock save's 1023 ceiling marks a
 * completion too long to time. */
#define PORT_HEADSHOTS_UNTIMED 1023

static inline s32 portHeadshotsOnlyMelee(s32 weaponid)
{
    return weaponid == ITEM_FIST || weaponid == ITEM_KNIFE;
}

/* Whether a stage runs Headshots Only: solo play, not demo playback. */
static inline s32 portHeadshotsOnlyStageActive(s32 setting, s32 solo, s32 demo)
{
    return setting && solo && !demo;
}

/* Whether this hit's damage is blocked. Only the player's hits on a guard in
 * a Headshots Only stage; the head, a hat and a held gun keep stock handling,
 * and melee keeps it while Headshots Melee allows it. */
static inline s32 portHeadshotsOnlyBlocks(s32 stageActive, s32 allowMelee, s32 byPlayer,
                                          s32 hitpart, s32 weaponid)
{
    if (!stageActive || !byPlayer) return FALSE;
    if (hitpart == HIT_HEAD || hitpart == HIT_HAT || hitpart == HIT_GUN) return FALSE;
    if (allowMelee && portHeadshotsOnlyMelee(weaponid)) return FALSE;
    return TRUE;
}

/* A finished run's seconds, kept to the stock save's 1..1023 range. */
static inline s32 portHeadshotsRecordSeconds(s32 seconds)
{
    if (seconds < 1) return 1;
    return seconds > PORT_HEADSHOTS_UNTIMED ? PORT_HEADSHOTS_UNTIMED : seconds;
}

/* Whether a new run replaces the kept record (0 = none yet). */
static inline s32 portHeadshotsRecordBetter(s32 kept, s32 seconds)
{
    return kept <= 0 || seconds < kept;
}

/* The setting (port_headshots_only.c). */
s32 portHeadshotsOnlyOptionGet(const char *key, s32 *value);
s32 portHeadshotsOnlyOptionSet(const char *key, s32 value);
s32 portHeadshotsOnlySetting(void);
s32 portHeadshotsOnlyMeleeSetting(void);
/* Latches the setting as a stage loads (lv.c). */
void portHeadshotsOnlyBeginStage(s32 solo, s32 demo);
s32 portHeadshotsOnlyStageOn(void);
/* chraction.c handles_shot_actors: TRUE when this hit does no damage. */
s32 portHeadshotsOnlyBlocksHit(s32 byPlayer, s32 hitpart, s32 weaponid);

/* Records (port_headshots_records.c). Folder is FOLDER1..; difficulty
 * DIFFICULTY_AGENT..DIFFICULTY_00. Seconds 0 = none. */
s32 portHeadshotsRecordGet(s32 folder, s32 mission, s32 difficulty);
/* A record the difficulty page shows: the save must also have the stock
 * completion, so a replaced or reset save never shows stale marks. */
s32 portHeadshotsRecordShown(s32 folder, s32 mission, s32 difficulty);
/* "mm:ss" for a shown, timed record, else "--:--" and FALSE. */
s32 portHeadshotsBestTimeText(s32 folder, s32 mission, s32 difficulty, char *text, s32 size);
/* file.c end_of_mission_briefing, after the stock save records the run. */
void portHeadshotsMissionComplete(s32 folder, s32 mission, s32 difficulty, s32 seconds);
/* file2.c: a folder erased, or copied to another. */
void portHeadshotsFolderErased(s32 folder);
void portHeadshotsFolderCopied(s32 from, s32 to);

#endif
