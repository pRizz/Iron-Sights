/* ROM-free tests of the 64-bit address rules (LP64-003 Task 3): the
 * host-pointer test, the region predicate and the startup address check; of
 * the asset-class guard (Task 4); and of the command-word predicates (Task 6). Built at both pointer widths by
 * scripts/port/test_lp64_address.sh.
 *
 *   test_lp64_address              the unit cases
 *   test_lp64_address address-space  the startup check alone, as main.c runs
 *                                    it (the harness links this driver
 *                                    position-independent and -no-pie)
 *   test_lp64_address require-animations  the animation guard alone: at 64
 *                                    bits it must stop the process with
 *                                    status 5, at 32 bits it must return */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "PR/ultratypes.h"
#include "platform_info.h"
#include "port_hostptr.h"
#include "port_lp64.h"

static int s_failures;

/* port_console.c's logger, which port_lp64.c prints through. */
void portConLogf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

static void expect(int condition, const char *what)
{
    if (!condition)
    {
        printf("FAIL: %s\n", what);
        s_failures++;
    }
}

static void testHostAddrTruthTable(void)
{
    // Arrange
    uintptr_t segmentWord = 0x0E000000u;
#if IS_64_BIT
    uintptr_t allLowBits = 0xFFFFFFFFu;
    uintptr_t fourGb = (uintptr_t) 0x100000000ull;
    uintptr_t lowWordLikeSegment3 = (uintptr_t) 0x0000AAAA03123456ull;
#else
    uintptr_t belowLine = 0x0FFFFFFFu;
    uintptr_t atLine = 0x10000000u;
#endif

    // Act and Assert
    expect(!PORT_IS_HOST_ADDR(segmentWord), "a segment-14 word is not a host address");
#if IS_64_BIT
    expect(!PORT_IS_HOST_ADDR(allLowBits), "0xFFFFFFFF is not a host address at 64 bits");
    expect(PORT_IS_HOST_ADDR(fourGb), "0x100000000 is a host address at 64 bits");
    expect(PORT_IS_HOST_ADDR(lowWordLikeSegment3),
           "0x0000AAAA03123456 is a host address although its low word looks like a segment-3 word");
#else
    expect(!PORT_IS_HOST_ADDR(belowLine), "0x0FFFFFFF is not a host address at 32 bits");
    expect(PORT_IS_HOST_ADDR(atLine), "0x10000000 is a host address at 32 bits");
#endif
}

static void testWordIsHostTruthTable(void)
{
    // Arrange
    uintptr_t segmentWord = 0x0E001234u;
#if IS_64_BIT
    uintptr_t allLowBits = 0xFFFFFFFFu;
    uintptr_t lowWordLikeSegment14 = (uintptr_t) 0x0000AAAA0E001234ull;
#else
    uintptr_t belowLine = 0x0FFFFFFFu;
    uintptr_t atLine = 0x10000000u;
#endif

    // Act and Assert
    expect(!PORT_WORD_IS_HOST(segmentWord), "a segment-14 word is not a host word");
#if IS_64_BIT
    expect(!PORT_WORD_IS_HOST(allLowBits), "0xFFFFFFFF is not a host word at 64 bits");
    expect(PORT_WORD_IS_HOST(lowWordLikeSegment14),
           "0x0000AAAA0E001234 is a host word although its low word looks like a segment-14 word");
#else
    expect(!PORT_WORD_IS_HOST(belowLine), "0x0FFFFFFF is not a host word at 32 bits");
    expect(PORT_WORD_IS_HOST(atLine), "0x10000000 is a host word at 32 bits");
#endif
}

static void testWordHasSegmentTruthTable(void)
{
    // Arrange
    uintptr_t segment14Word = 0x0E001234u;
    uintptr_t segment15Word = 0x0F001234u;
#if IS_64_BIT
    uintptr_t lowWordLikeSegment14 = (uintptr_t) 0x0000AAAA0E001234ull;
#endif

    // Act and Assert
    expect(PORT_WORD_HAS_SEGMENT(segment14Word, 0x0E), "0x0E001234 is a segment-14 word");
    expect(!PORT_WORD_HAS_SEGMENT(segment15Word, 0x0E), "0x0F001234 is not a segment-14 word");
#if IS_64_BIT
    expect(!PORT_WORD_HAS_SEGMENT(lowWordLikeSegment14, 0x0E),
           "0x0000AAAA0E001234 is not a segment-14 word at 64 bits");
#endif
}

static void testWordIsIndexTruthTable(void)
{
    // Arrange
    uintptr_t lastIndex = 0xFFFu;
    uintptr_t limit = 0x1000u;
#if IS_64_BIT
    uintptr_t lowWordLikeIndex = (uintptr_t) 0x0000AAAA00000123ull;
#endif

    // Act and Assert
    expect(PORT_WORD_IS_INDEX(lastIndex, 0x1000), "0xFFF is an index below 0x1000");
    expect(!PORT_WORD_IS_INDEX(limit, 0x1000), "0x1000 is not an index below 0x1000");
#if IS_64_BIT
    expect(!PORT_WORD_IS_INDEX(lowWordLikeIndex, 0x1000),
           "0x0000AAAA00000123 is not an index below 0x1000 at 64 bits");
#endif
}

/* The decision the command-word sites in propobj.c and bg.c make before
 * reading a texture number through w1: a host pointer and not an IMAGESEG tag
 * (0xABCD0000 | id). Written with the sites' own two expressions, since the
 * decision is inline there, not a function. */
static int siteWordIsTexturePointer(uintptr_t timg)
{
    return PORT_WORD_IS_HOST(timg) && (timg >> 16) != 0xABCDu;
}

static void testTexturePointerDecision(void)
{
    // Arrange
    uintptr_t imagesegTag = 0xABCD0012u;
#if IS_64_BIT
    uintptr_t pointerWithTagBits = (uintptr_t) 0x0000AAAAABCD1234ull;
#endif

    // Act and Assert
    expect(!siteWordIsTexturePointer(imagesegTag), "0xABCD0012 is an IMAGESEG tag, not a texture pointer");
#if IS_64_BIT
    expect(siteWordIsTexturePointer(pointerWithTagBits),
           "0x0000AAAAABCD1234 is a texture pointer although bits 16-31 are 0xABCD");
#endif
}

#if IS_64_BIT
static void testRegionEndingAtFourGbIsLow(void)
{
    // Arrange
    const void *start = (const void *) (uintptr_t) 0xFFFFF000u;

    // Act
    int high = portLp64RegionIsHigh(start, 0x1000u);

    // Assert
    expect(!high, "a region ending at 4 GB is low");
}

static void testRegionStartingAtFourGbIsHigh(void)
{
    // Arrange
    const void *start = (const void *) (uintptr_t) 0x100000000ull;

    // Act
    int high = portLp64RegionIsHigh(start, 0x1000u);

    // Assert
    expect(high, "a region starting at 4 GB is high");
}

static void testEmptyRegionAtFourGbIsHigh(void)
{
    // Arrange
    const void *start = (const void *) (uintptr_t) 0x100000000ull;

    // Act
    int high = portLp64RegionIsHigh(start, 0);

    // Assert
    expect(high, "a zero-size region at 4 GB is high");
}
#else
static void testCheckIsNoOpAtThirtyTwoBits(void)
{
    // Arrange: every 32-bit address is below 4 GB.
    const void *lowAddress = (const void *) (uintptr_t) 0x1000u;

    // Act: the check must not stop the program.
    (void) lowAddress;
    portLp64CheckRegion("low address", lowAddress, 16);
    portLp64CheckAddressSpace();

    // Assert
    expect(1, "the 32-bit check returns");
}
#endif

#if IS_64_BIT
static void testAssetNamesInDesignOrder(void)
{
    // Arrange
    static const char *const expected[PORT_LP64_ASSET_COUNT] = {
        "text banks", "fonts", "textures", "animation tables", "models",
        "collision tiles", "setup files", "backgrounds", "audio banks", "music sequences",
    };
    int i;

    for (i = 0; i < PORT_LP64_ASSET_COUNT; i++)
    {
        // Act
        const char *name = portLp64AssetName((enum PortLp64Asset) i);

        // Assert
        expect(name != NULL && strcmp(name, expected[i]) == 0, expected[i]);
    }
}

static void testAssetNameOutOfRange(void)
{
    // Act
    const char *name = portLp64AssetName(PORT_LP64_ASSET_COUNT);

    // Assert
    expect(name != NULL && strcmp(name, "unknown asset class") == 0, "a value past the classes is named unknown");
}
#else
static void testAssetGuardIsNoOpAtThirtyTwoBits(void)
{
    // Arrange: at 32 bits the in-place conversions are the shipped path.

    // Act
    portLp64RequireConverter(PORT_LP64_ANIMATIONS);

    // Assert
    expect(portLp64AssetConverted(PORT_LP64_ANIMATIONS), "every class counts as converted at 32 bits");
}
#endif

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "address-space") == 0)
    {
        portLp64CheckAddressSpace();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "require-animations") == 0)
    {
        portLp64RequireConverter(PORT_LP64_ANIMATIONS);
        printf("require-animations: returned\n");
        return 0;
    }

    testHostAddrTruthTable();
    testWordIsHostTruthTable();
    testWordHasSegmentTruthTable();
    testWordIsIndexTruthTable();
    testTexturePointerDecision();
#if IS_64_BIT
    testRegionEndingAtFourGbIsLow();
    testRegionStartingAtFourGbIsHigh();
    testEmptyRegionAtFourGbIsHigh();
    testAssetNamesInDesignOrder();
    testAssetNameOutOfRange();
#else
    testCheckIsNoOpAtThirtyTwoBits();
    testAssetGuardIsNoOpAtThirtyTwoBits();
#endif
    return s_failures == 0 ? 0 : 1;
}
