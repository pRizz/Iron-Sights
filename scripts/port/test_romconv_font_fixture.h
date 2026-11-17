/* The font converter's test fixture (test_romconv_font_fixture.c). */
#ifndef TEST_ROMCONV_FONT_FIXTURE_H
#define TEST_ROMCONV_FONT_FIXTURE_H

#include <stddef.h>
#include <ultra64.h>
#include "platform_info.h"
#include <bondtypes.h>
#include "port_romlayout.h"

#define ROM_SIZE 0xB88u    /* the table, 4 spare bytes, eight glyph bytes */
#define GLYPH0_AT 0xB80u   /* glyph 0's pixel_offset */
#define GOTHIC_SIZE 0x24B0u /* the stock segments' ROM sizes */
#define ZURICH_SIZE 0x3540u
#define FILL 0xA5u

/* The bytes struct font is longer than the file's table. */
#define GROWTH ((u32) (sizeof(struct font) - sizeof(RomFont)))
#define OUT_SIZE (ROM_SIZE + GROWTH)

/* The file offset of character CH's field FIELD. */
#define CHAR_AT(ch, field) (offsetof(RomFont, chars) + (ch) * sizeof(RomFontChar) + offsetof(RomFontChar, field))

/* The start of every refusal of the test font. */
#define REFUSAL "ge: lp64: cannot convert fonts \"font\": "

/* Room for the largest image, 8-byte aligned, every byte FILL before a call. */
union FontOut
{
    struct font font;
    u8 bytes[ZURICH_SIZE + 0x400];
};

extern char s_Log[512];          /* port_console.c's last line, captured */
extern u8 s_Rom[ZURICH_SIZE];    /* a segment big enough for the stock fonts */
extern union FontOut s_Out;

int startsWith(const char *text, const char *prefix);
/** 0 when OK; otherwise prints WHAT and returns 1. */
int expect(int ok, const char *what);
void putBe32(u8 *p, u32 v);
/** The test font: kerning[0] -2 and kerning[168] 0x55; glyph 0 index 0x21,
 * baseline 2, height 7, width 5, kerning index 3, its eight bytes 0xA1 to
 * 0xA8 at 0xB80; every other glyph, 93 among them, at pixel_offset 0. */
u8 *buildRom(void);
/** The test font with glyph 0 at pixel_offset AT. */
u8 *romWithGlyph0At(u32 at);
/** s_Out with every byte FILL. */
u8 *freshOut(void);
int outUntouched(void);
/** The test font converted into OUT_SIZE bytes of s_Out. */
struct font *convertTestFont(void);
/** Converts ROM (ROMSIZE bytes) into OUTSIZE bytes, which must be refused,
 * and checks that nothing was written and the refusal names the class, the
 * file and the reason WHY. */
int expectRefused(const u8 *rom, u32 romsize, u32 outsize, const char *why, const char *what);

#endif
