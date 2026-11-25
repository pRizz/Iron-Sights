#define _GNU_SOURCE /* port_lp64.c: dl_iterate_phdr, which must precede every include */
#include <sys/mman.h>
#include <sys/types.h>
/* pcsched.c's mappings go through testMmap, which can refuse them (below). */
static void *testMmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
#define mmap testMmap
#include "port/src/pcsched.c"
#undef mmap

static int s_RefuseMaps;

static void *testMmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    if (s_RefuseMaps)
    {
        return MAP_FAILED;
    }
    return mmap(addr, length, prot, flags, fd, offset);
}
#if IS_64_BIT
#include "port/src/port_lp64.c"
#include "port/src/port_lp64_notice.c"
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include "test_buffer_helpers.h"

#if IS_64_BIT
/* video.c's box, which the allocator's stops open (LP64-007 Task 6): a child
 * writes what it would show down a pipe to the parent. */
static int s_BoxFd = -1;

void portShowErrorBox(const char *title, const char *text)
{
    if (s_BoxFd >= 0)
    {
        dprintf(s_BoxFd, "%s: %s", title, text);
    }
}

/* A mapping the system refuses. */
static void *allocateRefused(u32 size)
{
    s_RefuseMaps = 1;
    return portAllocHighDedicated(size);
}

/* Runs the request in a child that someone could read a box in; it must
 * exit 1 having shown WANT. */
static void assertRequestShowsBox(void *(*allocate)(u32), u32 size, const char *want)
{
    int fds[2];
    int status = 0;
    char got[512];
    size_t used = 0;
    ssize_t n;
    pid_t child;

    assert(pipe(fds) == 0);
    child = fork();
    assert(child >= 0);
    if (child == 0)
    {
        close(fds[0]);
        s_BoxFd = fds[1];
        unsetenv("GE_NOINPUT");
        unsetenv("SDL_VIDEODRIVER");
        allocate(size);
        _exit(0); /* reached only if the request returned */
    }
    close(fds[1]);
    while ((n = read(fds[0], got + used, sizeof(got) - 1 - used)) > 0)
    {
        used += (size_t) n;
    }
    got[used] = '\0';
    close(fds[0]);
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    if (strstr(got, want) == NULL)
    {
        fprintf(stderr, "test_high_alloc: box '%s' lacks '%s'\n", got, want);
        abort();
    }
}
#endif

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
    assertRequestShowsBox(portAllocHigh, 0xffffffffu,
                          "Iron Sights: ge: high allocation: a request of 4294967295 bytes is too large");
    assertRequestShowsBox(allocateRefused, 256u << 20,
                          "Iron Sights: ge: high allocation: could not map 268435456 bytes above 4 GB");
#else
    assert(portAllocHigh(0xffffffffu) == NULL);
    assert(portAllocHigh(0xfffffff1u) == NULL);
    assert(portAllocHighDedicated(0xffffffffu) == NULL);
    assert(portAllocHighDedicated(0xffff0001u) == NULL);
#endif
    puts("High-address allocator alignment checks passed");
    return 0;
}
