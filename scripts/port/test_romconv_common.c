/* The converters' shared helpers (LP64-004 Task 1): byte order, the
 * scratch buffer, the offset map and the refusal line. Built and run at each
 * pointer width by test_romconv.sh, linked with port/src/romconv_common.c and
 * port/src/port_lp64.c (the class names). Every input is written here. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "port_romconv.h"

/* port_console.c's log, captured so the refusal line can be read back. */
static char s_Log[512];

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_Log, sizeof(s_Log), fmt, ap);
    va_end(ap);
}

/* The game's <string.h> (first on the include path, as in the game build)
 * declares no strcmp. */
static int sameText(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

static int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_common: FAIL: %s\n", what);
        return 1;
    }
    return 0;
}

static int testBe32ReadsBigEndian(void)
{
    // Arrange
    static const u8 bytes[4] = {0x12, 0x34, 0x56, 0x78};

    // Act
    u32 value = portConvBe32(bytes);

    // Assert
    return expect(value == 0x12345678u, "portConvBe32 {12 34 56 78} is 0x12345678");
}

static int testBe16ReadsBigEndian(void)
{
    // Arrange
    static const u8 bytes[2] = {0x9a, 0xbc};

    // Act
    u16 value = portConvBe16(bytes);

    // Assert
    return expect(value == 0x9abc, "portConvBe16 {9a bc} is 0x9abc");
}

static int testMapReturnsWhatWasPut(void)
{
    // Arrange
    struct PortConvMap map;
    u32 moved = 0;
    int ok = portConvMapInit(&map, 64);
    portConvMapPut(&map, 0x08, 0x40);
    portConvMapPut(&map, 0x3c, 0x00);

    // Act
    int found = ok && portConvMapGet(&map, 0x08, &moved) && moved == 0x40;
    int foundZero = ok && portConvMapGet(&map, 0x3c, &moved) && moved == 0;

    // Assert
    portConvMapFree(&map);
    return expect(found && foundZero, "a 64-byte file's map returns what was put, a new offset of 0 included");
}

static int testMapMissesAnOffsetNeverPut(void)
{
    // Arrange
    struct PortConvMap map;
    u32 moved = 0x5555;
    int ok = portConvMapInit(&map, 64);
    portConvMapPut(&map, 0x08, 0x40);

    // Act
    int found = portConvMapGet(&map, 0x0c, &moved);

    // Assert
    portConvMapFree(&map);
    return expect(ok && !found && moved == 0x5555, "an offset never put misses and leaves the result alone");
}

static int testMapMissesAnOddOffset(void)
{
    // Arrange
    struct PortConvMap map;
    u32 moved = 0;
    int ok = portConvMapInit(&map, 64);
    portConvMapPut(&map, 0x08, 0x40);

    // Act
    int found = portConvMapGet(&map, 0x09, &moved);

    // Assert
    portConvMapFree(&map);
    return expect(ok && !found, "an odd offset misses");
}

static int testScratchIsWritableAndGrows(void)
{
    // Arrange
    u8 *small = portConvScratch(100);
    memset(small, 0xa5, 100);

    // Act
    u8 *large = portConvScratch(200000);
    memset(large, 0x5a, 200000);

    // Assert
    return expect(small != NULL && large != NULL && large[0] == 0x5a && large[199999] == 0x5a,
            "portConvScratch(100) then (200000) return writable memory");
}

static int testReservedScratchDoesNotMove(void)
{
    // Arrange: the test before leaves the buffer at 200,000 bytes, so a request
    // of 262,144 reallocates unless the reserve grew it. A freed block's address
    // can come straight back (both sizes are mmap'd), so the check is also that
    // the bytes written through it survive: a fresh mapping is zeroed, and a
    // recycled heap block starts with the allocator's free-list links.
    portConvScratchReserve(262144);
    u8 *reserved = portConvScratch(1);
    for (u32 i = 0; i < 64; i++)
    {
        reserved[i] = (u8) (0xA5 ^ i);
    }

    // Act
    u8 *full = portConvScratch(262144);

    // Assert
    int kept = full == reserved;
    for (u32 i = 0; kept && i < 64; i++)
    {
        kept = full[i] == (u8) (0xA5 ^ i);
    }
    return expect(reserved != NULL && kept,
            "after portConvScratchReserve(262144) a request of 262,144 returns the reserved block untouched");
}

static int testMapMissesAtAndPastTheEnd(void)
{
    // Arrange
    struct PortConvMap map;
    u32 moved = 0x5555;
    int ok = portConvMapInit(&map, 64);
    portConvMapPut(&map, 0x40, 0x10);
    portConvMapPut(&map, 0x44, 0x10);

    // Act
    int foundEnd = portConvMapGet(&map, 0x40, &moved);
    int foundPast = portConvMapGet(&map, 0x44, &moved);
    int foundLast = portConvMapGet(&map, 0x3c, &moved);

    // Assert
    portConvMapFree(&map);
    return expect(ok && !foundEnd && !foundPast && !foundLast && moved == 0x5555,
            "a 64-byte map misses 0x40 and 0x44, and putting them writes no slot");
}

static int testMapMissesAHalfWordOffset(void)
{
    // Arrange
    struct PortConvMap map;
    u32 moved = 0x5555;
    int ok = portConvMapInit(&map, 64);
    portConvMapPut(&map, 0x08, 0x40);
    portConvMapPut(&map, 0x0a, 0x50);

    // Act
    int foundHalf = portConvMapGet(&map, 0x0a, &moved);
    int foundWord = portConvMapGet(&map, 0x08, &moved);

    // Assert
    portConvMapFree(&map);
    return expect(ok && !foundHalf && foundWord && moved == 0x40,
            "0x0a (a multiple of 2, not of 4) misses and does not alias 0x08");
}

static int testReservedMapReusesItsStorage(void)
{
    // Arrange: the scratch test above reserved 262,144 bytes, which also sizes
    // the map's store for a file that large.
    struct PortConvMap small;
    struct PortConvMap large;
    int okSmall = portConvMapInit(&small, 64);
    u32 *smallStore = small.newOffset;
    portConvMapFree(&small);

    // Act
    int okLarge = portConvMapInit(&large, 0x40000);
    u32 *largeStore = large.newOffset;
    portConvMapFree(&large);

    // Assert
    return expect(okSmall && okLarge && smallStore != NULL && largeStore == smallStore,
            "after the reserve, maps of 64 and 0x40000 bytes use the same storage");
}

static int testSecondLiveMapIsRefused(void)
{
    // Arrange
    struct PortConvMap first;
    struct PortConvMap second;
    int okFirst = portConvMapInit(&first, 64);
    s_Log[0] = '\0';

    // Act
    int okSecond = portConvMapInit(&second, 64);

    // Assert
    portConvMapFree(&second);
    portConvMapFree(&first);
    return expect(okFirst && !okSecond && second.slots == 0
            && sameText(s_Log, "ge: lp64: converter map: a second map while one is in use\n"),
            "a second map while one is live fails, named, and the first is untouched");
}

static int testMapRefusesASizeItCannotHold(void)
{
#if UINTPTR_MAX == 0xFFFFFFFFu
    // Arrange
    struct PortConvMap map;

    // Act
    int ok = portConvMapInit(&map, 0xFFFFFFFDu);

    // Assert
    portConvMapFree(&map);
    return expect(!ok && map.slots == 0, "at 32 bits a map of 0xFFFFFFFD bytes is refused, not wrapped");
#else
    return 0; /* at 64 bits the slot array's size cannot wrap */
#endif
}

static int testRefuseLogsTheClassFileAndReason(void)
{
    // Arrange
    s_Log[0] = '\0';

    // Act
    portConvRefuse(PORT_LP64_MODELS, "GtriggerZ", "needs %u bytes, has %u", 46000u, 45008u);

    // Assert
    return expect(sameText(s_Log, "ge: lp64: cannot convert models \"GtriggerZ\": needs 46000 bytes, has 45008\n"),
            "portConvRefuse logs ge: lp64: cannot convert CLASS \"FILE\": WHY");
}

int main(void)
{
    int failures = 0;

    failures += testBe32ReadsBigEndian();
    failures += testBe16ReadsBigEndian();
    failures += testMapReturnsWhatWasPut();
    failures += testMapMissesAnOffsetNeverPut();
    failures += testMapMissesAnOddOffset();
    failures += testScratchIsWritableAndGrows();
    failures += testReservedScratchDoesNotMove();
    failures += testMapMissesAtAndPastTheEnd();
    failures += testMapMissesAHalfWordOffset();
    failures += testReservedMapReusesItsStorage();
    failures += testSecondLiveMapIsRefused();
    failures += testMapRefusesASizeItCannotHold();
    failures += testRefuseLogsTheClassFileAndReason();
    return failures == 0 ? 0 : 1;
}
