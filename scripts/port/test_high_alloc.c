#define _GNU_SOURCE /* port_lp64.c: dl_iterate_phdr, which must precede every include */
#include "port/src/pcsched.c"
#if IS_64_BIT
#include "port/src/port_lp64.c"
#define PORT_LP64_NOTICE_HEADLESS /* port_lp64.c's stops: no box here */
#include "port/src/port_lp64_notice.c"
#include <sys/wait.h>
#include <unistd.h>
#endif
#include "test_buffer_helpers.h"

#if IS_64_BIT
/* At 64 bits a request the allocator cannot map stops the game (exit 1)
 * instead of returning NULL (LP64-003). Runs the request in a child. */
static void assertRequestExits(void *(*allocate)(u32), u32 size)
{
    int status = 0;
    pid_t child = fork();

    assert(child >= 0);
    if (child == 0)
    {
        allocate(size);
        _exit(0); /* reached only if the request returned */
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
}
#endif

int main(void)
{
    /* Oversized alignment must fail before attempting an OS mapping. */
#if IS_64_BIT
    assertRequestExits(portAllocHigh, 0xffffffffu);
    assertRequestExits(portAllocHigh, 0xfffffff1u);
    assertRequestExits(portAllocHighDedicated, 0xffffffffu);
    assertRequestExits(portAllocHighDedicated, 0xffff0001u);
#else
    assert(portAllocHigh(0xffffffffu) == NULL);
    assert(portAllocHigh(0xfffffff1u) == NULL);
    assert(portAllocHighDedicated(0xffffffffu) == NULL);
    assert(portAllocHighDedicated(0xffff0001u) == NULL);
#endif
    puts("High-address allocator alignment checks passed");
    return 0;
}
