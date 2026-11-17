/* ROM-free tests of the 64-bit address rules (LP64-003 Task 3): the
 * host-pointer test, the region predicate and the startup address check; of
 * the asset-class guard (Task 4); of the command-word predicates (Task 6); and
 * of the command-word accessors (Task 7); and of the game's strtol (src/str.c,
 * linked in by the harness) and gu.h's FTOFIX32, whose results must not follow
 * a 64-bit long (Task 10). Built at both pointer widths by
 * scripts/port/test_lp64_address.sh.
 *
 *   test_lp64_address              the unit cases
 *   test_lp64_address address-space  the startup check alone, as main.c runs
 *                                    it (the harness links this driver
 *                                    position-independent and -no-pie)
 *   test_lp64_address require-animations  the animation guard alone: at 64
 *                                    bits it must stop the process with
 *                                    status 5, at 32 bits it must return
 *   test_lp64_address pool-fatal     portLp64PoolFatal alone (LP64-004 Task 2):
 *                                    at 64 bits status 6 naming the pool, at
 *                                    32 bits it must return
 *   test_lp64_address music-heap     the music heap's allocator asked for more
 *                                    than its heap holds: status 6 at 64 bits,
 *                                    NULL at 32 bits
 *
 * The unit cases also cover the pool reports' line, their eight-line limit a
 * pool a stage, and their 32-bit no-op form. */
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PR/gu.h"
#include "PR/libaudio.h"
#include "PR/mbi.h"
#include "PR/ultratypes.h"
#include "platform_info.h"
#include "port_gfxwords.h"
#include "port_hostptr.h"
#include "port_lp64.h"

static int s_failures;

/* What the logger has printed since the last reset: the pool lines it saw
 * and the last of them, for the pool-report cases (LP64-004 Task 2). */
static int s_poolLines;
static char s_lastPoolLine[256];

/* port_console.c's logger, which port_lp64.c prints through. */
void portConLogf(const char *fmt, ...)
{
    char line[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    if (strncmp(line, "ge: lp64: pool ", 15) == 0)
    {
        s_poolLines++;
        snprintf(s_lastPoolLine, sizeof(s_lastPoolLine), "%s", line);
    }
    fputs(line, stdout);
    fflush(stdout);
}

static void resetPoolLines(void)
{
    s_poolLines = 0;
    s_lastPoolLine[0] = '\0';
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

/* gbi.h's own macro builds the command word unsigned, so its opcode reads back
 * through GFX_OP at both widths and the upper half stays zero at 64 bits. A
 * writer that sign-extended (a signed (u8) G_ENDDL << 24) fails the second
 * assertion: GFX_OP reads the low 32 bits, so the renderer's whole-word
 * w0 >> 24 is what such a writer would break. */
static void testEndDisplayListOpcode(void)
{
    // Arrange
    Gfx command = gsSPEndDisplayList();

    // Act
    u32 opcode = GFX_OP(&command);

    // Assert
    expect(opcode == (u8) G_ENDDL, "GFX_OP of gsSPEndDisplayList() is G_ENDDL");
#if IS_64_BIT
    expect((command.words.w0 >> 32) == 0, "gsSPEndDisplayList() leaves the upper half of w0 zero");
#endif
}

/* The game's strtol reports overflow as -1. Its bound came from a ULONG_MAX
 * of 64 bits at 64 bits, so 2^32 wrapped to 0 there. */
static void testStrtolOverflowIsMinusOne(void)
{
    // Act
    long value = strtol("4294967296", NULL, 10);

    // Assert
    expect(value == -1, "strtol reports a number past 32 bits as -1");
}

/* It negates in unsigned int; returned through a 64-bit long without a cast,
 * -5 came back as 4294967291. */
static void testStrtolNegativeKeepsSign(void)
{
    // Act
    long value = strtol("-5", NULL, 10);

    // Assert
    expect(value == -5, "strtol(\"-5\") is -5");
}

/* Past 32768.0 the conversion is out of range. The hardware's 32-bit
 * conversion gives INT_MIN (x86) or saturates (Arm); through a 64-bit long
 * the value wrapped instead (40000.0 to -25536.0). The volatile keeps the
 * compiler from folding it. */
static void testFixedPointConversionDoesNotWrap(void)
{
    // Arrange
    volatile float pastRange = 40000.0f;

    // Act
    int fixed = FTOFIX32(pastRange);

    // Assert
    expect(fixed == INT_MIN || fixed == INT_MAX, "FTOFIX32 past 32768.0 does not wrap");
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

#if IS_64_BIT
static void testPoolExhaustedLine(void)
{
    // Arrange
    portLp64PoolStageBegin();
    resetPoolLines();

    // Act
    portLp64PoolExhausted("texture pool", 2600);

    // Assert
    expect(strcmp(s_lastPoolLine, "ge: lp64: pool \"texture pool\" exhausted (request 2600 bytes)\n") == 0,
           "a pool report names the pool and the request");
}

static void testPoolExhaustedStopsAfterEightLines(void)
{
    // Arrange
    int i;

    portLp64PoolStageBegin();
    resetPoolLines();

    // Act
    for (i = 0; i < 9; i++)
    {
        portLp64PoolExhausted("room memory", 64);
    }

    // Assert
    expect(s_poolLines == 8, "nine reports of one pool in a stage print eight lines");
}

static void testPoolLimitIsPerPool(void)
{
    // Arrange
    int i;

    portLp64PoolStageBegin();
    for (i = 0; i < 8; i++)
    {
        portLp64PoolExhausted("room memory", 64);
    }
    resetPoolLines();

    // Act
    portLp64PoolExhausted("texture pool", 4300);

    // Assert
    expect(s_poolLines == 1, "a pool that has not used its eight lines still prints after another has");
}

static void testPoolLimitRestartsAtStageBegin(void)
{
    // Arrange
    int i;

    portLp64PoolStageBegin();
    for (i = 0; i < 8; i++)
    {
        portLp64PoolExhausted("room memory", 64);
    }
    resetPoolLines();

    // Act
    portLp64PoolStageBegin();
    portLp64PoolExhausted("room memory", 64);

    // Assert
    expect(s_poolLines == 1, "the next stage prints a pool's report again");
}
#else
static void testPoolReportsAreNoOpsAtThirtyTwoBits(void)
{
    // Arrange
    resetPoolLines();

    // Act
    portLp64PoolStageBegin();
    portLp64PoolExhausted("room memory", 64);
    portLp64PoolFatal("music heap", 4096);

    // Assert
    expect(s_poolLines == 0, "the pool reports print nothing and return at 32 bits");
}
#endif

/* The music heap's allocator (src/libultra/audio/heapalloc.c, linked in by
 * the harness) on a heap of 64 bytes asked for 4096: at 64 bits it must stop
 * the game naming the pool, at 32 bits it returns NULL as it always has. */
static int runMusicHeapRequest(void)
{
    static u8 heapBytes[64];
    ALHeap heap = { heapBytes, heapBytes, (s32) sizeof(heapBytes), 0 };
    void *result;

    result = alHeapDBAlloc(NULL, 0, &heap, 1, 4096);
    printf("music-heap: returned %s\n", result == NULL ? "NULL" : "a block");
    return result == NULL ? 0 : 1;
}

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
    if (argc > 1 && strcmp(argv[1], "pool-fatal") == 0)
    {
        portLp64PoolFatal("music heap", 4096);
        printf("pool-fatal: returned\n");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "music-heap") == 0)
    {
        return runMusicHeapRequest();
    }

    testHostAddrTruthTable();
    testWordIsHostTruthTable();
    testWordHasSegmentTruthTable();
    testWordIsIndexTruthTable();
    testTexturePointerDecision();
    testEndDisplayListOpcode();
    testStrtolOverflowIsMinusOne();
    testStrtolNegativeKeepsSign();
    testFixedPointConversionDoesNotWrap();
#if IS_64_BIT
    testRegionEndingAtFourGbIsLow();
    testRegionStartingAtFourGbIsHigh();
    testEmptyRegionAtFourGbIsHigh();
    testAssetNamesInDesignOrder();
    testAssetNameOutOfRange();
    testPoolExhaustedLine();
    testPoolExhaustedStopsAfterEightLines();
    testPoolLimitIsPerPool();
    testPoolLimitRestartsAtStageBegin();
#else
    testCheckIsNoOpAtThirtyTwoBits();
    testAssetGuardIsNoOpAtThirtyTwoBits();
    testPoolReportsAreNoOpsAtThirtyTwoBits();
#endif
    return s_failures == 0 ? 0 : 1;
}
