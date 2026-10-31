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
    puts("Memory bank range, overflow and resize checks passed");
    return 0;
}
