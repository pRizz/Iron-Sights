/**
 * port_bench.c - GE_BENCH=<file>: one row of timings per presented frame.
 *
 * The benchmark runner (scripts/port/bench/bench.py) reads this file; the game
 * only records. Rows are kept in memory and written once, so logging costs no
 * I/O while frames are being timed.
 *
 *   GE_BENCH=<file>            turn the log on and name the CSV it writes
 *   GE_BENCH_END=<tick>[,<stage>]
 *                              write the file and leave once g_GlobalTickCount
 *                              reaches <tick> in <stage> (a LEVELID number;
 *                              default: any stage but the title)
 *   GE_BENCH_MAX=<frames>      rows to keep (default 200000, 60 bytes each)
 *
 * Without GE_BENCH_END the file is written when the row buffer fills.
 *
 * Columns, all microseconds unless noted (see hitchlog.c for the same split):
 *   interval  present to present
 *   run       gfx_run so far, including swap and lim
 *   swap      SDL_GL_SwapWindow
 *   lim       the max-fps limiter
 *   sim       longest stretch of game code between two pumps
 *   audio     portAudioPump since the previous frame
 *   pacer     time the pump slept on its schedule since the previous frame
 *   draws     draw batches (count)
 *   stage     g_StageNum (LEVELID)
 *   tick      g_GlobalTickCount
 *   rooms models tex shaders uploads   timed load events since the previous frame
 *
 * Kept free of fork-only symbols: the stock comparison build compiles this
 * same file (scripts/port/bench/stock.py).
 */
#include <stdio.h>
#include <stdlib.h>

#define BENCH_EVENTS 5
#define BENCH_DEFAULT_ROWS 200000u
#define BENCH_STAGE_TITLE 90 /* LEVELID_TITLE */
#define BENCH_ANY_STAGE (-1)

typedef struct BenchRow
{
    unsigned int interval, run, swap, lim, sim, audio, pacer, draws;
    int stage, tick;
    unsigned int events[BENCH_EVENTS];
} BenchRow;

enum { BENCH_UNREAD = 0, BENCH_OFF, BENCH_ON, BENCH_WRITTEN };

static int sState = BENCH_UNREAD;
static const char *sPath;
static BenchRow *sRows;
static unsigned int sCount, sCapacity;
static int sEndTick, sEndStage = BENCH_ANY_STAGE;
static unsigned long long sLastAudioUs, sLastPacerUs;

static unsigned int benchClampU32(unsigned long long us)
{
    return (us > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (unsigned int) us;
}

static void benchInit(void)
{
    const char *maybePath = getenv("GE_BENCH");
    const char *maybeEnd = getenv("GE_BENCH_END");
    const char *maybeMax = getenv("GE_BENCH_MAX");

    sState = BENCH_OFF;
    if (maybePath == NULL || maybePath[0] == '\0')
    {
        return;
    }

    sCapacity = (maybeMax != NULL && atoi(maybeMax) > 0) ? (unsigned int) atoi(maybeMax) : BENCH_DEFAULT_ROWS;
    sRows = (BenchRow *) malloc((size_t) sCapacity * sizeof(BenchRow));
    if (sRows == NULL)
    {
        printf("ge: bench: no memory for %u rows, log off\n", sCapacity);
        fflush(stdout);
        return;
    }

    if (maybeEnd != NULL)
    {
        sscanf(maybeEnd, "%d,%d", &sEndTick, &sEndStage);
    }
    sPath = maybePath;
    sState = BENCH_ON;
    printf("ge: bench: logging frames to %s (%u rows, end tick %d, end stage %d)\n",
           sPath, sCapacity, sEndTick, sEndStage);
    fflush(stdout);
}

static void benchWrite(const char *reason)
{
    extern int portTickHz(void);
    extern int portRenderAsyncActive(void);
    FILE *maybeFile = fopen(sPath, "w");
    unsigned int i;

    sState = BENCH_WRITTEN;
    if (maybeFile == NULL)
    {
        printf("ge: bench: cannot create %s\n", sPath);
        fflush(stdout);
        return;
    }

    fprintf(maybeFile, "# ge-bench 1 tickhz=%d render_thread=%d fixedtick=%d nosleep=%d reason=%s\n",
            portTickHz(), portRenderAsyncActive() ? 1 : 0, getenv("GE_FIXEDTICK") != NULL,
            getenv("GE_NOSLEEP") != NULL, reason);
    fprintf(maybeFile, "interval,run,swap,lim,sim,audio,pacer,draws,stage,tick,rooms,models,tex,shaders,uploads\n");
    for (i = 0; i < sCount; i++)
    {
        const BenchRow *row = &sRows[i];

        fprintf(maybeFile, "%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%u,%u,%u,%u,%u\n",
                row->interval, row->run, row->swap, row->lim, row->sim, row->audio, row->pacer, row->draws,
                row->stage, row->tick, row->events[0], row->events[1], row->events[2], row->events[3],
                row->events[4]);
    }
    fclose(maybeFile);
    printf("ge: bench: wrote %u frames to %s (%s)\n", sCount, sPath, reason);
    fflush(stdout);
}

static int benchEndReached(int stage, int tick)
{
    if (sEndTick <= 0 || tick < sEndTick)
    {
        return 0;
    }
    if (sEndStage == BENCH_ANY_STAGE)
    {
        return stage != BENCH_STAGE_TITLE;
    }
    return stage == sEndStage;
}

/**
 * hitchlog.c portHitchFrame, once per presented frame. simmax_us and
 * event_us are the values it has just taken from its own counters.
 */
void portBenchFrame(unsigned long long interval_us, unsigned long long run_us,
                    unsigned long long swap_us, unsigned long long lim_us, unsigned int draws,
                    unsigned long long simmax_us, const unsigned long long *event_us)
{
    extern unsigned long long g_PortAudioUs;
    extern unsigned long long g_PortPumpSleepUs;
    extern int g_GlobalTickCount; /* s32 */
    extern int g_StageNum;        /* s32 */
    BenchRow *row;
    int i;

    if (sState == BENCH_UNREAD)
    {
        benchInit();
    }
    if (sState != BENCH_ON)
    {
        return;
    }

    row = &sRows[sCount++];
    row->interval = benchClampU32(interval_us);
    row->run = benchClampU32(run_us);
    row->swap = benchClampU32(swap_us);
    row->lim = benchClampU32(lim_us);
    row->sim = benchClampU32(simmax_us);
    row->audio = benchClampU32(g_PortAudioUs - sLastAudioUs);
    row->pacer = benchClampU32(g_PortPumpSleepUs - sLastPacerUs);
    row->draws = draws;
    row->stage = g_StageNum;
    row->tick = g_GlobalTickCount;
    for (i = 0; i < BENCH_EVENTS; i++)
    {
        row->events[i] = benchClampU32(event_us[i]);
    }
    sLastAudioUs = g_PortAudioUs;
    sLastPacerUs = g_PortPumpSleepUs;

    if (benchEndReached(row->stage, row->tick))
    {
        benchWrite("end");
#ifndef __EMSCRIPTEN__
        /* Leave here: the run is over, and no teardown is worth timing.
         * _Exit skips stdio, so write the profile (GE_PROFILE, port_profile.c)
         * and flush every log first. */
        {
            extern void portProfileWrite(void);
            portProfileWrite();
        }
        fflush(NULL);
        _Exit(0);
#endif
        return;
    }
    if (sCount >= sCapacity)
    {
        benchWrite("full");
    }
}

/** port_load_tour.c: end the run at the next frame, as the end tick would.
 * Returns 0 when no frame log is being kept (the caller quits instead). */
int portBenchRequestEnd(void)
{
    if (sState != BENCH_ON)
    {
        return 0;
    }
    sEndTick = 1;
    sEndStage = BENCH_ANY_STAGE;
    return 1;
}
