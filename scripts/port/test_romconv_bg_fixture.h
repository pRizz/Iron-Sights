/* The background converter's test fixture (test_romconv_bg_fixture.c). */
#ifndef TEST_ROMCONV_BG_FIXTURE_H
#define TEST_ROMCONV_BG_FIXTURE_H

#include <stddef.h>
#include <ultra64.h>
#include "platform_info.h"
#include "bg.h"

#define FM_SIZE 0x110u
#define ROOMS 5u   /* the zero record, two rooms, the end marker, the zero record */
#define PORTALS 3u /* two portals and the zero entry */
#define TABLES ((u32) (ROOMS * sizeof(bg_room_data) + PORTALS * sizeof(bg_portal_data_entry)))
#define FILL 0xA5u

/* The places of the test front matter. */
#define FM_ROOMS 0x14u   /* room record I at FM_ROOMS + 24 * I */
#define FM_VIS 0x8Cu     /* two visibility commands */
#define FM_PORTALS 0x9Cu /* portal entry I at FM_PORTALS + 8 * I */
#define FM_POINTS0 0xB4u /* portal 0's points: four */
#define FM_POINTS1 0xE8u /* portal 1's points: three, to FM_SIZE */

/* The start of every refusal of the test file, measured and written. */
#define REFUSAL "ge: lp64: cannot convert backgrounds \"test\": "
#define CONVERT_REFUSAL "ge: lp64: cannot convert backgrounds \"bg file\": "

/* Room for the tables, 8-byte aligned, every byte FILL before a call. */
union BgOut
{
    u64 align;
    u8 bytes[TABLES + 32];
};

extern char s_Log[512]; /* port_console.c's last line, captured */
extern u8 s_Fm[FM_SIZE];
extern union BgOut s_Out;
extern bg_room_data *s_Rooms;
extern bg_portal_data_entry *s_Portals;

int startsWith(const char *text, const char *prefix);
/** True when the SIZE bytes at A and B are equal (the game's <string.h>,
 * first on the include path as in the game build, declares no memcmp). */
int sameBytes(const void *a, const void *b, size_t size);
/** 0 when OK; otherwise prints WHAT and returns 1. */
int expect(int ok, const char *what);
void put32(u32 at, u32 value);
/** The test front matter in s_Fm, native byte order: the header {0,
 * 0x0F000014, 0x0F00009C, 0x0F00008C, 0}; a zero record; room 1 {0x0F000200,
 * 0x0F000280, 0x0F000300, (1.5, 12.5, -3.25)}; room 2 {0x0F000400,
 * 0x0F000500, 0, (100, 0, 7)}; the end marker {0x0F000900, 0x0F000980,
 * 0x0F000990}; a zero record; two visibility commands; portal 0
 * {0x0F0000B4, rooms 3 and 4, control bytes 0x01 0x02}, portal 1
 * {0x0F0000E8, rooms 1 and 2, 0x80 0x40}, a zero entry; the points. */
void buildFrontMatter(void);
/** s_Out with every byte FILL, and s_Rooms and s_Portals NULL. */
void freshOut(void);
/** True when s_Out holds FILL from byte FROM to its end. */
int outUntouched(u32 from);
/** The test front matter, measured and converted into s_Out; the result. */
int convertStock(void);
/** Measures and converts s_Fm (SIZE bytes) into OUTSIZE bytes; both must
 * refuse, Size naming "test" and the reason WHY, Convert writing nothing and
 * leaving s_Rooms and s_Portals NULL. */
int expectRefused(u32 size, u32 outsize, const char *why, const char *what);

#endif
