/* Host tests for port/src/port_macos.c: the executable's loaded range and the
 * sleep to an absolute time (MAC-001 Task 1), and the executable's real path
 * (MAC-002 Task 2). No game header: the functions use only the system's. Each
 * test prints its name and aborts on a failed check. Run with one argument, a
 * path, it checks only that the executable's path is that file's real path:
 * the harness runs it so through a symlink. */
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/time.h>
#include <time.h>

#include "port_macos.h"

#define CHECK(cond) \
    do \
    { \
        if (!(cond)) \
        { \
            fprintf(stderr, "test_macos_host: FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            abort(); \
        } \
    } while (0)

#define NS_PER_S 1000000000LL
#define NS_PER_MS 1000000LL

static int s_FileScopeVariable = 1;

static long long clockNs(void)
{
    struct timespec now;

    CHECK(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return (long long) now.tv_sec * NS_PER_S + now.tv_nsec;
}

static struct timespec timeAt(long long ns)
{
    struct timespec t;

    t.tv_sec = (time_t) (ns / NS_PER_S);
    t.tv_nsec = (long) (ns % NS_PER_S);
    return t;
}

static void imageRangeHoldsMain(uintptr_t mainAddress)
{
    uintptr_t lowest = 0;
    uintptr_t end = 0;

    puts("image range holds main");
    /* Act */
    CHECK(portMacImageRange(&lowest, &end) == 1);
    /* Assert */
    CHECK(lowest <= mainAddress);
    CHECK(mainAddress < end);
    CHECK(lowest <= (uintptr_t) &s_FileScopeVariable);
    CHECK((uintptr_t) &s_FileScopeVariable < end);
}

static void imageRangeIsAbove4Gb(void)
{
    uintptr_t lowest = 0;
    uintptr_t end = 0;

    puts("image range is above 4 GB");
    /* Act */
    CHECK(portMacImageRange(&lowest, &end) == 1);
    /* Assert: __PAGEZERO, which starts at 0, is left out. */
    CHECK(lowest >= (uintptr_t) 1 << 32);
    CHECK(end > lowest);
}

static void sleepReachesAFutureTime(void)
{
    long long target = clockNs() + 20 * NS_PER_MS;
    struct timespec t = timeAt(target);
    long long after;

    puts("sleep reaches a future time");
    /* Act */
    CHECK(portMacSleepUntil(&t) == 0);
    /* Assert */
    after = clockNs();
    CHECK(after >= target);
    CHECK(after < target + 250 * NS_PER_MS);
}

static void sleepReturnsAtOnceForAPastTime(void)
{
    long long before = clockNs();
    struct timespec t = timeAt(before - NS_PER_S);

    puts("sleep returns at once for a past time");
    /* Act */
    CHECK(portMacSleepUntil(&t) == 0);
    /* Assert */
    CHECK(clockNs() - before < 50 * NS_PER_MS);
}

static void onAlarm(int sig)
{
    (void) sig;
}

static void anInterruptedSleepSaysEintr(void)
{
    struct sigaction action;
    struct sigaction previous;
    struct itimerval timer = { { 0, 0 }, { 0, 5000 } };
    long long target;
    struct timespec t;
    int result;

    puts("an interrupted sleep says EINTR");
    /* Arrange: an alarm in 5 ms whose handler does not restart the call. */
    action.sa_handler = onAlarm;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    CHECK(sigaction(SIGALRM, &action, &previous) == 0);
    target = clockNs() + 200 * NS_PER_MS;
    t = timeAt(target);
    CHECK(setitimer(ITIMER_REAL, &timer, NULL) == 0);
    /* Act */
    result = portMacSleepUntil(&t);
    /* Assert */
    CHECK(result == EINTR);
    CHECK(clockNs() < target);
    /* Act: the callers' loop, retrying to the same absolute time. */
    while ((result = portMacSleepUntil(&t)) == EINTR)
    {
    }
    /* Assert */
    CHECK(result == 0);
    CHECK(clockNs() >= target);
    CHECK(sigaction(SIGALRM, &previous, NULL) == 0);
}

static void executablePathNamesThisFile(const char *argv0)
{
    char path[PATH_MAX];
    struct stat self;
    struct stat found;
    size_t length;

    puts("executable path names this file");
    /* Arrange */
    CHECK(stat(argv0, &self) == 0);
    /* Act */
    length = portMacExecutablePath(path, sizeof(path));
    /* Assert */
    CHECK(length > 0);
    CHECK(path[0] == '/');
    CHECK(strlen(path) == length);
    CHECK(stat(path, &found) == 0);
    CHECK(found.st_dev == self.st_dev);
    CHECK(found.st_ino == self.st_ino);
}

static void aBufferTooSmallIsLeftAlone(void)
{
    char path[4] = { 'a', 'b', 'c', 'd' };

    puts("a buffer too small is left alone");
    /* Act and assert */
    CHECK(portMacExecutablePath(path, sizeof(path)) == 0);
    CHECK(memcmp(path, "abcd", 4) == 0);
}

static void executablePathResolvesASymlink(const char *realFile)
{
    char expected[PATH_MAX];
    char path[PATH_MAX];

    puts("executable path resolves a symlink");
    /* Arrange: the harness started this program through a link to REALFILE. */
    CHECK(realpath(realFile, expected) != NULL);
    /* Act */
    CHECK(portMacExecutablePath(path, sizeof(path)) == strlen(expected));
    /* Assert */
    CHECK(strcmp(path, expected) == 0);
}

int main(int argc, char **argv)
{
    if (argc == 2)
    {
        executablePathResolvesASymlink(argv[1]);
        return 0;
    }
    executablePathNamesThisFile(argv[0]);
    aBufferTooSmallIsLeftAlone();
    imageRangeHoldsMain((uintptr_t) main);
    imageRangeIsAbove4Gb();
    sleepReachesAFutureTime();
    sleepReturnsAtOnceForAPastTime();
    anInterruptedSleepSaysEintr();
    return 0;
}
