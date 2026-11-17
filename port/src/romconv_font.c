/**
 * The font converter (port_romconv.h; LP64-004 Task 9).
 *
 * A Latin font segment is the 13 x 13 kerning table, 94 character records of
 * six big-endian words, then the glyph bytes; a record's last word is its
 * glyph's offset from the segment's start. At 32 bits portFontBuildSegments
 * (segments.c) swaps the table's words in place and load_font_tables adds the
 * copy's address to each offset, which struct fontchar keeps in its pointer
 * slot. At 64 bits that slot is 8 bytes and 8-byte aligned, so struct font is
 * 0x2F4 bytes longer than the file's table: the image is written here at the
 * run-time layout, every glyph offset moved by that growth and zero-extended,
 * and load_font_tables's fix-up runs unchanged.
 *
 * Compiled only at 64 bits and in the 32-bit oracle build, where the image
 * must equal the 32-bit staging byte for byte.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include <bondtypes.h>
#include "port_romconv.h"
#include "port_romlayout.h"
#ifdef GE_LP64_ORACLE
#include <stdlib.h>
#include "port_lp64_oracle.h"
#endif

#define FONT_FILE "font"
#define FONT_CHARS 94u
#define FONT_GROWTH ((u32) (sizeof(struct font) - sizeof(RomFont)))

_Static_assert(sizeof(((struct font *) 0)->kerning) == sizeof(((RomFont *) 0)->kerning), "kerning tables match");
_Static_assert(sizeof(((struct font *) 0)->chars) / sizeof(struct fontchar) == FONT_CHARS, "94 characters");

u32 portFontConvertedSize(u32 romsize)
{
    if (romsize < sizeof(RomFont) || romsize > UINT32_MAX - FONT_GROWTH)
    {
        return 0;
    }
    return romsize + FONT_GROWTH;
}

/* The ROM record of character CH. */
static const u8 *fontRomChar(const u8 *rom, u32 ch)
{
    return rom + offsetof(RomFont, chars) + ch * sizeof(RomFontChar);
}

/* Refuses, naming the glyph, a non-zero pixel offset outside the glyph bytes
 * [sizeof(RomFont), ROMSIZE). Reads only. */
static int fontOffsetsInGlyphBytes(const u8 *rom, u32 romsize)
{
    u32 ch;

    for (ch = 0; ch < FONT_CHARS; ch++)
    {
        u32 at = portConvBe32(fontRomChar(rom, ch) + offsetof(RomFontChar, pixel_offset));

        if (at != 0 && (at < sizeof(RomFont) || at >= romsize))
        {
            portConvRefuse(PORT_LP64_FONTS, FONT_FILE,
                    "glyph %u's pixel offset 0x%X is outside the glyph bytes (0x%X to 0x%X)", ch, at,
                    (u32) sizeof(RomFont), romsize);
            return 0;
        }
    }
    return 1;
}

/* Refuses, with nothing read past the table, a file or room that cannot hold
 * the image; returns the image's size, or 0 after a refusal. */
static u32 fontMeasure(const u8 *rom, u32 romsize, u32 outsize)
{
    u32 size;

    if (romsize < sizeof(RomFont))
    {
        portConvRefuse(PORT_LP64_FONTS, FONT_FILE, "the %u-byte file is shorter than its %u-byte table", romsize,
                (u32) sizeof(RomFont));
        return 0;
    }
    size = portFontConvertedSize(romsize);
    if (size == 0)
    {
        portConvRefuse(PORT_LP64_FONTS, FONT_FILE, "the %u-byte file's image would pass 4 GB", romsize);
        return 0;
    }
    if (outsize < size)
    {
        portConvRefuse(PORT_LP64_FONTS, FONT_FILE, "the font needs %u bytes, the room is %u", size, outsize);
        return 0;
    }
    return fontOffsetsInGlyphBytes(rom, romsize) ? size : 0;
}

int portFontConvert(const u8 *rom, u32 romsize, u8 *out, u32 outsize)
{
    u32 i;

    if (fontMeasure(rom, romsize, outsize) == 0)
    {
        return 0;
    }
    /* Field by field through locals and memcpy: OUT may have any alignment,
     * and the padding of a 64-bit struct font is written as zero. */
    memset(out, 0, sizeof(struct font));
    for (i = 0; i < 13 * 13; i++)
    {
        s32 kern = (s32) portConvBe32(rom + offsetof(RomFont, kerning) + i * 4);

        memcpy(out + offsetof(struct font, kerning) + i * sizeof(kern), &kern, sizeof(kern));
    }
    for (i = 0; i < FONT_CHARS; i++)
    {
        const u8 *src = fontRomChar(rom, i);
        u32 at = portConvBe32(src + offsetof(RomFontChar, pixel_offset));
        struct fontchar ch;

        memset(&ch, 0, sizeof(ch));
        ch.index = (s32) portConvBe32(src + offsetof(RomFontChar, index));
        ch.baseline = (s32) portConvBe32(src + offsetof(RomFontChar, baseline));
        ch.height = (s32) portConvBe32(src + offsetof(RomFontChar, height));
        ch.width = (s32) portConvBe32(src + offsetof(RomFontChar, width));
        ch.kerningindex = (s32) portConvBe32(src + offsetof(RomFontChar, kerningindex));
        /* at < romsize <= UINT32_MAX - FONT_GROWTH, so the sum cannot wrap. */
        ch.pixeldata = at == 0 ? NULL : (u8 *) (uintptr_t) (at + FONT_GROWTH);
        memcpy(out + offsetof(struct font, chars) + i * sizeof(ch), &ch, sizeof(ch));
    }
    memcpy(out + sizeof(struct font), rom + sizeof(RomFont), romsize - (u32) sizeof(RomFont));
    return 1;
}

#ifdef GE_LP64_ORACLE
void portFontOracle(const char *name, const u8 *staged, const u8 *rom, u32 romsize)
{
    u32 size = portFontConvertedSize(romsize);
    u8 *converted;

    if (size == 0)
    {
        portLp64OracleResult(PORT_LP64_FONTS, name, 0, "the segment is shorter than its table");
        return;
    }
    converted = (u8 *) malloc(size);
    if (converted == NULL)
    {
        portLp64OracleResult(PORT_LP64_FONTS, name, 0, "the oracle could not allocate the image");
        return;
    }
    if (!portFontConvert(rom, romsize, converted, size))
    {
        portLp64OracleResult(PORT_LP64_FONTS, name, 0, "the converter refused the font");
    }
    else
    {
        portLp64OracleBytes(PORT_LP64_FONTS, name, staged, converted, size);
    }
    free(converted);
}

void portFontListOracle(const u8 *staged, const u8 *rom)
{
    Gfx *converted = (Gfx *) malloc(PORT_FONTDL_COMMANDS * sizeof(Gfx));

    if (converted == NULL)
    {
        portLp64OracleResult(PORT_LP64_FONTS, "fontdl", 0, "the oracle could not allocate the list");
        return;
    }
    portGfxFromRom(converted, rom, PORT_FONTDL_COMMANDS);
    portLp64OracleBytes(PORT_LP64_FONTS, "fontdl", staged, converted, PORT_FONTDL_COMMANDS * (u32) sizeof(Gfx));
    free(converted);
}
#endif
