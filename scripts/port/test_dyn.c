/* ROM-free integration checks for the native frame allocator. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "src/game/dyn.c"
#include "port/src/port_dyn.c"

static jmp_buf failure;
void sysFatalError(const char *format, ...) { (void)format; longjmp(failure, 1); }
void osSyncPrintf(const char *format, ...)
{
    va_list args; va_start(args, format); vfprintf(stderr, format, args); va_end(args); abort();
}

static void rejected(s32 count, u32 bytes)
{
    u8 *before = g_GfxMemPos;
    if (!setjmp(failure)) { portDynAllocate(count, bytes, 1); assert(!"Allocation should fail"); }
    assert(g_GfxMemPos == before);
}

int main(void)
{
    /* Arrange: two small frame halves followed by texture sentinels. */
    u8 storage[256]; memset(storage, 0xa5, sizeof(storage));
    g_VtxBuffers[0] = storage; g_VtxBuffers[1] = storage + 64; g_VtxBuffers[2] = storage + 128;
    g_GfxActiveBufferIndex = 0; g_GfxMemPos = storage;

    /* Act/assert: capacity covers generated arms, preserving larger budgets. */
    for (s32 players = 1; players <= 4; players++)
        assert(portDynVertexCapacity(81920, players) == 256 * 1024 * players);
    assert(portDynVertexCapacity(2 * 1024 * 1024, 1) == 2 * 1024 * 1024);

    /* Act/assert: original alignment and all four allocation entry points. */
    assert(dynAllocate(1) == storage); assert(g_GfxMemPos == storage + 16);
    assert(dynAllocateVertices(1) == storage + 16);
    assert(dynAllocateLights(1) == storage + 32);
    g_GfxMemPos = storage;
    assert(dynAllocateMatrix() == storage); assert(g_GfxMemPos == storage + 64);
    rejected(1, 1); rejected(-1, 1); rejected(0x7fffffff, sizeof(Vtx));

    /* Act/assert: swapping resets only the active half; overflow cannot touch textures. */
    dynSwapBuffers(); assert(g_GfxMemPos == storage + 64);
    assert(dynAllocate(64) == storage + 64); rejected(1, 1);
    for (s32 i = 128; i < 256; i++) assert(storage[i] == 0xa5);
    puts("Native frame allocation checks passed");
    return 0;
}
