/* Opt-in, isolated quickload effect comparison; no ordinary gameplay changes. */
#if defined(GE_VIDEO) && !defined(__EMSCRIPTEN__) && !defined(GE_XBOX)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "boss.h"
#include "game/bondview.h"
#include "game/player.h"
#include "game/explosion.h"
#include "game/lv.h"
#include "game/gun.h"
#include "port_state.h"
#include <assets/oddtextures.h>

extern s32 portBondControlsLive(void);
extern int ge_shot_force;
extern void portConLogf(const char *, ...);
extern const char *portRootPath(const char *);
extern Gfx *g_ExplosionDisplayLists[];

static void auditTextureSources(s32 verify)
{
    static u32 original[2][64];
    Gfx *lists[] = { &globalDL_0x000, g_ExplosionDisplayLists[0] };
    s32 i, j;
    for (i = 0; i < 2; i++) for (j = 0; j < 64; j++) {
        const u32 command = lists[i][j].words.w0, address = lists[i][j].words.w1;
        if (command >> 24 == G_ENDDL) break;
        if (command >> 24 == G_SETTIMG) {
            u32 hash = 2166136261u, k;
            if (address >= 0x10000000u) for (k = 0; k < 256; k++) hash = (hash ^ ((u8 *)address)[k]) * 16777619u;
            portConLogf("state-effects: list=%d image=%08x source=%08x hash=%08x\n", i, command, address, hash);
            if (verify && original[i][j] != hash) {
                fputs("state-effects: effect texture memory changed\n", stderr);
                exit(2);
            }
            original[i][j] = hash;
        }
    }
}

static void auditSpawnEffects(void)
{
    PropRecord *bond = g_CurrentPlayer->prop;
    coord3d position = bond->pos;
    const f32 heading = g_CurrentPlayer->vv_theta * 3.14159265358979323846f / 180.0f;
    position.x -= sinf(heading) * 350;
    position.z += cosf(heading) * 350;
    position.y += 80;
    explosionCreate(NULL, &position, bond->stan, EXPLOSION_DEF_04, 0, 0, bond->rooms, 0);
    position.x += cosf(heading) * 100;
    explosionCreateSmoke(&position, bond->stan, 1, bond->rooms, 0);
    portConLogf("state-effects: spawn %.2f %.2f %.2f heading %.2f\n", position.x, position.y, position.z, g_CurrentPlayer->vv_theta);
}

/* Runs from the same frame boundary as the ordinary suspend-point commands. */
void portStateEffectsAudit(u32 restoreSerial)
{
    static s32 phase, ticks, sample, round;
    static u32 expectedRestore;
    static const s32 captureAt[] = {6, 18, 42, 90, 150, 240, 300, 420};
    const char *delayText = getenv("GE_STATE_EFFECTS_DELAY");
    const s32 delay = delayText ? atoi(delayText) : 1;
    if (!getenv("GE_STATE_EFFECTS_AUDIT") || !portBondControlsLive() || !g_CurrentPlayer) return;
    if (bossGetStageNum() == LEVELID_TITLE || !g_CurrentPlayer->prop || !g_SmokeBuffer || !g_ExplosionBuffer) return;
    ticks++;
    if (!phase) {
        const s32 stress = getenv("GE_STATE_EFFECTS_ADS") != NULL;
        if (ticks == 30) {
            auditTextureSources(0);
            if (stress) currentPlayerEquipWeaponWrapper(GUNRIGHT, ITEM_M16);
        }
        if (ticks == 30 && getenv("GE_STATE_EFFECTS_ACTIVE")) auditSpawnEffects();
        if (ticks < (stress ? 1200 : 90)) return;
        const s32 read = getenv("GE_STATE_EFFECTS_READ") != NULL;
        portStateRequest(PORT_STATE_QUICK, read);
        phase = read ? 2 : 1; ticks = sample = 0;
        expectedRestore = restoreSerial + 1;
        return;
    }
    if (phase == 2) {
        if (restoreSerial != expectedRestore) { fputs("state-effects: quickload failed\n", stderr); exit(2); }
        phase = 3; ticks = 1; sample = 0;
    }
    if (ticks == delay) { auditTextureSources(1); auditSpawnEffects(); }
    if (sample < 8 && ticks == delay + captureAt[sample]) {
        portConLogf("state-effects: %s sample=%d tick=%d\n", phase == 1 ? "baseline" : "restored", sample, ticks);
        ge_shot_force = 1;
        sample++;
    }
    if (ticks == delay + 440 && getenv("GE_STATE_EFFECTS_WEAPON"))
        currentPlayerEquipWeaponWrapper(GUNRIGHT, ITEM_ROCKETLAUNCH);
    if (ticks < delay + 480) return;
    if (phase == 1 || round < 2) {
        portStateRequest(PORT_STATE_QUICK, 1);
        expectedRestore = restoreSerial + 1;
        round++;
        phase = 2;
    } else {
        FILE *proof = fopen(portRootPath("state-effects-captured.txt"), "w");
        if (!proof) exit(2);
        fputs("Captured baseline and quickload effect sequences.\n", proof);
        fclose(proof);
        exit(0);
    }
}
#else
void portStateEffectsAudit(unsigned restoreSerial) { (void)restoreSerial; }
#endif
