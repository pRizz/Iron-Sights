/* Overflow-safe native bank allocation; the N64 matching path stays separate. */
#include <stdint.h>
#include "port_memp.h"
#include "port_poison.h"
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

/* enum MEMPOOL's names (memp.h), for the exhaustion message. */
static const char *bankName(u8 bank)
{
    static const char *const names[MEMPOOL_COUNT] = {
        "total", "mf", "2", "ml", "stage", "me", "permanent",
    };
    return bank < MEMPOOL_COUNT ? names[bank] : "invalid";
}

#if IS_64_BIT
/* LP64-004: at 64 bits every bank cursor stays 8-aligned from its bank's
 * start (each bank starts 16-aligned, memp.c's mempSetBankStarts), so every
 * block starts 8-aligned and records holding pointers can sit in any block.
 * The cursor moves past a block's end to the next multiple of 8, never past
 * the bank's end; a block starts exactly where the cursor is, so a request
 * for mempGetBankSizeLeft's bytes still fits exactly. The N64 and 32-bit
 * allocator round nothing; callers round their own requests (ALIGN16). */
static u8 *cursorAfter(const MemoryPool *pool, u8 *blockEnd)
{
    const uintptr_t used = (uintptr_t)blockEnd - (uintptr_t)pool->start;
    const uintptr_t room = (uintptr_t)pool->end - (uintptr_t)pool->start;
    const uintptr_t aligned = (used + 7) & ~(uintptr_t)7;
    return pool->start + (aligned < room ? aligned : room);
}
#endif

#if PORT_POISON_GAP
/* Sanitizer builds only (port_poison.h): the cursor moves on past a poisoned
 * gap after each block, so the next block starts after it and a write past a
 * block's end lands on poison. The gap is dropped at the bank's end, as the
 * 8-byte rounding is: a request mempGetBankSizeLeft promised still fits, and
 * a sanitizer build runs out of no bank a plain build fits in. The gap keeps
 * the 64-bit cursor 8-aligned (32 is a multiple of 8; the end is 16-aligned). */
static u8 *gapAfter(const MemoryPool *pool, u8 *cursor)
{
    const uintptr_t room = (uintptr_t)pool->end - (uintptr_t)cursor;
    return cursor + (room < PORT_POISON_GAP ? room : PORT_POISON_GAP);
}
#endif

static u32 remaining(const MemoryPool *pool)
{
    const uintptr_t start = (uintptr_t)pool->start, pos = (uintptr_t)pool->pos, end = (uintptr_t)pool->end;
    if (!pos || pos < start || pos > end) {
        sysFatalError("Invalid memory bank cursor");
        return 0;
    }
    return (u32) (end - pos);
}

void *portMempAllocate(u32 bytes, u8 bank)
{
    MemoryPool *pool = portMempPool(bank);
    if (bytes > remaining(pool)) {
        pool = portMempPool(MEMPOOL_PERMANENT);
        if (bytes > remaining(pool)) {
            sysFatalError("Memory bank exhausted (bank %u \"%s\", request %u bytes)", bank, bankName(bank), bytes);
            return NULL;
        }
        needmemallocation = TRUE;
    }
    pool->prevpos = pool->pos;
#if IS_64_BIT
    pool->pos = cursorAfter(pool, pool->pos + bytes);
#else
    pool->pos += bytes;
#endif
#if PORT_POISON_GAP
    pool->pos = gapAfter(pool, pool->pos);
#endif
    /* Everything from the cursor to the bank's end is poisoned (mempResetBank,
     * and the resize below); the block is exactly the bytes asked for. */
    PORT_UNPOISON(pool->prevpos, bytes);
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
    /* A shrink gives back the bytes past the new size; a growth takes the
     * bytes up to it. Both leave everything past the block poisoned again. */
    PORT_POISON(allocation, (uintptr_t)pool->pos - (uintptr_t)allocation);
    PORT_UNPOISON(allocation, size);
#if IS_64_BIT
    pool->pos = cursorAfter(pool, (u8 *)allocation + size);
#else
    pool->pos = (u8 *)allocation + size;
#endif
#if PORT_POISON_GAP
    pool->pos = gapAfter(pool, pool->pos);
#endif
    return MEMP_ADD_ENTRY_SUCCESS;
}
