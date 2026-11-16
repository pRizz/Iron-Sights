/**
 * port_late_loads.c - loading work that happens after a stage has loaded.
 *
 * The rule (scripts/port/late_loads.md): a stage pays for its rooms,
 * textures, models and shaders while it loads, not when the player first
 * looks at them. Work done on first use during play stalls a frame - the
 * Runway shutter decoded 60 HD textures in one 350 ms frame. This file marks
 * when a stage's load ends and reports every timed load event after that.
 *
 * A stage's first LATE_HIDDEN_FRAMES frames are drawn in full but presented
 * black, each finished in the driver (gfx_opengl_end_frame,
 * gfx_sdl_blank_back_buffer): a program's first real draw, the driver's
 * background compiles, the level cube's first build and the load's uploads
 * cost there, behind the load's black screen, rather than in the first frames
 * of play (Runway's second frame waited 190 ms in the driver). The load ends
 * with the first frame shown.
 *
 *
 *   ge: late load: <kind> <ms> ms during play (stage, tick): <what>
 *       printed for an event of GE_LATEWARNMS (default 3, 0 = off) or more,
 *       at most 20 lines a stage, so a run's log names a regression;
 *   GE_LATELOG=<file> writes every late event, of any size, one line each:
 *       L <stage> <tick> <kind> <microseconds> <what>
 *       plus 'S <stage>' when a stage's load ends; late_load_audit.py reads it.
 *
 * Kinds are hitchlog.c's categories (rooms, models, tex, shaders, uploads)
 * plus detail-only kinds that do not add to those counters (hd: an HD image
 * read on the render thread, inside an upload).
 *
 * portLateLoadActive() tells the renderer whether the stage is in play, so a
 * texture it can read in the background then is not read on the spot.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define LATE_WARN_LINES 20
#define LATE_HIDDEN_FRAMES 3

static const char *const sLateCatName[5] = { "rooms", "models", "tex", "shaders", "uploads" };

static volatile int sLateActive;  /* the stage's first frame has been presented */
static volatile int sLateReady;   /* the stage has loaded; its first frame is on its way */
static int sLateHidden;           /* the stage's frames drawn but not shown so far (render thread) */
static int sLateThisHidden;       /* the frame being presented was kept off the screen */
static int sLateInit, sLateWarnUs, sLateWarnLines;
static FILE *sLateFile;

extern void portHitchNote(int cat, unsigned long long us);

static void lateInit(void)
{
    const char *warn, *path;

    if (sLateInit)
    {
        return;
    }
    sLateInit = 1;
    warn = getenv("GE_LATEWARNMS");
    sLateWarnUs = (warn != NULL && *warn != '\0') ? atoi(warn) * 1000 : 3000;
    path = getenv("GE_LATELOG");
    if (path != NULL && *path != '\0')
    {
        sLateFile = fopen(path, "w");
        if (sLateFile == NULL)
        {
            printf("ge: late load: cannot create %s\n", path);
        }
        else
        {
            fprintf(sLateFile, "# ge-latelog 1: L <stage> <tick> <kind> <us> <what>; S <stage> <tick> = load ended\n");
            fflush(sLateFile);
        }
    }
}

static int lateStage(void)
{
    extern int g_StageNum; /* s32, boss.c */
    return g_StageNum;
}

static int lateTick(void)
{
    extern int g_GlobalTickCount; /* s32, lv.c */
    return g_GlobalTickCount;
}

/** boss.c, before lvlStageLoad: what follows is the stage's load. */
void portLateLoadStageBegin(void)
{
    lateInit();
    sLateActive = 0;
    sLateReady = 0;
    sLateWarnLines = 0;
}

/** boss.c, once lvlStageLoad and its holds are done. */
void portLateLoadStageReady(void)
{
    sLateHidden = 0;
    sLateReady = 1;
}

int portLoadAheadPart(int part);

/** gfx_sdl2.cpp, render thread, before each swap: 1 to keep this frame
 * off the screen, one of the stage's first LATE_HIDDEN_FRAMES. */
int portLateLoadHideFrame(void)
{
    sLateThisHidden = sLateReady && !sLateActive && portLoadAheadPart(8) && sLateHidden < LATE_HIDDEN_FRAMES;
    sLateHidden += sLateThisHidden;
    return sLateThisHidden;
}

/** hitchlog.c, render thread, after every present: the first frame shown
 * after the stage is ready ends its load. */
void portLateLoadFramePresented(void)
{
    if (!sLateReady || sLateActive || sLateThisHidden)
    {
        return;
    }
    sLateActive = 1;
    lateInit();
    if (sLateFile != NULL)
    {
        fprintf(sLateFile, "S %d %d\n", lateStage(), lateTick());
        fflush(sLateFile);
    }
}

int portLateLoadActive(void)
{
    return sLateActive;
}

/**
 * GE_LOAD_AHEAD=0 restores first-use loading, for before/after audits and
 * benchmarks of one executable: no HD prefetch (the download's images are
 * read when first drawn, in play too), the shader prewarm keeps its 3 ms a
 * frame during loads, GL texture names are made one at a time, and no
 * warm-up frames. A mask keeps some parts: 1 HD prefetch, 2 prewarm at load
 * (and room-cache twins), 4 texture names ahead, 8 warm-up frames and the
 * loading finish; unset is all of them (15).
 */
static int lateAheadMask(void)
{
    static int mask = -1;

    if (mask < 0)
    {
        const char *e = getenv("GE_LOAD_AHEAD");
        mask = (e == NULL || *e == '\0') ? 15 : (int) strtol(e, NULL, 0) & 15;
        if (mask != 15)
        {
            printf("ge: late load: GE_LOAD_AHEAD=%d, the other parts load on first use as before\n", mask);
        }
    }
    return mask;
}

int portLoadAheadEnabled(void)
{
    return lateAheadMask() != 0;
}

int portLoadAheadPart(int part)
{
    return (lateAheadMask() & part) != 0;
}

static void lateReport(const char *kind, unsigned long long us, const char *what)
{
    const int stage = lateStage(), tick = lateTick();

    if (sLateFile != NULL)
    {
        fprintf(sLateFile, "L %d %d %s %llu %s\n", stage, tick, kind, us, what);
        fflush(sLateFile);
    }
    if (sLateWarnUs > 0 && us >= (unsigned long long) sLateWarnUs && sLateWarnLines < LATE_WARN_LINES)
    {
        sLateWarnLines++;
        printf("ge: late load: %s %llu.%llu ms during play (stage %d, tick %d): %s%s\n", kind, us / 1000,
               (us / 100) % 10, stage, tick, what,
               sLateWarnLines == LATE_WARN_LINES ? " (no more this stage)" : "");
        fflush(stdout);
    }
}

/** A detail-only late event: kind and what as printf text; no counters. */
void portLateLoadNote(const char *kind, unsigned long long us, const char *fmt, ...)
{
    char what[160];
    va_list args;

    if (!sLateActive)
    {
        return;
    }
    va_start(args, fmt);
    vsnprintf(what, sizeof(what), fmt, args);
    va_end(args);
    lateReport(kind, us, what);
}

/**
 * hitchlog.c's counters plus the late report: cat as portHitchNote, what a
 * name (NULL for none) and id a number naming the item (room, texture).
 */
void portHitchNoteItem(int cat, unsigned long long us, const char *what, unsigned int id)
{
    char text[160];

    portHitchNote(cat, us);
    if (!sLateActive || cat < 0 || cat >= 5)
    {
        return;
    }
    if (what != NULL)
    {
        snprintf(text, sizeof(text), "%s", what);
    }
    else
    {
        snprintf(text, sizeof(text), "%u", id);
    }
    lateReport(sLateCatName[cat], us, text);
}
