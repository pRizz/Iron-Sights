/* The display-list expanders (LP64-004 Task 8): portGfxFromRom,
 * portGfxFromRomBackward and portGfxFromWords turn 8-byte commands into host
 * Gfx with both words zero-extended, write nothing outside COMMANDS Gfx, and
 * keep the overlaps port_romconv.h allows. At 32 bits (the oracle build) the
 * texCopyGdls oracle is checked too. Built and run at each pointer width by
 * test_romconv.sh, under the sanitizers at 64 bits, so a read outside the
 * exact-size sources below is a failure. Every input is written here. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "port_gfxwords.h"
#include "port_romconv.h"

#define FILL 0xA5u
#define THREE 3u
/* The bytes an overlapping layout may put the source after DST: G, as
 * port_romconv.h names it. */
#define GROWTH (sizeof(Gfx) - 8u)

/* port_console.c's log, captured so the oracle's line can be read back. */
static char s_Log[512];

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_Log, sizeof(s_Log), fmt, ap);
    va_end(ap);
}

#ifdef GE_LP64_ORACLE
static int startsWith(const char *text, const char *prefix)
{
    while (*prefix != '\0' && *text == *prefix)
    {
        text++;
        prefix++;
    }
    return *prefix == '\0';
}
#endif

static int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_gfx: FAIL: %s\n", what);
        return 1;
    }
    return 0;
}

/* The plan's two commands: G_VTX with a segment-0x0E word, then G_ENDDL. */
static const u8 kRom[16] = {
    0x04, 0x30, 0x00, 0x40, 0x0E, 0x00, 0x01, 0x20,
    0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* Every bit set in both words: a sign extension would show in the upper
 * halves at 64 bits. */
static const u8 kAllOnes[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* Three distinct commands for the overlap cases. */
static const u8 kThree[THREE * 8] = {
    0x04, 0x30, 0x00, 0x40, 0x0E, 0x00, 0x01, 0x20,
    0xFF, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00, 0x01,
    0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* A shared buffer: one Gfx of FILL before, THREE Gfx for the commands, one
 * Gfx of FILL after. */
static union
{
    Gfx cmds[THREE + 2];
    u8 bytes[(THREE + 2) * sizeof(Gfx)];
} s_Shared;

static Gfx s_Out[THREE + 1];

static void fillOut(void)
{
    memset(s_Out, FILL, sizeof(s_Out));
}

static int bytesAreFill(const void *p, size_t n)
{
    const u8 *b = (const u8 *) p;
    size_t i;

    for (i = 0; i < n; i++)
    {
        if (b[i] != FILL)
        {
            return 0;
        }
    }
    return 1;
}

/* Fills the shared buffer and copies kThree to SRCOFFSET bytes into its
 * commands; returns the commands' first Gfx. */
static Gfx *sharedWithSourceAt(size_t srcoffset)
{
    memset(s_Shared.bytes, FILL, sizeof(s_Shared.bytes));
    memcpy((u8 *) &s_Shared.cmds[1] + srcoffset, kThree, sizeof(kThree));
    return &s_Shared.cmds[1];
}

static int sharedEdgesUntouched(void)
{
    return bytesAreFill(&s_Shared.cmds[0], sizeof(Gfx)) && bytesAreFill(&s_Shared.cmds[THREE + 1], sizeof(Gfx));
}

/* The three commands as the forward call writes them on separate buffers. */
static int sameAsSeparate(const Gfx *got)
{
    Gfx want[THREE];
    u32 i;

    portGfxFromRom(want, kThree, THREE);
    for (i = 0; i < THREE; i++)
    {
        if (got[i].words.w0 != want[i].words.w0 || got[i].words.w1 != want[i].words.w1)
        {
            return 0;
        }
    }
    return 1;
}

static int testFirstCommandsOpcodeAndW0(void)
{
    // Arrange
    fillOut();

    // Act
    portGfxFromRom(s_Out, kRom, 2);

    // Assert
    return expect(GFX_OP(&s_Out[0]) == 0x04 && s_Out[0].words.w0 == 0x04300040u,
            "command 0 is G_VTX with w0 0x04300040");
}

static int testFirstCommandsW1AndSecondCommandsW0(void)
{
    // Arrange
    fillOut();

    // Act
    portGfxFromRom(s_Out, kRom, 2);

    // Assert
    return expect(s_Out[0].words.w1 == 0x0E000120u && s_Out[1].words.w0 == 0xB8000000u,
            "command 0's w1 is 0x0E000120 and command 1's w0 0xB8000000");
}

static int testWordsArePointerSized(void)
{
    // Assert
    return expect(sizeof(s_Out[0].words.w0) == sizeof(void *), "a Gfx word is pointer-sized");
}

static int testW0IsZeroExtended(void)
{
    // Arrange
    fillOut();

    // Act
    portGfxFromRom(s_Out, kAllOnes, 1);

    // Assert
    return expect(s_Out[0].words.w0 == 0xFFFFFFFFu, "0xFFFFFFFF in w0 is zero-extended, not sign-extended");
}

static int testW1IsZeroExtended(void)
{
    // Arrange
    fillOut();

    // Act
    portGfxFromRom(s_Out, kAllOnes, 1);

    // Assert
    return expect(s_Out[0].words.w1 == 0xFFFFFFFFu, "0xFFFFFFFF in w1 is zero-extended, not sign-extended");
}

static int testForwardWritesNothingPastItsCommands(void)
{
    // Arrange
    fillOut();

    // Act
    portGfxFromRom(s_Out, kThree, 2);

    // Assert
    return expect(bytesAreFill(&s_Out[2], 2 * sizeof(Gfx)), "two commands write two Gfx and nothing after");
}

static int testForwardWithNoCommandsTouchesNothing(void)
{
    // Arrange
    fillOut();

    // Act
    portGfxFromRom(s_Out, NULL, 0);

    // Assert
    return expect(bytesAreFill(s_Out, sizeof(s_Out)), "no commands: nothing read (NULL source) or written");
}

static int testBackwardOnSeparateBuffersMatchesForward(void)
{
    // Arrange
    fillOut();

    // Act
    portGfxFromRomBackward(s_Out, kThree, THREE);

    // Assert
    return expect(sameAsSeparate(s_Out), "backward on separate buffers gives the forward call's commands");
}

static int testBackwardWritesNothingPastItsCommands(void)
{
    // Arrange
    fillOut();

    // Act
    portGfxFromRomBackward(s_Out, kThree, THREE);

    // Assert
    return expect(bytesAreFill(&s_Out[THREE], sizeof(Gfx)), "backward writes nothing after its commands");
}

static int testBackwardWithNoCommandsTouchesNothing(void)
{
    // Arrange
    fillOut();

    // Act
    portGfxFromRomBackward(s_Out, NULL, 0);

    // Assert
    return expect(bytesAreFill(s_Out, sizeof(s_Out)), "backward, no commands: nothing read or written");
}

static int testForwardAtItsTightestOverlap(void)
{
    // Arrange: the source G * (THREE - 1) bytes after DST, the start of
    // room for THREE Gfx (at 32 bits, the same address).
    Gfx *dst = sharedWithSourceAt(GROWTH * (THREE - 1));

    // Act
    portGfxFromRom(dst, (const u8 *) dst + GROWTH * (THREE - 1), THREE);

    // Assert
    return expect(sameAsSeparate(dst) && sharedEdgesUntouched(),
            "forward with DST G * (n - 1) bytes before the source gives the separate result, nothing outside");
}

static int testForwardWithTheSourceAtTheBuffersEnd(void)
{
    // Arrange: the plan's layout, the commands at the end of room for THREE
    // Gfx and DST at its start.
    Gfx *dst = sharedWithSourceAt(THREE * sizeof(Gfx) - sizeof(kThree));

    // Act
    portGfxFromRom(dst, (const u8 *) dst + THREE * sizeof(Gfx) - sizeof(kThree), THREE);

    // Assert
    return expect(sameAsSeparate(dst) && sharedEdgesUntouched(),
            "forward with the source at the end of the buffer gives the separate result, nothing outside");
}

static int testBackwardAtItsTightestOverlap(void)
{
    // Arrange: the source G bytes after DST.
    Gfx *dst = sharedWithSourceAt(GROWTH);

    // Act
    portGfxFromRomBackward(dst, (const u8 *) dst + GROWTH, THREE);

    // Assert
    return expect(sameAsSeparate(dst) && sharedEdgesUntouched(),
            "backward with DST G bytes before the source gives the separate result, nothing outside");
}

static int testBackwardInPlace(void)
{
    // Arrange
    Gfx *dst = sharedWithSourceAt(0);

    // Act
    portGfxFromRomBackward(dst, (const u8 *) dst, THREE);

    // Assert
    return expect(sameAsSeparate(dst) && sharedEdgesUntouched(),
            "backward with DST at the source gives the separate result, nothing outside");
}

static int testBackwardIntoTheBuffersEnd(void)
{
    // Arrange: texCopyGdls's room lists, the source at the start of a
    // buffer and DST ending at its end.
    u8 *buffer = (u8 *) &s_Shared.cmds[1];
    size_t sourceSize = sizeof(kThree);
    Gfx *dst;

    memset(s_Shared.bytes, FILL, sizeof(s_Shared.bytes));
    memcpy(buffer, kThree, sourceSize);
    dst = (Gfx *) (buffer + 8);
    /* room for THREE Gfx plus 8 bytes: DST overlaps the source when Gfx is 16 */

    // Act
    portGfxFromRomBackward(dst, buffer, THREE);

    // Assert
    return expect(sameAsSeparate(dst) && bytesAreFill(&s_Shared.cmds[0], sizeof(Gfx)),
            "backward with DST 8 bytes after the source gives the separate result");
}

static int testWordsAreCopiedZeroExtended(void)
{
    // Arrange
    static const u32 words[4] = {0x04300040u, 0x0E000120u, 0xFFFFFFFFu, 0xFFFFFFFFu};
    fillOut();

    // Act
    portGfxFromWords(s_Out, words, 2);

    // Assert
    return expect(s_Out[0].words.w0 == 0x04300040u && s_Out[0].words.w1 == 0x0E000120u
            && s_Out[1].words.w0 == 0xFFFFFFFFu && s_Out[1].words.w1 == 0xFFFFFFFFu,
            "native words become the commands' words, 0xFFFFFFFF zero-extended");
}

static int testWordsWriteNothingPastTheirCommands(void)
{
    // Arrange
    static const u32 words[2] = {0xB8000000u, 0};
    fillOut();

    // Act
    portGfxFromWords(s_Out, words, 1);

    // Assert
    return expect(bytesAreFill(&s_Out[1], THREE * sizeof(Gfx)), "one native command writes one Gfx");
}

static int testWordsWithNoCommandsTouchNothing(void)
{
    // Arrange
    fillOut();

    // Act
    portGfxFromWords(s_Out, NULL, 0);

    // Assert
    return expect(bytesAreFill(s_Out, sizeof(s_Out)), "native words, no commands: nothing read or written");
}

#ifdef GE_LP64_ORACLE
/* texCopyGdls's 32-bit body, as tex.c has it: each big-endian command copied
 * to DST with its words swapped, last to first. */
static void texCopyGdls32(Gfx *src, Gfx *dst, s32 bytes)
{
    s32 n = bytes >> 3;
    const u32 *s = (const u32 *) src + 2 * (n - 1);
    u32 *d = (u32 *) dst + 2 * (n - 1);

    while (n--)
    {
        u32 w0 = s[0];
        u32 w1 = s[1];

        d[0] = (w0 >> 24) | ((w0 >> 8) & 0xFF00u) | ((w0 & 0xFF00u) << 8) | (w0 << 24);
        d[1] = (w1 >> 24) | ((w1 >> 8) & 0xFF00u) | ((w1 & 0xFF00u) << 8) | (w1 << 24);
        s -= 2;
        d -= 2;
    }
}

static int testOracleFindsTheCopyIdentical(void)
{
    // Arrange
    Gfx *src = sharedWithSourceAt(0);
    Gfx *dst = (Gfx *) ((u8 *) src + 8);
    s_Log[0] = '\0';
    portGfxCopyOracleBefore(src, dst, (s32) sizeof(kThree));
    texCopyGdls32(src, dst, (s32) sizeof(kThree));

    // Act
    portGfxCopyOracleAfter();

    // Assert
    return expect(startsWith(s_Log, "ge: lp64: oracle: textures room list ")
            && strstr(s_Log, " identical (24 bytes)") != NULL,
            "the oracle finds an overlapping 32-bit room-list copy identical");
}

static int testOracleFindsACopyThatDiffers(void)
{
    // Arrange
    Gfx *src = sharedWithSourceAt(0);
    Gfx *dst = (Gfx *) ((u8 *) src + 8);
    s_Log[0] = '\0';
    portGfxCopyOracleBefore(src, dst, (s32) sizeof(kThree));
    texCopyGdls32(src, dst, (s32) sizeof(kThree));
    dst[2].words.w1 ^= 1;

    // Act
    portGfxCopyOracleAfter();

    // Assert
    return expect(startsWith(s_Log, "ge: lp64: oracle: textures room list ")
            && strstr(s_Log, " DIFFERS at +0x14") != NULL,
            "the oracle finds the third command's w1 differing");
}
#endif

int main(void)
{
    int failures = 0;

    failures += testFirstCommandsOpcodeAndW0();
    failures += testFirstCommandsW1AndSecondCommandsW0();
    failures += testWordsArePointerSized();
    failures += testW0IsZeroExtended();
    failures += testW1IsZeroExtended();
    failures += testForwardWritesNothingPastItsCommands();
    failures += testForwardWithNoCommandsTouchesNothing();
    failures += testBackwardOnSeparateBuffersMatchesForward();
    failures += testBackwardWritesNothingPastItsCommands();
    failures += testBackwardWithNoCommandsTouchesNothing();
    failures += testForwardAtItsTightestOverlap();
    failures += testForwardWithTheSourceAtTheBuffersEnd();
    failures += testBackwardAtItsTightestOverlap();
    failures += testBackwardInPlace();
    failures += testBackwardIntoTheBuffersEnd();
    failures += testWordsAreCopiedZeroExtended();
    failures += testWordsWriteNothingPastTheirCommands();
    failures += testWordsWithNoCommandsTouchNothing();
#ifdef GE_LP64_ORACLE
    failures += testOracleFindsTheCopyIdentical();
    failures += testOracleFindsACopyThatDiffers();
#endif
    return failures == 0 ? 0 : 1;
}
