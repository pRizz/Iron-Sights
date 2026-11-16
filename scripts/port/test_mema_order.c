/* Drives the mema allocator (src/mema.c) with made-up heap addresses that
 * straddle 0x80000000, where a 32-bit address changes sign when read as
 * signed. mema never dereferences the addresses it manages, so they need no
 * memory behind them. Built at both pointer widths by
 * scripts/port/test_mema_order.sh.
 *
 * - The free-space list is ordered unsigned (memaDefragPass compares with an
 *   unsigned local): a pass puts the space below 0x80000000 first, and the
 *   next pass merges the two adjacent spaces.
 * - The heap-range check of the GE_ROOM_AUDIT free trace compares signed, as
 *   it has since the decompilation: at 32 bits a free at 0x80008000 in a heap
 *   that starts at 0x7FFF0000 is reported outside the heap.
 * - The list walks stop at the -1 end marker, at both widths. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mema.c"

static s32 s_audit;

s32 portRoomAuditEnabled(void)
{
    return s_audit;
}

void portRoomAuditOnHeapReset(void)
{
}

void debTryAdd(void *data, const char *name)
{
    (void) data;
    (void) name;
}

#define HEAP_START 0x7FFF0000u
#define HEAP_SIZE 0x20000u
#define LOW_SPACE 0x7FFFF000u
#define HIGH_SPACE 0x80000000u
#define SPACE_SIZE 0x1000u
#define HIGH_FREE 0x80008000u

static int check_unsigned_merge(void)
{
    // Arrange: two adjacent free spaces, the higher one first in the list.
    memaReset((void *) (uintptr_t) HEAP_START, HEAP_SIZE);
    g_MemoryAllocations.spaces[0].addr = HIGH_SPACE;
    g_MemoryAllocations.spaces[0].size = SPACE_SIZE;
    g_MemoryAllocations.spaces[1].addr = LOW_SPACE;
    g_MemoryAllocations.spaces[1].size = SPACE_SIZE;

    // Act
    memaSingleDefragPass();
    memaSingleDefragPass();
    s32 longest = memaGetLongestFree();

    // Assert
    if (longest != (s32) (2 * SPACE_SIZE)
        || (uintptr_t) g_MemoryAllocations.spaces[0].addr != LOW_SPACE) {
        fprintf(stderr, "mema order: spaces either side of 0x80000000 did not merge "
                "(longest 0x%x, first 0x%llx)\n", (unsigned) longest,
                (unsigned long long) (uintptr_t) g_MemoryAllocations.spaces[0].addr);
        return 0;
    }
    return 1;
}

static int read_audit_of_free(char *out, size_t outsize)
{
    FILE *capture = tmpfile();
    int saved = dup(STDERR_FILENO);
    size_t got;

    if (capture == NULL || saved < 0) {
        fprintf(stderr, "mema order: cannot capture stderr\n");
        return 0;
    }
    fflush(stderr);
    dup2(fileno(capture), STDERR_FILENO);
    s_audit = 1;
    memaFree((void *) (uintptr_t) HIGH_FREE, 0x100);
    s_audit = 0;
    fflush(stderr);
    dup2(saved, STDERR_FILENO);
    close(saved);
    rewind(capture);
    got = fread(out, 1, outsize - 1, capture);
    out[got] = '\0';
    fclose(capture);
    return 1;
}

static int check_signed_audit_range(void)
{
    char audit[1024];

    // Arrange
    memaReset((void *) (uintptr_t) HEAP_START, HEAP_SIZE);

    // Act
    if (!read_audit_of_free(audit, sizeof(audit))) {
        return 0;
    }

    // Assert
    if (strstr(audit, "MEMA FREE OUT OF HEAP") == NULL) {
        fprintf(stderr, "mema order: the 32-bit audit compared unsigned; trace was:\n%s", audit);
        return 0;
    }
    return 1;
}

int main(void)
{
    int ok = check_unsigned_merge();
    const char *width = sizeof(void *) == 4 ? "32-bit" : "64-bit";

    if (sizeof(void *) == 4) {
        ok = check_signed_audit_range() && ok;
    }
    if (!ok) {
        return 1;
    }
    printf("mema order OK (%s)\n", width);
    return 0;
}
