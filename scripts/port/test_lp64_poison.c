/* LP64-005 Task 2: in a sanitizer build the game's arenas poison what they
 * have not handed out (port/include/port_poison.h), so a write one byte past
 * a block of the stage bank, the room heap, the music heap or the graphics
 * pool dies with use-after-poison, and a write inside a block does not.
 * Each probe runs in its own child process; test_buffers.sh builds this file
 * only in its sanitized half. The arenas are invented here, no ROM. */
#include "port_poison.h"
#include "src/memp.c"
#include "port/src/port_memp.c"
#include "src/mema.c"
#include "src/libultra/audio/heapinit.c"
#include "src/libultra/audio/heapalloc.c"
#include "src/game/dyn.c"
#include "port/src/port_dyn.c"
#include "test_buffer_helpers.h"
char *strstr(const char *haystack, const char *needle);
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if !defined(__SANITIZE_ADDRESS__)
#error "test_lp64_poison.c checks a sanitizer build: compile it with -fsanitize=address"
#endif

void sysFatalError(const char *format, ...) { fprintf(stderr, "fatal: %s\n", format); abort(); }
void portTexCrosshairForget(const void *low, const void *high) { (void)low; (void)high; }
void debTryAdd(void *data, const char *name) { (void)data; (void)name; }
s32 portRoomAuditEnabled(void) { return 0; }
void portRoomAuditOnHeapReset(void) {}
const char *tokenFind(s32 index, const char *token) { (void)index; (void)token; return NULL; }
s32 getPlayerCount(void) { return 1; }
#if IS_64_BIT
void portLp64PoolFatal(const char *pool, u32 request) { fprintf(stderr, "pool %s %u\n", pool, request); abort(); }
#endif

#define BLOCK 0x14
static u8 s_Stage[0x100] __attribute__((aligned(16)));
static u8 s_Permanent[0x40] __attribute__((aligned(16)));
static u8 s_Heap[0x100] __attribute__((aligned(16)));
static u8 s_Vtx[0x200] __attribute__((aligned(16)));

/* A stage bank of the given size and a small permanent bank, both reset as
 * boss.c resets them, so both start wholly poisoned. */
static void makeBanks(u32 stageBytes)
{
    g_mempPools[MEMPOOL_STAGE] = (MemoryPool){s_Stage, s_Stage, s_Stage + stageBytes, NULL};
    g_mempPools[MEMPOOL_PERMANENT] = (MemoryPool){s_Permanent, s_Permanent, s_Permanent + sizeof(s_Permanent), NULL};
    needmemallocation = FALSE;
    mempResetBank(MEMPOOL_STAGE);
    mempResetBank(MEMPOOL_PERMANENT);
}

static void touch(u8 *block, u32 offset) { ((volatile u8 *)block)[offset] = 1; }

static void bankPastFirstBlock(void)
{
    u8 *first;
    makeBanks(sizeof(s_Stage));
    first = mempAllocBytesInBank(BLOCK, MEMPOOL_STAGE);
    mempAllocBytesInBank(BLOCK, MEMPOOL_STAGE);
    touch(first, BLOCK);
}

static void bankInsideBothBlocks(void)
{
    u8 *first, *second;
    makeBanks(sizeof(s_Stage));
    first = mempAllocBytesInBank(BLOCK, MEMPOOL_STAGE);
    second = mempAllocBytesInBank(BLOCK, MEMPOOL_STAGE);
    assert(second >= first + BLOCK + PORT_POISON_GAP);
    for (u32 i = 0; i < BLOCK; i++) { touch(first, i); touch(second, i); }
}

/* ob.c's load: take the whole remainder, fill it, then trim it to the file.
 * The block offered and the block committed agree, the trim finds it to be
 * the bank's last, and the next block starts past a poisoned gap. */
static u8 *loadIntoRemainder(void)
{
    s32 left;
    u8 *whole, *next;
    makeBanks(sizeof(s_Stage));
    mempAllocBytesInBank(BLOCK, MEMPOOL_STAGE);
    left = mempGetBankSizeLeft(MEMPOOL_STAGE);
    whole = mempAllocBytesInBank((u32)left, MEMPOOL_STAGE);
    assert(whole + left == s_Stage + sizeof(s_Stage) && !needmemallocation);
    touch(whole, (u32)left - 1);
    assert(mempAddEntryOfSizeToBank(whole, BLOCK, MEMPOOL_STAGE) == MEMP_ADD_ENTRY_SUCCESS);
    touch(whole, BLOCK - 1);
    next = mempAllocBytesInBank(BLOCK, MEMPOOL_STAGE);
    assert(next >= whole + BLOCK + PORT_POISON_GAP && !needmemallocation);
    touch(next, 0);
    touch(next, BLOCK - 1);
    return whole;
}

static void remainderLoadInside(void) { loadIntoRemainder(); }
static void remainderPastTrim(void) { touch(loadIntoRemainder(), BLOCK); }

/* A block that fits the bank without the gap but not with it stays in the
 * bank: the gap is dropped at the bank's end, as the 8-byte rounding is. */
static void bankNearlyFull(void)
{
    s32 left;
    u8 *last;
    makeBanks(0x40);
    mempAllocBytesInBank(BLOCK, MEMPOOL_STAGE);
    left = mempGetBankSizeLeft(MEMPOOL_STAGE);
    assert(left > 0 && left < PORT_POISON_GAP);
    last = mempAllocBytesInBank((u32)left, MEMPOOL_STAGE);
    assert(last + left == s_Stage + 0x40 && !needmemallocation);
    assert(g_mempPools[MEMPOOL_STAGE].pos == s_Stage + 0x40);
    touch(last, (u32)left - 1);
}

/* portMempAllocate's fall-back: a block the stage bank cannot hold comes
 * from the permanent bank, unpoisoned there, with poison after it. */
static u8 *permanentFallback(void)
{
    u8 *block;
    makeBanks(0x40);
    mempAllocBytesInBank(0x30, MEMPOOL_STAGE);
    block = mempAllocBytesInBank(BLOCK, MEMPOOL_STAGE);
    assert(needmemallocation && block == s_Permanent);
    touch(block, 0);
    touch(block, BLOCK - 1);
    return block;
}

static void bankFallbackInside(void) { permanentFallback(); }
static void bankFallbackPast(void) { touch(permanentFallback(), BLOCK); }

static void roomHeapPastBlock(void)
{
    memaReset(s_Heap, sizeof(s_Heap));
    touch(memaAlloc(BLOCK), BLOCK);
}

static void roomHeapAfterFree(void)
{
    u8 *block;
    memaReset(s_Heap, sizeof(s_Heap));
    block = memaAlloc(BLOCK);
    memaFree(block, BLOCK);
    touch(block, 0);
}

static void roomHeapInside(void)
{
    u8 *block;
    memaReset(s_Heap, sizeof(s_Heap));
    block = memaAlloc(BLOCK);
    touch(block, 0);
    touch(block, BLOCK - 1);
}

/* memaRealloc grows a block into the free space after it (memaGrow) and
 * shrinks one by freeing its tail. */
static u8 *roomHeapReallocated(u32 newsize)
{
    u8 *block;
    memaReset(s_Heap, sizeof(s_Heap));
    block = memaAlloc(BLOCK);
    assert(memaRealloc((intptr_t)block, BLOCK, newsize) == 1);
    touch(block, 0);
    touch(block, newsize - 1);
    return block;
}

static void roomHeapGrown(void) { roomHeapReallocated(2 * BLOCK); }
static void roomHeapShrunk(void) { touch(roomHeapReallocated(8), 8); }

/* A music heap block is whole 16-byte cache lines: callers DMA whole lines
 * into it (music.c copies ALIGN16 of its request), so the block ends at the
 * request rounded up to 16. */
#define MUSIC_BLOCK ((BLOCK + 15) & ~15)

static void musicHeapPastBlock(void)
{
    ALHeap heap;
    alHeapInit(&heap, s_Heap, sizeof(s_Heap));
    touch(alHeapAlloc(&heap, 1, BLOCK), MUSIC_BLOCK);
}

static void musicHeapInside(void)
{
    ALHeap heap;
    u8 *block;
    alHeapInit(&heap, s_Heap, sizeof(s_Heap));
    block = alHeapAlloc(&heap, 1, BLOCK);
    touch(block, 0);
    touch(block, MUSIC_BLOCK - 1);
}

static void makeGraphicsPool(void)
{
    g_VtxBuffers[0] = s_Vtx;
    g_VtxBuffers[1] = s_Vtx + 0x100;
    g_VtxBuffers[2] = s_Vtx + 0x200;
    g_GfxActiveBufferIndex = 0;
    g_GfxMemPos = s_Vtx;
}

static void graphicsPoolPastBlock(void)
{
    makeGraphicsPool();
    dynGetMasterDisplayList();
    touch(dynAllocate(BLOCK), BLOCK);
}

/* The half a frame finished stays readable while the next frame is built:
 * the renderer reads it then (video.c's render thread). */
static void graphicsPoolFinishedHalf(void)
{
    u8 *block;
    makeGraphicsPool();
    dynGetMasterDisplayList();
    block = dynAllocate(BLOCK);
    touch(block, BLOCK - 1);
    dynSwapBuffers();
    dynGetMasterDisplayList();
    touch(dynAllocate(BLOCK), 0);
    assert(((volatile u8 *)block)[BLOCK - 1] == 1);
}

static void graphicsPoolStaleHalf(void)
{
    u8 *block;
    makeGraphicsPool();
    dynGetMasterDisplayList();
    block = dynAllocate(BLOCK);
    dynSwapBuffers();
    dynGetMasterDisplayList();
    dynSwapBuffers();
    dynGetMasterDisplayList();
    touch(block, 0);
}

/* Every probe has a deadline: a child still running after it is killed and
 * the probe fails by name. The parent reads the child's standard error only
 * until the child exits, never waiting for an end of file that a process the
 * child left behind could hold off. */
#define PROBE_SECONDS 10
/* The whole run's bound, which also covers the sanitizer's checks at exit:
 * under qemu-i386, LeakSanitizer's exit check hangs even in an empty main. */
#define RUN_SECONDS 120
#define SECONDS_TEXT(n) #n
#define SECONDS(n) SECONDS_TEXT(n)

static int s_ProbesRun, s_ProbesPassed;

static void probeFailed(const char *name, const char *why, const char *errors)
{
    fprintf(stderr, "FAIL: %s: %s%s%s\n", name, why, errors[0] ? ":\n" : "", errors);
    exit(1);
}

static void runTimedOut(int signum)
{
    static const char message[] =
        "FAIL: test_lp64_poison: timed out after " SECONDS(RUN_SECONDS) " s (the probes, or the sanitizer's checks at exit)\n";
    (void)signum;
    write(2, message, sizeof(message) - 1);
    _exit(1);
}

static long nowMilliseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/* Appends what the pipe holds now; returns 0 at its end, 1 otherwise. */
static int readAvailable(int reader, char *errors, size_t capacity, size_t *used)
{
    ssize_t got = 0;
    while (*used + 1 < capacity && (got = read(reader, errors + *used, capacity - 1 - *used)) > 0) *used += (size_t)got;
    return got != 0;
}

/* Runs probe in a child; returns its wait status and its standard error. */
static int runChild(const char *name, void (*probe)(void), char *errors, size_t capacity)
{
    int pipes[2], status, pipeOpen = 1;
    size_t used = 0;
    long deadline = nowMilliseconds() + PROBE_SECONDS * 1000L;
    pid_t child;
    assert(pipe(pipes) == 0);
    fflush(NULL);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(pipes[0]);
        dup2(pipes[1], 2);
        signal(SIGALRM, SIG_DFL);
        alarm(PROBE_SECONDS);
        probe();
        _exit(0);
    }
    close(pipes[1]);
    assert(fcntl(pipes[0], F_SETFL, O_NONBLOCK) == 0);
    errors[0] = '\0';
    for (;;) {
        struct pollfd ready = {pipes[0], POLLIN, 0};
        pid_t done = waitpid(child, &status, WNOHANG);
        assert(done >= 0);
        if (done == child) break;
        if (nowMilliseconds() >= deadline) {
            kill(child, SIGKILL);
            assert(waitpid(child, &status, 0) == child);
            readAvailable(pipes[0], errors, capacity, &used);
            errors[used] = '\0';
            close(pipes[0]);
            probeFailed(name, "timed out", errors);
        }
        if (pipeOpen && poll(&ready, 1, 20) > 0) pipeOpen = readAvailable(pipes[0], errors, capacity, &used);
        else if (!pipeOpen) poll(NULL, 0, 20);
    }
    readAvailable(pipes[0], errors, capacity, &used);
    errors[used] = '\0';
    close(pipes[0]);
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) probeFailed(name, "timed out", errors);
    return status;
}

static void expectDies(const char *name, void (*probe)(void))
{
    static char errors[1 << 16];
    int status;
    s_ProbesRun++;
    status = runChild(name, probe, errors, sizeof(errors));
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) probeFailed(name, "the child exited 0; nothing reported the write", "");
    if (!strstr(errors, "use-after-poison")) probeFailed(name, "the child died without use-after-poison", errors);
    s_ProbesPassed++;
    printf("%s: died with use-after-poison\n", name);
}

static void expectLives(const char *name, void (*probe)(void))
{
    static char errors[1 << 16];
    char why[48];
    int status;
    s_ProbesRun++;
    status = runChild(name, probe, errors, sizeof(errors));
    snprintf(why, sizeof(why), "the child failed (status %d)", status);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) probeFailed(name, why, errors);
    s_ProbesPassed++;
    printf("%s: exited 0\n", name);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGALRM, runTimedOut);
    alarm(RUN_SECONDS);
    expectDies("bank: one byte past the first of two blocks", bankPastFirstBlock);
    expectLives("bank: inside both blocks", bankInsideBothBlocks);
    expectLives("bank: whole remainder, trimmed, then another block", remainderLoadInside);
    expectDies("bank: one byte past a trimmed remainder", remainderPastTrim);
    expectLives("bank: a block that fits only without the gap", bankNearlyFull);
    expectLives("bank: a block the stage bank cannot hold, in the permanent bank", bankFallbackInside);
    expectDies("bank: one byte past a block in the permanent bank", bankFallbackPast);
    expectDies("room heap: one byte past a block", roomHeapPastBlock);
    expectDies("room heap: a freed block", roomHeapAfterFree);
    expectLives("room heap: inside a block", roomHeapInside);
    expectLives("room heap: inside a block grown by memaRealloc", roomHeapGrown);
    expectDies("room heap: past a block shrunk by memaRealloc", roomHeapShrunk);
    expectDies("music heap: one byte past a block's last cache line", musicHeapPastBlock);
    expectLives("music heap: inside a block's last cache line", musicHeapInside);
    expectDies("graphics pool: one byte past a block", graphicsPoolPastBlock);
    expectLives("graphics pool: the finished half while the next is built", graphicsPoolFinishedHalf);
    expectDies("graphics pool: a block two frames old", graphicsPoolStaleHalf);
    assert(s_ProbesPassed == s_ProbesRun);
    printf("test_lp64_poison: PASS (%d-bit, %d probes)\n", (int)(sizeof(void *) * 8), s_ProbesPassed);
    return 0;
}
