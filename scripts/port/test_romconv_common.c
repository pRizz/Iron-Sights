/* The converters' shared helpers (LP64-004 Task 1): byte order, the
 * scratch buffer, the offset map and the refusal line. Built and run at each
 * pointer width by test_romconv.sh, linked with port/src/romconv_common.c and
 * port/src/port_lp64.c (the class names). Every input is written here. */
#include <stdarg.h>
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
    // Arrange
    portConvScratchReserve(262144);

    // Act
    u8 *first = portConvScratch(262144);
    u8 *second = portConvScratch(262144);

    // Assert
    return expect(first != NULL && first == second, "after portConvScratchReserve(262144) a request of 262,144 does not move");
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
    failures += testRefuseLogsTheClassFileAndReason();
    return failures == 0 ? 0 : 1;
}
