/* The setup file converter's fixture builder (LP64-004 Task 14): builds a
 * big-endian setup file from calls that return each item's offset, so the
 * tests name things by what they are. Every byte is written here with
 * invented values; nothing is read from a ROM. Shared by test_romconv_setup.c
 * and test_romconv_setup_checks.c. */
#ifndef TEST_ROMCONV_SETUP_BUILD_H
#define TEST_ROMCONV_SETUP_BUILD_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "bondconstants.h"
#include "platform_info.h"
#include "port_romconv.h"
#include "port_romconv_stage.h"

#define SF_CAP 0x800u
#define SF_OUT_CAP (3u * SF_CAP)

/* The game's <string.h>, first on the include path, declares no memcmp; the
 * compiler's own are called by name. */
#define sameBytes(a, b, size) (__builtin_memcmp((a), (b), (size)) == 0)
#define contains(text, part) (__builtin_strstr((text), (part)) != NULL)
#define sameText(a, b) (__builtin_strcmp((a), (b)) == 0)

static u8 g_File[SF_CAP];
static u32 g_Size;
static u8 g_Out[SF_OUT_CAP] __attribute__((aligned(16)));
static char g_Log[512];

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(g_Log, sizeof(g_Log), fmt, ap);
    va_end(ap);
}

static inline int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_setup: FAIL: %s (log: %s)\n", what, g_Log);
        fflush(stdout); /* before a sanitizer stops a later test */
        return 1;
    }
    return 0;
}

static inline int allBytes(const u8 *p, size_t size, u8 value)
{
    size_t i;

    for (i = 0; i < size && p[i] == value; i++)
    {
    }
    return i == size;
}

static inline void sfPut8(u32 at, u32 v) { g_File[at] = (u8) v; }
static inline void sfPut16(u32 at, u32 v) { g_File[at] = (u8) (v >> 8); g_File[at + 1] = (u8) v; }
static inline void sfPut32(u32 at, u32 v) { sfPut16(at, v >> 16); sfPut16(at + 2, v & 0xFFFF); }
static inline u32 sfGet32(u32 at)
{
    return ((u32) g_File[at] << 24) | ((u32) g_File[at + 1] << 16) | ((u32) g_File[at + 2] << 8) | g_File[at + 3];
}
static inline u32 sfFloat(f32 f)
{
    u32 bits;

    __builtin_memcpy(&bits, &f, 4);
    return bits;
}

/** An empty file of a zero header; g_Out 0xAA. */
static inline void sfBegin(void)
{
    __builtin_memset(g_File, 0, sizeof(g_File));
    __builtin_memset(g_Out, 0xAA, sizeof(g_Out));
    g_Log[0] = '\0';
    g_Size = 40;
}

/** Points header word FIELD (RomSetupHeader's order) at AT. */
static inline void sfHeader(u32 field, u32 at) { sfPut32(field * 4, at); }

/** Appends N words (each a 32-bit value) and returns their offset. */
static inline u32 sfWords(u32 n, ...)
{
    const u32 at = g_Size;
    va_list ap;
    u32 i;

    va_start(ap, n);
    for (i = 0; i < n; i++)
    {
        sfPut32(g_Size, va_arg(ap, u32));
        g_Size += 4;
    }
    va_end(ap);
    return at;
}

/** Appends a pad at POS with name offset PLINK (0 for the terminator). */
static inline u32 sfPad(f32 x, f32 y, f32 z, u32 plink)
{
    return sfWords(11, sfFloat(x), sfFloat(y), sfFloat(z), sfFloat(0.0f), sfFloat(1.0f), sfFloat(0.0f),
        sfFloat(0.0f), sfFloat(0.0f), sfFloat(1.0f), plink, 0x51u);
}

/** Appends a bound pad: a pad, then its box of six floats from XMIN in steps of 1. */
static inline u32 sfBoundPad(f32 x, u32 plink, f32 xmin)
{
    const u32 at = sfPad(x, 2.0f, 3.0f, plink);
    u32 i;

    for (i = 0; i < 6; i++)
    {
        sfWords(1, sfFloat(xmin + (f32) i));
    }
    return at;
}

/** Appends BYTES of byte data and returns its offset. */
static inline u32 sfBytes(const u8 *bytes, u32 n)
{
    const u32 at = g_Size;

    __builtin_memcpy(g_File + g_Size, bytes, n);
    g_Size += n;
    return at;
}

/** Pads the file to a multiple of 4 with zero bytes. */
static inline void sfAlign4(void) { g_Size = (g_Size + 3) & ~3u; }

/** Appends a credits entry. */
static inline u32 sfCredit(u32 text1, u32 text2, u32 pos1, u32 align1, u32 pos2, u32 align2)
{
    const u32 at = g_Size;

    sfPut16(at, text1);
    sfPut16(at + 2, text2);
    sfPut16(at + 4, pos1);
    sfPut16(at + 6, align1);
    sfPut16(at + 8, pos2);
    sfPut16(at + 10, align2);
    g_Size += 12;
    return at;
}

/** An intro record's type word: the type in its low byte. */
#define INTRO_WORD(type) ((u32) (type))

/* ==== the main fixture ================================================== */

struct SetupFixture
{
    u32 pads, boundpads, waypoints, waygroups, paths, ailists, padnames, boundpadnames, intro, credits, props;
    u32 wp0List, wp1List, wg0Neighbours, wg0Points, path0List, wp2List; /* wp2List: inside wp0List */
    u32 names, p0, p1, tail1;                                            /* p0 at an odd offset; tail1 is "1" of "p1" */
    u32 code, ai0, ai1;                                                  /* ai0 odd; ai1 inside ai0's code */
};

#define AI0_ID 0x0400u
#define AI1_ID 0x0401u
#define CAM_TEXT1 0x1234u
#define CAM_TEXT2 0x5678u

/* Two pads and the terminator, one bound pad, three waypoints with neighbour
 * lists, one waygroup, one path, two AI lists with byte code, two pad names,
 * an intro of a spawn, a camera and credits with two entries, and only the
 * end record for props. Names and AI code sit between the tables. With
 * NESTED, waypoint 2's list starts inside waypoint 0's; without it, it has
 * its own. */
static inline void sfBuildFixture(struct SetupFixture *f, int nested)
{
    static const u8 names[] = {'x', 'p', '0', 0, 'p', '1', 0, 0};
    u8 code[12] = {0x99, 0x2A, 0xAD, 0x11, 0, 0, 0, 0, 0, 0, 0, 0};

    code[4] = (u8) AI_EndList;
    sfBegin();
    f->names = sfBytes(names, sizeof(names));
    f->p0 = f->names + 1;
    f->p1 = f->names + 4;
    f->tail1 = f->names + 5;
    f->pads = sfPad(10.0f, 20.0f, -30.0f, f->p0);
    sfPad(1.0f, 2.0f, -40.0f, f->p1);
    sfPad(0.0f, 0.0f, 0.0f, 0);
    f->boundpads = sfBoundPad(5.0f, f->tail1, -7.0f);
    sfBoundPad(0.0f, 0, 0.0f);
    f->wp0List = sfWords(3, 1u, 2u, (u32) -1);
    f->wp1List = sfWords(2, 0u, (u32) -1);
    f->wp2List = f->wp0List + 4;
    f->wg0Neighbours = sfWords(1, (u32) -1);
    f->wg0Points = sfWords(4, 0u, 1u, 2u, (u32) -1);
    f->path0List = sfWords(3, 0u, 1u, (u32) -1);
    if (!nested)
    {
        f->wp2List = sfWords(2, 2u, (u32) -1);
    }
    f->waypoints = sfWords(4, 0u, f->wp0List, 0u, 0u);
    sfWords(4, 1u, f->wp1List, 0u, 0u);
    sfWords(4, 2u, f->wp2List, 0u, 0u);
    sfWords(4, (u32) -1, 0u, 0u, 0u);
    f->waygroups = sfWords(3, f->wg0Neighbours, f->wg0Points, 0u);
    sfWords(3, 0u, 0u, 0u);
    f->paths = sfWords(2, f->path0List, 0x03010005u); /* ID 3, isLoop 1, len 5 */
    sfWords(2, 0u, 0u);
    f->code = sfBytes(code, sizeof(code));
    f->ai0 = f->code + 1;
    f->ai1 = f->code + 2;
    f->ailists = sfWords(2, f->ai0, AI0_ID);
    sfWords(2, f->ai1, AI1_ID);
    sfWords(2, 0u, 0u);
    f->padnames = sfWords(3, f->p0, f->p1, 0u);
    f->boundpadnames = sfWords(2, f->tail1, 0u);
    f->credits = sfCredit(0x0101, 0x0102, 10, 1, 20, 2);
    sfCredit(0x0103, 0x0104, 30, 0, 40, 1);
    sfCredit(0, 0, 0, 0, 0, 0);
    f->intro = sfWords(3, INTRO_WORD(INTROTYPE_SPAWN), 1u, 0u);
    sfWords(10, INTRO_WORD(INTROTYPE_CAMERA), 100u, 200u, 300u, 400u, 500u, 6u, CAM_TEXT1, CAM_TEXT2, 0u);
    sfWords(2, INTRO_WORD(INTROTYPE_CREDITS), f->credits);
    sfWords(1, INTRO_WORD(INTROTYPE_END));
    f->props = sfWords(1, (u32) PROPDEF_END);
    sfHeader(0, f->waypoints);
    sfHeader(1, f->waygroups);
    sfHeader(2, f->intro);
    sfHeader(3, f->props);
    sfHeader(4, f->paths);
    sfHeader(5, f->ailists);
    sfHeader(6, f->pads);
    sfHeader(7, f->boundpads);
    sfHeader(8, f->padnames);
    sfHeader(9, f->boundpadnames);
}

/** The main fixture, waypoint 2's list starting inside waypoint 0's. */
static inline void sfBuildMain(struct SetupFixture *f)
{
    sfBuildFixture(f, 1);
}

/** Measures and converts g_File into g_Out; returns the size, or 0. */
static inline u32 sfConvert(void)
{
    const u32 n = portSetupConvertedSize(g_File, g_Size, "UsetuptestZ");

    if (n == 0 || n > SF_OUT_CAP || portSetupConvert(g_File, g_Size, g_Out, n, "UsetuptestZ") != 1)
    {
        return 0;
    }
    return n;
}

/** Whether the file is refused by both entry points with g_Out untouched, the log naming PART. */
static inline int sfRefused(const char *part)
{
    const int measured = portSetupConvertedSize(g_File, g_Size, "UsetuptestZ") == 0;
    const int named = contains(g_Log, "cannot convert setup files \"UsetuptestZ\"") && contains(g_Log, part);

    g_Log[0] = '\0';
    return measured && named && portSetupConvert(g_File, g_Size, g_Out, SF_OUT_CAP, "UsetuptestZ") == 0
        && contains(g_Log, part) && allBytes(g_Out, sizeof(g_Out), 0xAA);
}

#define AT(type, base, off) ((type *) (void *) ((base) + (uintptr_t) (off)))

#endif
