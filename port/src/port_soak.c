/* Growth soak audit: GE_SOAK plays a list of solo stages over and over in one
 * process, each for a number of frames, returning through the title between
 * them as an attract demo does, and logs process memory and renderer resource
 * counts ("ge: memstats:") so growth lap over lap shows (scripts/port/soak.py).
 *
 *   GE_SOAK=<stages|all>[:<frames>[:<laps>]]  e.g. GE_SOAK=dam,facility:900:3
 *
 * GE_MEMSTATS=1 alone logs a line about a second into each stage. The web
 * build reads GE_SOAK from Module.ENV, which an audit sets before Play. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <malloc.h>
#elif defined(__GLIBC__)
#include <malloc.h>
#endif

extern void portConLogf(const char *fmt, ...);
extern void ramromFadeToTitle(void); /* ramromreplay.c: the attract demos' exit */

#define SOAK_MAX_STAGES 32
static const struct { const char *name; s32 id; } sSoakStages[] = {
    { "dam", LEVELID_DAM },           { "facility", LEVELID_FACILITY }, { "runway", LEVELID_RUNWAY },
    { "surface", LEVELID_SURFACE },   { "bunker1", LEVELID_BUNKER1 },   { "silo", LEVELID_SILO },
    { "frigate", LEVELID_FRIGATE },   { "surface2", LEVELID_SURFACE2 }, { "bunker2", LEVELID_BUNKER2 },
    { "statue", LEVELID_STATUE },     { "archives", LEVELID_ARCHIVES }, { "streets", LEVELID_STREETS },
    { "depot", LEVELID_DEPOT },       { "train", LEVELID_TRAIN },       { "jungle", LEVELID_JUNGLE },
    { "control", LEVELID_CONTROL },   { "caverns", LEVELID_CAVERNS },   { "cradle", LEVELID_CRADLE },
    { "aztec", LEVELID_AZTEC },       { "egypt", LEVELID_EGYPT },
};
#define SOAK_KNOWN ((s32) (sizeof(sSoakStages) / sizeof(sSoakStages[0])))

static s32 sSoakList[SOAK_MAX_STAGES], sSoakCount, sSoakFrames = 900, sSoakLaps = 1;
static s32 sSoakNext, sSoakLap, sSoakInStage, sSoakStageFrame, sSoakFading, sSoakSettle;
static s32 sSoakParsed, sMemStats = -1, sSoakLastStage = -1;

static void soakParse(const char *spec)
{
    char names[512];
    char *colon, *name;
    s32 i;

    sSoakCount = 0;
    if (spec == NULL || spec[0] == '\0') return;
    snprintf(names, sizeof(names), "%s", spec);
    colon = strchr(names, ':');
    if (colon != NULL) {
        *colon++ = '\0';
        sSoakFrames = atoi(colon) > 0 ? atoi(colon) : sSoakFrames;
        colon = strchr(colon, ':');
        if (colon != NULL && atoi(colon + 1) > 0) sSoakLaps = atoi(colon + 1);
    }
    if (strcmp(names, "all") == 0) {
        for (i = 0; i < SOAK_KNOWN; i++) sSoakList[sSoakCount++] = sSoakStages[i].id;
    } else {
        for (name = strtok(names, ","); name != NULL && sSoakCount < SOAK_MAX_STAGES; name = strtok(NULL, ",")) {
            for (i = 0; i < SOAK_KNOWN && strcmp(name, sSoakStages[i].name) != 0; i++) {}
            if (i < SOAK_KNOWN) sSoakList[sSoakCount++] = sSoakStages[i].id;
            else portConLogf("ge: soak: unknown stage %s\n", name);
        }
    }
    if (sSoakCount > 0)
        portConLogf("ge: soak: %d stages x %d frames x %d laps\n", (int) sSoakCount, (int) sSoakFrames, (int) sSoakLaps);
}

static void soakInit(void)
{
#ifdef __EMSCRIPTEN__
    /* libc caches the environment before Play; an audit sets Module.ENV.GE_SOAK
     * just before it, so read it live (ASCII), as gfx_sdl2.cpp reads its own */
    char spec[256];
    s32 i;
#endif
    if (sSoakParsed) return;
    sSoakParsed = 1;
#ifdef __EMSCRIPTEN__
    for (i = 0; i < (s32) sizeof(spec) - 1; i++) {
        spec[i] = (char) EM_ASM_INT({
            var v = Module['ENV']['GE_SOAK'] || '';
            return $0 < v.length ? v.charCodeAt($0) & 127 : 0;
        }, i);
        if (spec[i] == '\0') break;
    }
    spec[i] = '\0';
    soakParse(spec);
#else
    soakParse(getenv("GE_SOAK"));
#endif
}

s32 portSoakActive(void)
{
    soakInit();
    return sSoakCount > 0;
}

#ifdef GE_VIDEO
extern void gfx_memstats(unsigned out[6]); /* gfx_pc.cpp */
#endif

/** One line: process memory, then the renderer's resources. */
void portMemStatsLog(const char *where, s32 stage)
{
    unsigned long privateKb = 0, residentKb = 0, heapKb = 0;
    unsigned gfx[6] = { 0, 0, 0, 0, 0, 0 };
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *) &pmc, sizeof(pmc))) {
        privateKb = (unsigned long) (pmc.PrivateUsage / 1024);
        residentKb = (unsigned long) (pmc.WorkingSetSize / 1024);
    }
#elif defined(__EMSCRIPTEN__)
    struct mallinfo mi = mallinfo();
    privateKb = (unsigned long) (emscripten_get_heap_size() / 1024); /* the Wasm memory, which only grows */
    heapKb = (unsigned long) (mi.uordblks / 1024);
#else
    FILE *f = fopen("/proc/self/statm", "r");
    unsigned long size = 0, resident = 0;
    if (f != NULL) {
        if (fscanf(f, "%lu %lu", &size, &resident) == 2) {
            privateKb = size * 4;
            residentKb = resident * 4;
        }
        fclose(f);
    }
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
    heapKb = (unsigned long) (mallinfo2().uordblks / 1024);
#endif
#endif
#ifdef GE_VIDEO
    gfx_memstats(gfx);
#endif
    portConLogf("ge: memstats: %s lap=%d stage=%d private=%luKB resident=%luKB heap=%luKB texcache=%u freeids=%u "
                "gltex=%u fb=%u programs=%u hd=%uKB\n",
                where, (int) sSoakLap, (int) stage, privateKb, residentKb, heapKb, gfx[0], gfx[1], gfx[3], gfx[4], gfx[5],
                gfx[2]);
}

/** front.c, as a stage starts: its end then returns to the legal screen. */
s32 portSoakIsCurrent(void)
{
    return portSoakActive() && sSoakInStage;
}

/** front.c, each front-end tick: the next stage to launch, or -1 for now. */
s32 portSoakNextStage(void)
{
    if (!portSoakActive() || sSoakInStage) return -1;
    if (sSoakSettle == 0 && (sSoakNext > 0 || sSoakLap > 0)) {
        portMemStatsLog("front", sSoakLastStage);  /* back in the menus after this stage */
    }
    /* let the boot or returning menus settle before the next launch */
    if (++sSoakSettle < 60) return -1;
    if (sSoakNext >= sSoakCount) {
        sSoakNext = 0;
        if (++sSoakLap >= sSoakLaps) {
            portConLogf("ge: soak: complete after %d laps\n", (int) sSoakLaps);
#ifndef __EMSCRIPTEN__
            exit(0);
#else
            sSoakCount = 0;
            return -1;
#endif
        }
    }
    sSoakSettle = 0;
    sSoakInStage = 1;
    sSoakStageFrame = 0;
    sSoakFading = 0;
    sSoakLastStage = sSoakList[sSoakNext];
    return sSoakList[sSoakNext++];
}

/** lv.c, each frame of a stage. */
void portSoakTick(s32 stage)
{
    if (sMemStats < 0) sMemStats = getenv("GE_MEMSTATS") != NULL;
    if (!portSoakActive()) {
        if (sMemStats && sSoakStageFrame++ == 60) portMemStatsLog("stage", stage);
        return;
    }
    if (!sSoakInStage || sSoakFading) return;
    if (++sSoakStageFrame == sSoakFrames) {
        portMemStatsLog("stage", stage);
        sSoakFading = 1;
        ramromFadeToTitle();
    }
}

/** front.c, each front-end tick: a soak stage that ended is over. */
void portSoakFrontTick(void)
{
    /* also a stage that ended early (Bond killed); a tick before it loaded is not its end */
    if (sSoakInStage && sSoakStageFrame > 0) {
        sSoakInStage = 0;
        sSoakSettle = 0;
    }
    if (!portSoakActive()) sSoakStageFrame = 0;
}
