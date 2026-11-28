/* macOS host functions (MAC-001). See port/include/port_macos.h for the
 * contracts and scripts/port/test_macos_host.sh for their tests. CMake
 * compiles this file only when APPLE is true, with -Wall -Wextra -Werror. */
#include "port_macos.h"

#include <errno.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslimits.h>

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
        /* memcmp, not strncmp: in the game's build <string.h> is the game's
         * own short header (include/string.h). The name with its NUL is 11
         * of segname's 16 bytes. */
        if (segment->cmd != LC_SEGMENT_64 || memcmp(segment->segname, SEG_PAGEZERO, sizeof SEG_PAGEZERO) == 0)
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

size_t portMacExecutablePath(char *buffer, size_t size)
{
    char raw[PATH_MAX];
    char real[PATH_MAX];
    uint32_t rawSize = sizeof(raw);
    size_t length = 0;

    /* dyld's path may be relative or name a symlink; realpath makes it the
     * file's own. */
    if (_NSGetExecutablePath(raw, &rawSize) != 0 || realpath(raw, real) == NULL)
    {
        return 0;
    }
    /* Counted here, not with strlen: in the game's build <string.h> is the
     * game's own short header, whose strlen takes unsigned chars. */
    while (real[length] != '\0')
    {
        length++;
    }
    if (length + 1 > size)
    {
        return 0;
    }
    memcpy(buffer, real, length + 1);
    return length;
}
