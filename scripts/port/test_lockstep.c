/* ROM-free checks of lockstep (LP64-006 Task 2, test_lp64_compare.sh):
 * GE_LOCKSTEP makes the game's clock count the retraces the pump delivered,
 * and the pump deliver one retrace for each wait of the game's frame queue.
 * libultra_shim.c is included whole with its host clock stubbed (the macro
 * below renames its clock_gettime call), port_lockstep.c with it; pcsched.c,
 * the real pump, is compiled beside this file. --gc-sections drops what the
 * pump and the clock do not reach, so only that needs a stub here. Each
 * scenario runs in its own child, since port_lockstep.c reads GE_LOCKSTEP once
 * a process. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "port_lockstep.h"

/* The stubbed host clock: 12.345678 s, read by osGetTime when lockstep is off. */
#define TEST_HOST_SECONDS 12
#define TEST_HOST_NANOSECONDS 345678000L
static int testHostClock(clockid_t clock, struct timespec *now)
{
    (void) clock;
    now->tv_sec = TEST_HOST_SECONDS;
    now->tv_nsec = TEST_HOST_NANOSECONDS;
    return 0;
}
#define clock_gettime testHostClock
#include "../../port/src/libultra_shim.c"
#undef clock_gettime
#include "../../port/src/port_lockstep.c"
#include "sched.h"

#define FRAMES 100
#define PUMPS_A_FRAME 3

/* What pcsched.c's pump reaches outside itself. */
OSSched os_scheduler;
s32 g_ContQueuesCreated;
void portConLogf(const char *fmt, ...)
{
    (void) fmt;
}
void portHitchSimSpan(unsigned long long us)
{
    (void) us;
}
s32 portTickHz(void)
{
    return 60;
}
void portGunTexAudit(void)
{
}
void portRoomAuditTick(void)
{
}
void joyPoll(void)
{
}
void musicFadeTick(void)
{
}
void portAudioPump(void)
{
}
s32 get_is_ramrom_flag(void)
{
    return 0;
}
void ge_dbg_vec(const char *tag, const f32 *v, s32 n)
{
    (void) tag;
    (void) v;
    (void) n;
}
extern void portPumpEvents(void);
extern u32 g_PortFramesPumped;

static OSScClient sClient;
static OSMesg sClientBuffer[32];
static OSMesgQueue sClientQueue;
static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lockstep: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

/* The game's frame queue: one scheduler client, as init.c registers. */
static void registerClient(void)
{
    osCreateMesgQueue(&sClientQueue, sClientBuffer, 32);
    sClient.msgQ = &sClientQueue;
    sClient.next = NULL;
    os_scheduler.clientList = &sClient;
}

/* The retrace messages waiting in the frame queue, taken out of it. */
static int takeRetraces(void)
{
    OSMesg message;
    int count = 0;

    while (osRecvMesg(&sClientQueue, &message, OS_MESG_NOBLOCK) == 0)
    {
        count += ((OSScMsg *) message)->type == OS_SC_RETRACE_MSG;
    }
    return count;
}

static double hostSeconds(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double) now.tv_sec + (double) now.tv_nsec / 1e9;
}

static int timeIsRetracesTimesAField(void)
{
    int retrace;

    // Arrange
    registerClient();

    // Act
    portPumpEvents();
    takeRetraces();

    // Assert
    expect(portLockstepMicroseconds() == 16666u, "16,666 us after one retrace");
    expect(osGetTime() == 16666ull * 46875ull / 1000ull, "osGetTime counts the retraces");

    // Act
    for (retrace = 2; retrace <= 60; retrace++)
    {
        portPumpEvents();
        takeRetraces();
    }

    // Assert
    expect(portLockstepMicroseconds() == 1000000u, "1,000,000 us after 60 retraces");
    return failures != 0;
}

static int unsetReadsTheHostClock(void)
{
    const u64 microseconds = (u64) TEST_HOST_SECONDS * 1000000ull + (u64) TEST_HOST_NANOSECONDS / 1000ull;

    // Act
    const OSTime now = osGetTime();

    // Assert
    expect(!portLockstep(), "lockstep is off with GE_LOCKSTEP unset");
    expect(now == microseconds * 46875ull / 1000ull, "osGetTime converts the host clock as before");
    expect(osGetCount() == (u32) now, "osGetCount is osGetTime's low word");
    return failures != 0;
}

static int oneRetraceAFrame(void)
{
    double started;
    int frame;
    int pump;

    // Arrange
    registerClient();
    started = hostSeconds();

    for (frame = 1; frame <= FRAMES; frame++)
    {
        // Act: a frame whose waits pump three times
        for (pump = 0; pump < PUMPS_A_FRAME; pump++)
        {
            portPumpEvents();
        }

        // Assert
        if (takeRetraces() != 1)
        {
            expect(0, "one retrace message a frame");
            break;
        }
    }
    expect(g_PortFramesPumped == FRAMES, "the pump counts one retrace a frame");
    expect(hostSeconds() - started < 1.0, "the pump never sleeps (100 paced retraces take 1.67 s)");
    return failures != 0;
}

/* Runs SCENARIO in a child with GE_LOCKSTEP set when LOCKSTEP. */
static int inChild(const char *name, int lockstep, int (*scenario)(void))
{
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    if (child < 0)
    {
        perror("fork");
        return 1;
    }
    if (child == 0)
    {
        if (lockstep)
        {
            setenv("GE_LOCKSTEP", "1", 1);
        }
        else
        {
            unsetenv("GE_LOCKSTEP");
        }
        unsetenv("GE_FRAMES");
        status = scenario();
        fflush(stdout); /* _exit skips stdio */
        _exit(status);
    }
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        printf("test_lockstep: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), name);
        return 1;
    }
    return 0;
}

int main(void)
{
    int bad = 0;

    bad |= inChild("time_is_retraces_times_a_field", 1, timeIsRetracesTimesAField);
    bad |= inChild("unset_reads_the_host_clock", 0, unsetReadsTheHostClock);
    bad |= inChild("one_retrace_a_frame", 1, oneRetraceAFrame);
    if (bad)
    {
        return 1;
    }
    printf("test_lockstep: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
