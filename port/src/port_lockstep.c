/**
 * Lockstep's switch and clock (LP64-006 Task 2; port_lockstep.h says what
 * lockstep changes and where). Compiled where PORT_PLAY_HARNESS is 1; CMake
 * leaves this file out of a 32-bit build with video, the shipped kind.
 */
#include <stdio.h>
#include <stdlib.h>
#include "port_lockstep.h"

#if PORT_PLAY_HARNESS
#define PORT_LOCKSTEP_FIELDS_A_SECOND 60u
#define PORT_LOCKSTEP_MICROSECONDS_A_SECOND 1000000u

extern s32 get_is_ramrom_flag(void);

static u32 sRetraces;

s32 portLockstep(void)
{
    static s32 on = -1;

    if (on < 0)
    {
        on = getenv("GE_LOCKSTEP") != NULL;
        if (on)
        {
            printf("ge: lockstep: on\n");
            fflush(stdout);
        }
    }
    return on;
}

u64 portLockstepMicroseconds(void)
{
    return (u64) sRetraces * PORT_LOCKSTEP_MICROSECONDS_A_SECOND / PORT_LOCKSTEP_FIELDS_A_SECOND;
}

void portLockstepRetrace(void)
{
    if (get_is_ramrom_flag() != 0)
    {
        fprintf(stderr, "ge: FATAL: GE_LOCKSTEP cannot replay a demo\n");
        fflush(NULL);
        exit(1);
    }
    sRetraces++;
}
#endif
