/* ROM-free checks of the room cache's part key (port/fast3d/gfx_roomcache_key.h)
 * at each pointer width (LP64-005 Task 11). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "port/fast3d/gfx_roomcache_key.h"

#if IS_64_BIT
/* Two list addresses that differ only in BIT, with one vertex base, must not
 * share a key. */
static void checkListsDifferingInBitGetTheirOwnKeys(uintptr_t list, int bit)
{
    /* Arrange */
    const uintptr_t low = list & ~((uintptr_t) 1 << bit);
    const uintptr_t high = list | ((uintptr_t) 1 << bit);
    const void *vtx = (const void *) (uintptr_t) 0x0000b6030d20ull;
    /* Act */
    const uint64_t a = roc_part_key((const void *) low, vtx);
    const uint64_t b = roc_part_key((const void *) high, vtx);
    /* Assert */
    if (a == b) {
        fprintf(stderr, "test_roomcache_key: lists %#llx and %#llx share key %#llx\n",
                (unsigned long long) low, (unsigned long long) high, (unsigned long long) a);
        abort();
    }
}

static void testListsDifferingInBit44GetTheirOwnKeys(void)
{
    /* 16 TiB apart: the lowest bit the old shift by 20 dropped */
    checkListsDifferingInBitGetTheirOwnKeys((uintptr_t) 0x0000a18e3f98ull, 44);
}

static void testListsDifferingInBit47GetTheirOwnKeys(void)
{
    /* the top of the 48-bit user range, set in every part list seen on
     * aarch64 (0xffff7d200000 to 0xffff7d257f80 in the Dam run) */
    checkListsDifferingInBitGetTheirOwnKeys((uintptr_t) 0xffff7d257f80ull, 47);
}

static void testThe64BitKeyIsTheRotatedList(void)
{
    /* Arrange: a list and a vertex base like the Dam run's */
    const uintptr_t dl = (uintptr_t) 0xffff7d257f80ull, vtx = (uintptr_t) 0xffff6a1c0010ull;
    /* Act */
    const uint64_t key = roc_part_key((const void *) dl, (const void *) vtx);
    /* Assert: the list rotated left by 20, XOR the vertex base */
    if (key != 0xfff72da8921c001full) {
        fprintf(stderr, "test_roomcache_key: the 64-bit key is %#llx, expected 0xfff72da8921c001f\n",
                (unsigned long long) key);
        abort();
    }
}
#else
static void testThe32BitKeyIsTheShiftedList(void)
{
    /* Arrange: 32-bit addresses, which the shift by 20 keeps whole */
    const uintptr_t dl = (uintptr_t) 0x0812f3a0u, vtx = (uintptr_t) 0x09a0c010u;
    /* Act */
    const uint64_t key = roc_part_key((const void *) dl, (const void *) vtx);
    /* Assert: the list shifted left by 20, XOR the vertex base, as before LP64-005 */
    if (key != 0x812f33a0c010ull) {
        fprintf(stderr, "test_roomcache_key: the 32-bit key is %#llx, expected 0x812f33a0c010\n",
                (unsigned long long) key);
        abort();
    }
}
#endif

int main(void)
{
#if IS_64_BIT
    testListsDifferingInBit44GetTheirOwnKeys();
    testListsDifferingInBit47GetTheirOwnKeys();
    testThe64BitKeyIsTheRotatedList();
#else
    testThe32BitKeyIsTheShiftedList();
#endif
    printf("test_roomcache_key: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
