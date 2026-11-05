/* GE_HITMAP_PROBE=<hits>[,zoom][,live][,best=<s>][,cheat][,cap=<n>] (test harness,
 * FORK-048): in a solo stage,
 * fire <hits> player bullets through the game's own hit test (chrTestHit and
 * chrHandleBulletHit) at random points on on-screen characters' hit boxes,
 * one every few ticks, then abort the mission as the watch does. On the
 * title side it moves on from the mission status page to the debrief, opens
 * the closer look when asked, captures one GE_SHOT-style frame and exits.
 * `live` skips falling characters, whose death animations turn the body away
 * from the facing GE_HITMAP_LOG's bearing column reports. Pair it with
 * GE_CHEATS=invuln so Bond outlives the fight. `best` puts a best time for
 * this mission and difficulty in the RAM save (never the campaign file) and
 * `cheat` raises the New Cheat notice, so the debrief prints its longest rows.
 * `cap` records at most n hits, so the rest exercise the closer look's Not
 * recorded line without 1024 targets. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <boss.h>
#include "bondaicommands.h"
#include "game/bondview.h"
#include "game/chr.h"
#include "game/chrai.h"
#include "game/file2.h"
#include "game/front.h"
#include "game/gun.h"
#include "game/matrixmath.h"
#include "game/model.h"
#include "game/mp_music.h"
#include "port_hitmap.h"

#define PROBE_SPACING 4   /* ticks between shots */
#define PROBE_SETTLE  240 /* ticks after the stage starts */
#define PROBE_GIVE_UP 7200 /* abort even when too few characters came into view */

extern PropRecord *getCurrentPlayerProp(void);
extern int ge_shot_force;
void portHitMapProbeTick(void);
void portHitMapProbeMenu(s32 menu);

static s32 probeHits = -1; /* -1 unread, 0 off */
static s32 probeZoom;
static s32 probeLive;
static s32 probeBest;
static s32 probeCheat;
static s32 probeFired;
static s32 probeTick;
static s32 probeMenuTicks;
static u32 probeSeed = 0x1234567u;

static void probeRead(void)
{
    const char *e = getenv("GE_HITMAP_PROBE");

    probeHits = (e != NULL) ? atoi(e) : 0;
    probeZoom = e != NULL && strstr(e, "zoom") != NULL;
    probeLive = e != NULL && strstr(e, "live") != NULL;
    probeBest = (e != NULL && strstr(e, "best=") != NULL) ? atoi(strstr(e, "best=") + 5) : 0;
    probeCheat = e != NULL && strstr(e, "cheat") != NULL;
    if (e != NULL && strstr(e, "cap=") != NULL) portHitMapLimitSet(atoi(strstr(e, "cap=") + 4));
    probeTick = -PROBE_SETTLE;
}

static f32 probeRandom(void)
{
    probeSeed = probeSeed * 1664525u + 1013904223u;
    return (probeSeed >> 8) / 16777216.0f;
}

static ModelNode *probeBodyBox(Model *model)
{
    ModelNode *node, *pick = NULL;
    s32 seen = 0;

    for (node = model->obj->RootNode; node != NULL; ) {
        if ((node->Opcode & 0xff) == MODELNODE_OPCODE_BBOX) {
            const s32 part = (s32)node->Data->BoundingBox.ModelNumber;

            if (part >= HIT_LEFT_FOOT && part <= HIT_CHEST && probeRandom() * ++seen < 1.0f) pick = node;
        }
        if (node->Child != NULL) { node = node->Child; continue; }
        while (node != NULL && node->Next == NULL) node = node->Parent;
        if (node != NULL) node = node->Next;
    }
    return pick;
}

/* One bullet from the camera through a random point inside a random box. */
static s32 probeShoot(PropRecord *prop)
{
    ShotData shot;
    ModelNode *node = probeBodyBox(prop->chr->model);
    const struct bbox *b;
    Mtxf *mtx;
    coord3d local, target;
    f32 len;
    s32 a, c;

    if (node == NULL || (mtx = modelFindNodeMtx(prop->chr->model, node, 0)) == NULL) return FALSE;
    b = &node->Data->BoundingBox.Bounds;
    for (a = 0; a < 3; a++) {
        local.f[a] = b->f[a][0] + (b->f[a][1] - b->f[a][0]) * (0.1f + 0.8f * probeRandom());
    }
    for (c = 0; c < 3; c++) {
        target.f[c] = mtx->m[3][c] + local.x * mtx->m[0][c] + local.y * mtx->m[1][c] + local.z * mtx->m[2][c];
    }
    len = sqrtf(target.x * target.x + target.y * target.y + target.z * target.z);
    if (!(len > 0.0f)) return FALSE;
    memset(&shot, 0, sizeof(shot));
    shot.weapon = ITEM_AK47;
    shot.viewDir.x = target.x / len;
    shot.viewDir.y = target.y / len;
    shot.viewDir.z = target.z / len;
    shot.dir = shot.viewDir;
    mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), &shot.dir);
    shot.maxdist = 100000.0f;
    inc_curplayer_hitcount_with_weapon(shot.weapon, SHOT_REGISTER_TOTAL);
    chrTestHit(prop, &shot);
    if (shot.hits[0].prop == NULL) return TRUE;
    chrHandleBulletHit(&shot, &shot.hits[0]);
    return TRUE;
}

static PropRecord *probeTarget(void)
{
    PropRecord **p;
    PropRecord *pick = NULL;
    s32 seen = 0;

    for (p = g_OnScreenPropList; p < g_LastOnScreenProp; p++) {
        PropRecord *prop = *p;

        if (prop == NULL || prop->type != PROP_TYPE_CHR || prop->chr == NULL || prop->chr->model == NULL) continue;
        /* chrTestHit walks the hit list the last render built; none yet, no target. */
        if (!(prop->flags & PROPFLAG_ONSCREEN) || prop->chr->field_20 == NULL
            || prop->chr->actiontype == ACT_DEAD || (probeLive && prop->chr->actiontype == ACT_DIE)) continue;
        if (probeRandom() * ++seen < 1.0f) pick = prop;
    }
    return pick;
}

void portHitMapProbeTick(void)
{
    PropRecord *bond, *target;

    if (probeHits < 0) probeRead();
    if (probeHits == 0 || probeFired < 0) return;
    bond = getCurrentPlayerProp();
    if (bond == NULL || bond->stan == NULL) return;
    if (probeTick++ < 0 || probeTick % PROBE_SPACING != 0) return;
    if (probeFired < probeHits && probeTick < PROBE_GIVE_UP) {
        target = probeTarget();
        if (target != NULL && probeShoot(target)) probeFired++;
        return;
    }
    fprintf(stderr, "ge: hitmap probe fired=%d recorded=%d, aborting\n", probeFired, portHitMapCount());
    probeFired = -1;
    set_missionstate(MISSION_STATE_0);
    bossRunTitleStage();
    mission_failed_or_aborted = TRUE;
}

void portHitMapProbeMenu(s32 menu)
{
    if (probeHits <= 0 || probeFired >= 0) return;
    if (menu == MENU_MISSION_FAILED && ++probeMenuTicks > 60) {
        extern void fileSetDifficultyStageTime(save_data *save, LEVEL_SOLO_SEQUENCE level, DIFFICULTY difficulty, s32 seconds);

        probeMenuTicks = 0;
        if (probeBest > 0) {
            fileSetDifficultyStageTime(fileGetSaveForFoldernum(selected_folder_num),
                                       mission_folder_setup_entries[briefingpage].mission_num, selected_difficulty, probeBest);
        }
        if (probeCheat) g_NewCheatUnlocked = TRUE;
        frontChangeMenu(MENU_MISSION_COMPLETE, FALSE);
        return;
    }
    if (menu != MENU_MISSION_COMPLETE) return;
    probeMenuTicks++;
    if (probeMenuTicks == 30 && probeZoom) portHitMapZoomSet(TRUE);
    if (probeMenuTicks == 90) ge_shot_force = 1;
    if (probeMenuTicks == 100) {
        fprintf(stderr, "ge: hitmap probe captured the debrief\n");
        exit(0);
    }
}
