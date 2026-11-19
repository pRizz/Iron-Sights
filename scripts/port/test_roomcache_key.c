/* ROM-free checks of the room cache's part key (port/fast3d/gfx_roomcache_key.h)
 * at each pointer width (LP64-005 Task 11). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "port/fast3d/gfx_roomcache_key.h"

static void testListsDifferingAboveBit43GetTheirOwnKeys(void)
{
    /* Arrange: two list addresses a 64-bit process can hold that differ only
     * in bit 44 (16 TiB apart), with one vertex base */
    const uintptr_t low = (uintptr_t) 0x0000a18e3f98ull;
    const uintptr_t high = low | ((uintptr_t) 1 << (IS_64_BIT ? 44 : 0));
    const void *vtx = (const void *) (uintptr_t) 0x0000b6030d20ull;
    /* Act */
    const uint64_t a = roc_part_key((const void *) low, vtx);
    const uint64_t b = roc_part_key((const void *) high, vtx);
    /* Assert */
    if (IS_64_BIT && a == b) {
        fprintf(stderr, "test_roomcache_key: lists %#llx and %#llx share key %#llx\n",
                (unsigned long long) low, (unsigned long long) high, (unsigned long long) a);
        abort();
    }
}

static void testThe32BitKeyIsUnchanged(void)
{
    /* Arrange: 32-bit addresses, which the shift by 20 keeps whole */
    const uintptr_t dl = (uintptr_t) 0x0812f3a0u, vtx = (uintptr_t) 0x09a0c010u;
    /* Act */
    const uint64_t key = roc_part_key((const void *) dl, (const void *) vtx);
    /* Assert */
    if (!IS_64_BIT && key != (((uint64_t) dl << 20) ^ (uint64_t) vtx)) {
        fprintf(stderr, "test_roomcache_key: the 32-bit key changed: %#llx\n", (unsigned long long) key);
        abort();
    }
}

int main(void)
{
    testListsDifferingAboveBit43GetTheirOwnKeys();
    testThe32BitKeyIsUnchanged();
    printf("test_roomcache_key: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
