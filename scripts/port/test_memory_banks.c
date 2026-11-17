#include <setjmp.h>
#include "src/memp.c"
#include "port/src/port_memp.c"
#include "test_buffer_helpers.h"
static jmp_buf failure;
void sysFatalError(const char *format, ...) { (void)format; longjmp(failure, 1); }
void portTexCrosshairForget(const void *low, const void *high) { (void)low; (void)high; }

static void refused(u32 bytes, u8 bank)
{
    u8 *before = g_mempPools[MEMPOOL_STAGE].pos;
    if (!setjmp(failure)) { mempAllocBytesInBank(bytes, bank); assert(!"Expected rejection"); }
    assert(before == g_mempPools[MEMPOOL_STAGE].pos);
}

int main(void)
{
    /* Arrange: separate stage/permanent pools followed by sentinels. */
    u8 storage[256]; memset(storage, 0x55, sizeof(storage));
    g_mempPools[MEMPOOL_STAGE] = (MemoryPool){storage, storage, storage+64, NULL};
    g_mempPools[MEMPOOL_PERMANENT] = (MemoryPool){storage+64, storage+64, storage+128, NULL};
    /* Act/assert: exact fit and the inherited permanent-bank fallback. */
    assert(mempAllocBytesInBank(64, MEMPOOL_STAGE) == storage);
    assert(mempAllocBytesInBank(16, MEMPOOL_STAGE) == storage+64);
    assert(needmemallocation);
    assert(mempAddEntryOfSizeToBank(storage+64, 32, MEMPOOL_STAGE) == MEMP_ADD_ENTRY_SUCCESS);
    assert(g_mempPools[MEMPOOL_PERMANENT].pos == storage+96);
    refused(0xffffffffu, MEMPOOL_STAGE); refused(1, 255);
    if (!setjmp(failure)) { mempAddEntryOfSizeToBank(storage+64, -1, MEMPOOL_STAGE); assert(!"Expected rejection"); }
    if (!setjmp(failure)) { mempAddEntryOfSizeToBank(storage+64, 65, MEMPOOL_STAGE); assert(!"Expected rejection"); }
    for (s32 i=128; i<256; i++) assert(storage[i] == 0x55);

    /* Arrange: a fresh stage bank whose end is the next one's start. */
    needmemallocation = FALSE;
    g_mempPools[MEMPOOL_STAGE] = (MemoryPool){storage, storage, storage+128, NULL};
    g_mempPools[MEMPOOL_PERMANENT] = (MemoryPool){storage+128, storage+128, storage+128, NULL};
    /* Act: an odd block, then the next. Assert: at 64 bits the next starts
     * on the bank's next 8-byte boundary (LP64-004); at 32 bits it starts
     * where the odd block ended, as on the N64. */
    assert(mempAllocBytesInBank(13, MEMPOOL_STAGE) == storage);
    {
        u8 *next = mempAllocBytesInBank(4, MEMPOOL_STAGE);
        assert(next == storage + (IS_64_BIT ? 16 : 13));
        /* Act: trim the last block to an odd size. Assert: the cursor is
         * rounded the same way, and the trimmed block keeps its start. */
        assert(mempAddEntryOfSizeToBank(next, 3, MEMPOOL_STAGE) == MEMP_ADD_ENTRY_SUCCESS);
        assert(g_mempPools[MEMPOOL_STAGE].pos == storage + (IS_64_BIT ? 24 : 16));
    }
    /* Act: a load that takes the whole remainder (ob.c's fileIndexLoadToBank).
     * Assert: it gets exactly the bytes promised and stays in the bank. */
    {
        s32 left = mempGetBankSizeLeft(MEMPOOL_STAGE);
        u8 *whole = mempAllocBytesInBank((u32)left, MEMPOOL_STAGE);
        assert(left == (IS_64_BIT ? 104 : 112));
        assert(!needmemallocation && whole + left == storage + 128);
        assert(g_mempPools[MEMPOOL_STAGE].pos == storage + 128);
    }
    for (s32 i=128; i<256; i++) assert(storage[i] == 0x55);
    puts("Memory bank range, overflow and resize checks passed");
    return 0;
}
