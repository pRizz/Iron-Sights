/* The font converter's test fixture (LP64-004 Task 9), shared by
 * test_romconv_font.c and test_romconv_font_checks.c: a small font segment
 * built from numbers written here, the output room, and the checks the tests
 * share. */
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

char s_Log[512];
u8 s_Rom[ZURICH_SIZE];
union FontOut s_Out;

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_Log, sizeof(s_Log), fmt, ap);
    va_end(ap);
}

int startsWith(const char *text, const char *prefix)
{
    while (*prefix != '\0' && *text == *prefix)
    {
        text++;
        prefix++;
    }
    return *prefix == '\0';
}

int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_font: FAIL: %s (log: %s)\n", what, s_Log);
        fflush(stdout); /* before a sanitizer stops a later test */
        return 1;
    }
    return 0;
}

void putBe32(u8 *p, u32 v)
{
    p[0] = (u8) (v >> 24);
    p[1] = (u8) (v >> 16);
    p[2] = (u8) (v >> 8);
    p[3] = (u8) v;
}

u8 *buildRom(void)
{
    u32 i;

    memset(s_Rom, 0, sizeof(s_Rom));
    putBe32(s_Rom + offsetof(RomFont, kerning), 0xFFFFFFFEu);
    putBe32(s_Rom + offsetof(RomFont, kerning) + 168 * 4, 0x55);
    putBe32(s_Rom + CHAR_AT(0, index), 0x21);
    putBe32(s_Rom + CHAR_AT(0, baseline), 2);
    putBe32(s_Rom + CHAR_AT(0, height), 7);
    putBe32(s_Rom + CHAR_AT(0, width), 5);
    putBe32(s_Rom + CHAR_AT(0, kerningindex), 3);
    putBe32(s_Rom + CHAR_AT(0, pixel_offset), GLYPH0_AT);
    for (i = 0; i < 8; i++)
    {
        s_Rom[GLYPH0_AT + i] = (u8) (0xA1 + i);
    }
    return s_Rom;
}

/* Glyph 0 of the test font at pixel_offset AT. */
u8 *romWithGlyph0At(u32 at)
{
    u8 *rom = buildRom();

    putBe32(rom + CHAR_AT(0, pixel_offset), at);
    return rom;
}

u8 *freshOut(void)
{
    memset(s_Out.bytes, FILL, sizeof(s_Out.bytes));
    return s_Out.bytes;
}

int outUntouched(void)
{
    size_t i;

    for (i = 0; i < sizeof(s_Out.bytes); i++)
    {
        if (s_Out.bytes[i] != FILL)
        {
            return 0;
        }
    }
    return 1;
}

struct font *convertTestFont(void)
{
    portFontConvert(buildRom(), ROM_SIZE, freshOut(), OUT_SIZE);
    return &s_Out.font;
}

int expectRefused(const u8 *rom, u32 romsize, u32 outsize, const char *why, const char *what)
{
    int result;

    freshOut();
    s_Log[0] = '\0';
    result = portFontConvert(rom, romsize, s_Out.bytes, outsize);
    return expect(result == 0 && outUntouched() && startsWith(s_Log, REFUSAL)
            && startsWith(s_Log + sizeof(REFUSAL) - 1, why),
            what);
}
