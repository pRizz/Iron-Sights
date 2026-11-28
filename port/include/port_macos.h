/* macOS host functions (MAC-001): what the 64-bit game needs from the system
 * that macOS spells differently from Linux. port/src/port_macos.c is compiled
 * only when CMake's APPLE is true; shared files call these from __APPLE__
 * branches. scripts/port/test_macos_host.sh tests them. */
#ifndef PORT_MACOS_H
#define PORT_MACOS_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The main executable's loaded range: the span of its LC_SEGMENT_64 load
 * commands with dyld's slide applied, __PAGEZERO left out (it maps nothing
 * and starts at 0). Returns 1 and sets *lowest and *end (one past the last
 * byte), or 0, leaving both alone, when the image has no such segment. */
int portMacImageRange(uintptr_t *lowest, uintptr_t *end);

/* Sleeps until CLOCK_MONOTONIC reaches *target. Returns 0 when it has (at
 * once for a time already past) and EINTR when a signal ended the sleep
 * early: what clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, target, NULL)
 * returns on Linux, so a caller's retry loop is the same on both. */
int portMacSleepUntil(const struct timespec *target);

/* The running executable's real path (MAC-002): dyld's path for it with
 * symlinks resolved, NUL-terminated, in buffer. Returns its length, or 0,
 * leaving buffer unchanged, when it does not fit in size bytes or cannot be
 * read. macOS has no /proc/self/exe; the data root and the restart after a
 * save import start from this. */
size_t portMacExecutablePath(char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif
