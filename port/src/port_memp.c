/* Overflow-safe native bank allocation; the N64 matching path stays separate. */
#include <stdint.h>
#include "port_memp.h"
extern MemoryPool g_mempPools[MEMPOOL_COUNT];
extern s32 needmemallocation;
extern void sysFatalError(const char *format, ...);

MemoryPool *portMempPool(u8 bank)
{
    if (bank >= MEMPOOL_COUNT) {
        sysFatalError("Invalid memory bank %u", bank);
        return NULL;
    }
    return g_mempPools + bank;
}

static u32 remaining(const MemoryPool *pool)
{
    const uintptr_t start = (uintptr_t)pool->start, pos = (uintptr_t)pool->pos, end = (uintptr_t)pool->end;
    if (!pos || pos < start || pos > end) {
        sysFatalError("Invalid memory bank cursor");
        return 0;
    }
    return end - pos;
}

void *portMempAllocate(u32 bytes, u8 bank)
{
    MemoryPool *pool = portMempPool(bank);
    if (bytes > remaining(pool)) {
        pool = portMempPool(MEMPOOL_PERMANENT);
        if (bytes > remaining(pool)) {
            sysFatalError("Memory bank exhausted (bank %u, request %u bytes)", bank, bytes);
            return NULL;
        }
        needmemallocation = TRUE;
    }
    pool->prevpos = pool->pos;
    pool->pos += bytes;
    return pool->prevpos;
}

MEMP_ADD_ENTRY_RESULT portMempResize(void *allocation, s32 size, u8 bank)
{
    MemoryPool *pool = portMempPool(bank);
    if (needmemallocation && allocation == g_mempPools[MEMPOOL_PERMANENT].prevpos)
        pool = portMempPool(MEMPOOL_PERMANENT);
    remaining(pool);
    if (allocation != pool->prevpos) return MEMP_ADD_ENTRY_NOT_LAST_ALLOCATION;
    if (size < 0 || (uintptr_t)allocation < (uintptr_t)pool->start
        || (uintptr_t)allocation > (uintptr_t)pool->pos
        || (u32)size > (uintptr_t)pool->end - (uintptr_t)allocation) {
        sysFatalError("Memory bank resize exceeds its allocation range");
        return MEMP_ADD_ENTRY_NOT_LAST_ALLOCATION;
    }
    pool->pos = (u8 *)allocation + size;
    return MEMP_ADD_ENTRY_SUCCESS;
}
