/* The font converter (LP64-004 Task 9): portFontConvertedSize measures the
 * run-time image of a big-endian font segment, and portFontConvert writes it
 * (a struct font with native fields, each pixel offset moved by the growth of
 * the table and zero-extended into pixeldata, then the glyph bytes). The stock
 * segments' sizes are pinned against segments.c's staging arrays. The
 * refusals and the oracle are in test_romconv_font_checks.c. Built and run at
 * each pointer width by test_romconv.sh. Every input is written here and in
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

static int testTheTableGrowsByItsPointerSlots(void)
{
    // Assert
    return expect(GROWTH == (IS_64_BIT ? 0x2F4u : 0u), "struct font is 0x2F4 bytes longer at 64 bits, as long at 32");
}

static int testMeasuresTheRunTimeImage(void)
{
    // Act
    u32 n = portFontConvertedSize(ROM_SIZE);

    // Assert
    return expect(n == ROM_SIZE - 0xB74 + sizeof(struct font), "the image is the file less its table plus struct font");
}

static int testMeasuresBankGothicAtItsStagingSize(void)
{
    // Act
    u32 n = portFontConvertedSize(GOTHIC_SIZE);

    // Assert: segments.c's staging array and its labels at each width.
    return expect(n == (IS_64_BIT ? 0x24B0u + 0x2F4u : 0x24B0u), "Bank Gothic measures 0x24B0 + 0x2F4 at 64 bits");
}

static int testMeasuresZurichBoldAtItsStagingSize(void)
{
    // Act
    u32 n = portFontConvertedSize(ZURICH_SIZE);

    // Assert: segments.c's staging array and its labels at each width.
    return expect(n == (IS_64_BIT ? 0x3540u + 0x2F4u : 0x3540u), "Zurich Bold measures 0x3540 + 0x2F4 at 64 bits");
}

static int testStagesBankGothicInWholeSixteens(void)
{
    // Act
    u32 staged = PORT_FONT_STAGED_SIZE(portFontConvertedSize(GOTHIC_SIZE));

    // Assert: segments.c's span and array, which the stage bank allocates.
    return expect(staged == (IS_64_BIT ? 0x24B0u + 0x300u : 0x24B0u), "Bank Gothic is staged in 0x24B0 + 0x300 bytes");
}

static int testStagesZurichBoldInWholeSixteens(void)
{
    // Act
    u32 staged = PORT_FONT_STAGED_SIZE(portFontConvertedSize(ZURICH_SIZE));

    // Assert: segments.c's span and array, which the stage bank allocates.
    return expect(staged == (IS_64_BIT ? 0x3540u + 0x300u : 0x3540u), "Zurich Bold is staged in 0x3540 + 0x300 bytes");
}

static int testTheFontListFillsItsStagingSize(void)
{
    // Assert: segments.c's font list label at each width.
    return expect(PORT_FONTDL_COMMANDS * sizeof(Gfx) == (IS_64_BIT ? 0x180u : 0xC0u),
            "the font list's 24 commands are 0x180 bytes at 64 bits");
}

static int testMeasuresNothingBelowTheTable(void)
{
    // Act
    u32 n = portFontConvertedSize(0xB73);

    // Assert
    return expect(n == 0, "a file one byte shorter than the table measures 0");
}

static int testMeasuresTheLargestFileWithoutWrapping(void)
{
    // Act
    u32 n = portFontConvertedSize(0xFFFFFFFFu);

    // Assert: 0 where the growth would wrap; the file's size at 32 bits.
    return expect(n == (IS_64_BIT ? 0u : 0xFFFFFFFFu), "a 0xFFFFFFFF-byte file measures 0 at 64 bits, itself at 32");
}

static int testConverts(void)
{
    // Act
    int result = portFontConvert(buildRom(), ROM_SIZE, freshOut(), OUT_SIZE);

    // Assert
    return expect(result == 1, "the test font converts");
}

static int testWritesTheKerning(void)
{
    // Act
    struct font *f = convertTestFont();

    // Assert
    return expect(f->kerning[0] == -2 && f->kerning[168] == 0x55, "kerning[0] is -2 and kerning[168] 0x55, native");
}

static int testWritesTheMetrics(void)
{
    // Act
    struct font *f = convertTestFont();

    // Assert
    return expect(f->chars[0].index == 0x21 && f->chars[0].baseline == 2 && f->chars[0].height == 7
            && f->chars[0].width == 5 && f->chars[0].kerningindex == 3,
            "glyph 0's index, baseline, height, width and kerning index are native");
}

static int testMovesThePixelOffset(void)
{
    // Act
    struct font *f = convertTestFont();

    // Assert
    return expect((uintptr_t) f->chars[0].pixeldata == 0xB80 - 0xB74 + sizeof(struct font),
            "glyph 0's pixeldata is its offset moved by the table's growth");
}

static int testCopiesTheGlyphBytes(void)
{
    // Act
    struct font *f = convertTestFont();
    const u8 *glyph = s_Out.bytes + (uintptr_t) f->chars[0].pixeldata;

    // Assert
    return expect(glyph[0] == 0xA1 && glyph[7] == 0xA8, "glyph 0's first and last bytes are at its pixeldata");
}

static int testKeepsAZeroOffsetNull(void)
{
    // Act
    struct font *f = convertTestFont();

    // Assert
    return expect(f->chars[93].pixeldata == NULL, "glyph 93's pixel offset 0 stays NULL");
}

static int testAcceptsTheFilesLastByte(void)
{
    // Arrange
    u8 *rom = romWithGlyph0At(ROM_SIZE - 1);

    // Act
    int result = portFontConvert(rom, ROM_SIZE, freshOut(), OUT_SIZE);

    // Assert
    return expect(result == 1 && (uintptr_t) s_Out.font.chars[0].pixeldata == ROM_SIZE - 1 + GROWTH,
            "a glyph at the file's last byte converts to the image's last byte");
}

static int testZeroesThePadding(void)
{
    // Act
    convertTestFont();

    // Assert: at 64 bits the 4 bytes after the kerning table and the 4 before
    // glyph 0's pixeldata; at 32 bits there are none and both spans are empty.
    const u8 *afterKerning = s_Out.bytes + sizeof(s_Out.font.kerning);
    const u8 *beforePixels = s_Out.bytes + offsetof(struct font, chars[0].kerningindex) + 4;
    size_t padKerning = offsetof(struct font, chars) - sizeof(s_Out.font.kerning);
    size_t padChar = offsetof(struct fontchar, pixeldata) - offsetof(struct fontchar, kerningindex) - 4;
    size_t i;
    int zero = padKerning == (IS_64_BIT ? 4u : 0u) && padChar == padKerning;

    for (i = 0; i < padKerning; i++)
    {
        zero = zero && afterKerning[i] == 0 && beforePixels[i] == 0;
    }
    return expect(zero, "the padding of the run-time table is zero");
}

static int testWritesNothingPastTheImage(void)
{
    // Act
    convertTestFont();

    // Assert
    return expect(s_Out.bytes[OUT_SIZE] == FILL, "the byte after the image is untouched");
}

static int testConvertsBankGothicIntoItsStagingSize(void)
{
    // Arrange: a Bank Gothic-sized segment whose glyph 0 is its last byte.
    u8 *rom = romWithGlyph0At(GOTHIC_SIZE - 1);

    // Act
    int result = portFontConvert(rom, GOTHIC_SIZE, freshOut(), IS_64_BIT ? 0x24B0u + 0x2F4u : 0x24B0u);

    // Assert
    return expect(result == 1 && s_Out.bytes[(IS_64_BIT ? 0x24B0u + 0x2F4u : 0x24B0u)] == FILL,
            "Bank Gothic fills its staging size exactly");
}

static int testConvertsZurichBoldIntoItsStagingSize(void)
{
    // Arrange: a Zurich Bold-sized segment whose glyph 0 is its last byte.
    u8 *rom = romWithGlyph0At(ZURICH_SIZE - 1);

    // Act
    int result = portFontConvert(rom, ZURICH_SIZE, freshOut(), IS_64_BIT ? 0x3540u + 0x2F4u : 0x3540u);

    // Assert
    return expect(result == 1 && s_Out.bytes[(IS_64_BIT ? 0x3540u + 0x2F4u : 0x3540u)] == FILL,
            "Zurich Bold fills its staging size exactly");
}

int main(void)
{
    int failures = 0;

    failures += testTheTableGrowsByItsPointerSlots();
    failures += testMeasuresTheRunTimeImage();
    failures += testMeasuresBankGothicAtItsStagingSize();
    failures += testMeasuresZurichBoldAtItsStagingSize();
    failures += testStagesBankGothicInWholeSixteens();
    failures += testStagesZurichBoldInWholeSixteens();
    failures += testTheFontListFillsItsStagingSize();
    failures += testMeasuresNothingBelowTheTable();
    failures += testMeasuresTheLargestFileWithoutWrapping();
    failures += testConverts();
    failures += testWritesTheKerning();
    failures += testWritesTheMetrics();
    failures += testMovesThePixelOffset();
    failures += testCopiesTheGlyphBytes();
    failures += testKeepsAZeroOffsetNull();
    failures += testAcceptsTheFilesLastByte();
    failures += testZeroesThePadding();
    failures += testWritesNothingPastTheImage();
    failures += testConvertsBankGothicIntoItsStagingSize();
    failures += testConvertsZurichBoldIntoItsStagingSize();
    return failures == 0 ? 0 : 1;
}
