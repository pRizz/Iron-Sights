/* macOS host functions (MAC-001). See port/include/port_macos.h for the
 * contracts and scripts/port/test_macos_host.sh for their tests. CMake
 * compiles this file only when APPLE is true, with -Wall -Wextra -Werror. */
#include "port_macos.h"

#include <errno.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <string.h>

#define NS_PER_S 1000000000LL

int portMacImageRange(uintptr_t *lowest, uintptr_t *end)
{
    /* Image 0 is the main executable. */
    const struct mach_header_64 *header = (const struct mach_header_64 *) _dyld_get_image_header(0);
    uintptr_t slide = (uintptr_t) _dyld_get_image_vmaddr_slide(0);
    const struct load_command *command;
    uintptr_t low = 0;
    uintptr_t high = 0;
    int found = 0;
    uint32_t i;

    if (header == NULL || header->magic != MH_MAGIC_64)
    {
        return 0;
    }
    command = (const struct load_command *) (header + 1);
    for (i = 0; i < header->ncmds; i++)
    {
        const struct segment_command_64 *segment = (const struct segment_command_64 *) command;

        command = (const struct load_command *) ((const char *) command + command->cmdsize);
        if (segment->cmd != LC_SEGMENT_64 || strncmp(segment->segname, SEG_PAGEZERO, sizeof segment->segname) == 0)
        {
            continue;
        }
        {
            uintptr_t start = slide + (uintptr_t) segment->vmaddr;
            uintptr_t stop = start + (uintptr_t) segment->vmsize;

            if (!found || start < low)
            {
                low = start;
            }
            if (!found || stop > high)
            {
                high = stop;
            }
            found = 1;
        }
    }
    if (!found)
    {
        return 0;
    }
    *lowest = low;
    *end = high;
    return 1;
}

int portMacSleepUntil(const struct timespec *target)
{
    /* macOS has no clock_nanosleep. Sleep for what is left and look again:
     * nanosleep counts on another clock than CLOCK_MONOTONIC, so one sleep
     * of the difference can end a little short of the target. */
    for (;;)
    {
        struct timespec now;
        struct timespec left;
        long long ns;

        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        {
            return errno;
        }
        ns = (long long) (target->tv_sec - now.tv_sec) * NS_PER_S + (target->tv_nsec - now.tv_nsec);
        if (ns <= 0)
        {
            return 0;
        }
        left.tv_sec = (time_t) (ns / NS_PER_S);
        left.tv_nsec = (long) (ns % NS_PER_S);
        if (nanosleep(&left, NULL) != 0)
        {
            return errno;
        }
    }
}
