/* Native frame-pool headroom and a guard before any auxiliary-data write. */
#include <stdint.h>
#include "port_dyn.h"

extern u8 *g_VtxBuffers[3], *g_GfxMemPos;
extern u8 g_GfxActiveBufferIndex;
extern void sysFatalError(const char *format, ...);

s32 portDynVertexCapacity(s32 inheritedBytes, s32 players)
{
    const s32 minimum = 256 * 1024 * players;
    return inheritedBytes < minimum ? minimum : inheritedBytes;
}

void *portDynAllocate(s32 count, u32 elementBytes, u32 alignment)
{
    const uintptr_t pos = (uintptr_t)g_GfxMemPos;
    const uintptr_t start = (uintptr_t)g_VtxBuffers[g_GfxActiveBufferIndex];
    const uintptr_t end = (uintptr_t)g_VtxBuffers[g_GfxActiveBufferIndex + 1];
    const u64 bytes = ((u64)(u32)count * elementBytes + alignment - 1) & ~(u64)(alignment - 1);
    if (count < 0 || pos < start || pos > end || bytes > end - pos) {
        sysFatalError("Frame data buffer exhausted (request %u x %u, remaining %u bytes)",
            (u32)count, elementBytes, pos <= end ? (u32)(end - pos) : 0);
        return NULL; /* The fatal handler terminates; never advance into textures. */
    }
    g_GfxMemPos += (u32)bytes;
    return (void *)pos;
}
