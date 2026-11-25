/* The font converter's input checks (LP64-004 Task 9): portFontConvert
 * refuses by name, with nothing written, a short room, a short file, a size
 * that would wrap and a pixel offset outside the glyph bytes; and, in the
 * 32-bit oracle build, portFontOracle and portFontListOracle report a staged
 * font and font list identical, or the first byte that differs. Built and run
 * at each pointer width by test_romconv.sh. Every input is written here and in
 * test_romconv_font_fixture.c. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include <bondtypes.h>
#include "port_romconv.h"
#include "port_romlayout.h"
#include "test_romconv_font_fixture.h"

static int testRefusesBankGothicOneByteShortOfItsStagingSize(void)
{
    // Act, Assert
    return expectRefused(romWithGlyph0At(GOTHIC_SIZE - 1), GOTHIC_SIZE, (IS_64_BIT ? 0x24B0u + 0x2F4u : 0x24B0u) - 1,
            "the font needs ", "Bank Gothic one byte short of its staging size is refused, nothing written");
}

static int testRefusesARoomOneByteShort(void)
{
    // Act, Assert
    return expectRefused(buildRom(), ROM_SIZE, OUT_SIZE - 1, "the font needs ",
            "a room one byte short is refused, nothing written");
}

static int testRefusesAFileShorterThanTheTable(void)
{
    // Act, Assert
    return expectRefused(buildRom(), 0xB73, OUT_SIZE, "the 2931-byte file is shorter than its 2932-byte table",
            "a file one byte shorter than the table is refused, nothing written");
}

#if IS_64_BIT
static int testRefusesASizeThatWouldWrap(void)
{
    // Act, Assert: refused before any glyph offset is read past the table.
    return expectRefused(buildRom(), 0xFFFFFFFFu, 0xFFFFFFFFu, "the 4294967295-byte file's image would pass 4 GB",
            "a file whose image would wrap is refused, nothing written");
}
#endif

static int testRefusesAnOffsetPastTheFile(void)
{
    // Act, Assert
    return expectRefused(romWithGlyph0At(0xFFFFFF), ROM_SIZE, OUT_SIZE,
            "glyph 0's pixel offset 0xFFFFFF is outside the glyph bytes",
            "pixel offset 0xFFFFFF is refused, nothing written");
}

static int testRefusesAnOffsetAtTheFilesEnd(void)
{
    // Act, Assert
    return expectRefused(romWithGlyph0At(ROM_SIZE), ROM_SIZE, OUT_SIZE,
            "glyph 0's pixel offset 0xB88 is outside the glyph bytes",
            "a pixel offset at the file's end is refused, nothing written");
}

static int testRefusesAnOffsetInsideTheTable(void)
{
    // Act, Assert
    return expectRefused(romWithGlyph0At(0xB73), ROM_SIZE, OUT_SIZE,
            "glyph 0's pixel offset 0xB73 is outside the glyph bytes",
            "a pixel offset inside the table is refused, nothing written");
}

static int testRefusesTheLastGlyphsOffset(void)
{
    // Arrange
    u8 *rom = buildRom();
    putBe32(rom + CHAR_AT(93, pixel_offset), 4);

    // Act, Assert
    return expectRefused(rom, ROM_SIZE, OUT_SIZE, "glyph 93's pixel offset 0x4 is outside the glyph bytes",
            "glyph 93's pixel offset inside the table is refused, nothing written");
}

static int testAcceptsAnOffsetAtTheTablesEnd(void)
{
    // Act
    int result = portFontConvert(romWithGlyph0At(0xB74), ROM_SIZE, freshOut(), OUT_SIZE);

    // Assert
    return expect(result == 1 && (uintptr_t) s_Out.font.chars[0].pixeldata == sizeof(struct font),
            "a glyph at the table's end converts to the struct's end");
}

/* LP64-007 Task 7: load_font_tables and the text drawing read a glyph's
 * height rows of width rounded up to 8 bytes from its pixel offset. */
static int testRefusesAGlyphWhoseRowsPassTheFile(void)
{
    // Arrange: two 8-byte rows at 0xB80 in a 0xB88-byte file
    u8 *rom = buildRom();
    putBe32(rom + CHAR_AT(0, height), 2);

    // Act, Assert
    return expectRefused(rom, ROM_SIZE, OUT_SIZE, "glyph 0's 2 rows of 8 bytes at 0xB80 pass the 2952-byte file",
            "a glyph whose rows run past the file is refused, nothing written");
}

static int testRefusesAGlyphWhoseRoundedWidthPassesTheFile(void)
{
    // Arrange: width 9 rounds up to a 16-byte row
    u8 *rom = buildRom();
    putBe32(rom + CHAR_AT(0, width), 9);

    // Act, Assert
    return expectRefused(rom, ROM_SIZE, OUT_SIZE, "glyph 0's 1 rows of 16 bytes at 0xB80 pass the 2952-byte file",
            "a glyph whose width rounded up to 8 runs past the file is refused, nothing written");
}

static int testRefusesAGlyphOfNegativeWidth(void)
{
    // Arrange: width -1, a row of 0xFFFFFFFF rounded up
    u8 *rom = buildRom();
    putBe32(rom + CHAR_AT(0, width), 0xFFFFFFFFu);

    // Act, Assert
    return expectRefused(rom, ROM_SIZE, OUT_SIZE, "glyph 0's 1 rows of 4294967296 bytes at 0xB80 pass",
            "a glyph of width -1 is refused, nothing written");
}

#ifdef GE_LP64_ORACLE
/* What the 32-bit staging writes for the test font: the table's words in
 * native order, the glyph bytes as they are (segments.c's portFontFromRom). */
static u8 *inPlaceFont(void)
{
    static u8 image[ROM_SIZE];
    u32 w;

    memcpy(image, buildRom(), ROM_SIZE);
    for (w = 0; w < sizeof(RomFont) / 4; w++)
    {
        u32 v = portConvBe32(s_Rom + w * 4);
        memcpy(image + w * 4, &v, 4);
    }
    return image;
}

static int testOracleFindsTheStagedFontIdentical(void)
{
    // Arrange
    u8 *staged = inPlaceFont();
    s_Log[0] = '\0';

    // Act
    portFontOracle("fontbankgothic", staged, s_Rom, ROM_SIZE);

    // Assert
    return expect(startsWith(s_Log, "ge: lp64: oracle: fonts fontbankgothic identical (2952 bytes)"),
            "the oracle finds the staged font identical");
}

static int testOracleFindsAGlyphByteThatDiffers(void)
{
    // Arrange
    u8 *staged = inPlaceFont();
    staged[GLYPH0_AT + 1] ^= 1;
    s_Log[0] = '\0';

    // Act
    portFontOracle("fontbankgothic", staged, s_Rom, ROM_SIZE);

    // Assert
    return expect(startsWith(s_Log, "ge: lp64: oracle: fonts fontbankgothic DIFFERS at +0xB81"),
            "the oracle finds glyph 0's second byte differing");
}

/* The font list: 24 commands, command i's words 0xE7000000 + i and 0x01000000 + 8i. */
static const u8 *romFontList(void)
{
    static u8 list[PORT_FONTDL_COMMANDS * 8];
    u32 i;

    for (i = 0; i < PORT_FONTDL_COMMANDS; i++)
    {
        putBe32(list + i * 8, 0xE7000000u + i);
        putBe32(list + i * 8 + 4, 0x01000000u + i * 8);
    }
    return list;
}

static u32 *stagedFontList(void)
{
    static u32 words[PORT_FONTDL_COMMANDS * 2];
    u32 i;

    for (i = 0; i < PORT_FONTDL_COMMANDS; i++)
    {
        words[i * 2] = 0xE7000000u + i;
        words[i * 2 + 1] = 0x01000000u + i * 8;
    }
    return words;
}

static int testOracleFindsTheStagedFontListIdentical(void)
{
    // Arrange
    u32 *staged = stagedFontList();
    s_Log[0] = '\0';

    // Act
    portFontListOracle((const u8 *) staged, romFontList());

    // Assert
    return expect(startsWith(s_Log, "ge: lp64: oracle: fonts fontdl identical (192 bytes)"),
            "the oracle finds the staged font list identical");
}

static int testOracleFindsAFontListWordThatDiffers(void)
{
    // Arrange
    u32 *staged = stagedFontList();
    staged[47] ^= 0x100;
    s_Log[0] = '\0';

    // Act
    portFontListOracle((const u8 *) staged, romFontList());

    // Assert
    return expect(startsWith(s_Log, "ge: lp64: oracle: fonts fontdl DIFFERS at +0xBD"),
            "the oracle finds the last command's second word differing");
}
#endif

int main(void)
{
    int failures = 0;

    failures += testRefusesBankGothicOneByteShortOfItsStagingSize();
    failures += testRefusesARoomOneByteShort();
    failures += testRefusesAFileShorterThanTheTable();
#if IS_64_BIT
    failures += testRefusesASizeThatWouldWrap();
#endif
    failures += testRefusesAnOffsetPastTheFile();
    failures += testRefusesAnOffsetAtTheFilesEnd();
    failures += testRefusesAnOffsetInsideTheTable();
    failures += testRefusesTheLastGlyphsOffset();
    failures += testAcceptsAnOffsetAtTheTablesEnd();
    failures += testRefusesAGlyphWhoseRowsPassTheFile();
    failures += testRefusesAGlyphWhoseRoundedWidthPassesTheFile();
    failures += testRefusesAGlyphOfNegativeWidth();
#ifdef GE_LP64_ORACLE
    failures += testOracleFindsTheStagedFontIdentical();
    failures += testOracleFindsAGlyphByteThatDiffers();
    failures += testOracleFindsTheStagedFontListIdentical();
    failures += testOracleFindsAFontListWordThatDiffers();
#endif
    return failures == 0 ? 0 : 1;
}
