/* The animation table converter (LP64-004 Task 3): portAnimHeadersConvert
 * writes each listed header's run-time ModelAnimation over the file header in
 * the table, with pointers into the staging image, and replaces the list's
 * entries with the headers' addresses; it refuses, writing nothing, a header
 * that does not fit or two that overlap. Built and run at each pointer width
 * by test_romconv.sh (the 64-bit half under the address and undefined-
 * behaviour sanitizers, since the 64-bit ModelAnimation is packed). Every
 * input is written here. */
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

#if IS_64_BIT
_Static_assert(offsetof(ModelAnimation, bitStream) == 24, "64-bit ModelAnimation.bitStream is at 24");
_Static_assert(_Alignof(ModelAnimation) == 4, "64-bit ModelAnimation is 4-byte aligned");
#else
_Static_assert(sizeof(ModelAnimation) == 0x40, "32-bit ModelAnimation is unchanged");
#endif

#define IMAGE_SIZE 0x100u

/* port_console.c's log, captured so the refusal line can be read back. */
static char s_Log[512];

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_Log, sizeof(s_Log), fmt, ap);
    va_end(ap);
}

static int startsWith(const char *text, const char *prefix)
{
    while (*prefix != '\0' && *text == *prefix)
    {
        text++;
        prefix++;
    }
    return *prefix == '\0';
}

/* The game's <string.h> (first on the include path, as in the game build)
 * declares no memcmp. */
static int sameBytes(const void *a, const void *b, size_t size)
{
    const u8 *x = (const u8 *) a;
    const u8 *y = (const u8 *) b;
    size_t i;

    for (i = 0; i < size; i++)
    {
        if (x[i] != y[i])
        {
            return 0;
        }
    }
    return 1;
}

static int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_anim: FAIL: %s\n", what);
        return 1;
    }
    return 0;
}

/* A staged animation_data image of 0x100 bytes: three headers at 0x1C, 0x60
 * and 0xA4 (0x1C and 0xA4 at 4 mod 8), the first's two descriptors at 0x30
 * and its stream at 0x40; every other byte a pattern. `table` starts as a copy
 * of it, as the permanent copy does in the game. */
typedef struct AnimFixture
{
    u8 staged[IMAGE_SIZE];
    u8 table[IMAGE_SIZE];
    intptr_t list[6];
    intptr_t *lists[1];
} AnimFixture;

static u8 s_Entries[0x3000]; /* the animation_entries stand-in */

static void putHeader(u8 *image, u32 offset, u32 entries, u16 frames, u32 descriptors, u16 frameBits, u32 stream)
{
    RomAnimHeader header = {0};

    header.entries_offset = entries;
    header.frames = frames;
    header.unk06 = 3;
    header.unk07 = 1;
    header.descriptors_offset = descriptors;
    header.unk0c = 0x12;
    header.frame_bits = frameBits;
    header.stream_offset = stream;
    memcpy(image + offset, &header, sizeof(header));
}

static void putDescriptor(u8 *image, u32 offset, u16 bitOffset, u8 bitCount, u16 valueOffset)
{
    ModelAnimBitField field = {0};

    field.bitOffset = bitOffset;
    field.bitCount = bitCount;
    field.valueOffset = valueOffset;
    memcpy(image + offset, &field, sizeof(field));
}

static void arrange(AnimFixture *f, const intptr_t *list, size_t count)
{
    u32 i;

    for (i = 0; i < IMAGE_SIZE; i++)
    {
        f->staged[i] = (u8) (i ^ 0x5A);
    }
    putHeader(f->staged, 0x1C, 0x2B4C, 77, 0x30, 0x260, 0x40);
    putDescriptor(f->staged, 0x30, 0, 5, 0);
    putDescriptor(f->staged, 0x36, 5, 7, 0x10);
    putHeader(f->staged, 0x60, 0x100, 10, 0x80, 0x48, 0x90);
    putHeader(f->staged, 0xA4, 0x200, 12, 0xB8, 0x50, 0xC0);
    memcpy(f->table, f->staged, IMAGE_SIZE);
    memset(f->list, 0, sizeof(f->list));
    memcpy(f->list, list, count * sizeof(*list));
    f->lists[0] = f->list;
}

static const intptr_t kStockList[] = {0x1C, 1, 0x60, 0xA4, 0x1C, 0};

static int convert(AnimFixture *f)
{
    return portAnimHeadersConvert(f->table, IMAGE_SIZE, f->staged, IMAGE_SIZE, s_Entries, f->lists, 1);
}

static int testWritesTheRunTimeHeader(void)
{
    // Arrange
    static AnimFixture f;
    arrange(&f, kStockList, 6);

    // Act
    int ok = convert(&f);

    // Assert
    ModelAnimation *a = (ModelAnimation *) (f.table + 0x1C);
    return expect(ok == 1 && a->address == (intptr_t) (s_Entries + 0x2B4C) && a->unk04 == 77 && a->unk06 == 3
            && a->unk07 == 1 && a->unk0C == 0x12 && a->unk0E == 0x260,
            "the header at 0x1C has the entries address and the file's fields");
}

static int testPointsIntoTheStagingImage(void)
{
    // Arrange
    static AnimFixture f;
    arrange(&f, kStockList, 6);

    // Act
    int ok = convert(&f);

    // Assert
    ModelAnimation *a = (ModelAnimation *) (f.table + 0x1C);
    return expect(ok == 1 && (u8 *) a->bitDescriptors == f.staged + 0x30 && a->bitStream == f.staged + 0x40,
            "bitDescriptors and bitStream lead into the staging image");
}

static int testDescriptorsReadThroughThePointer(void)
{
    // Arrange
    static AnimFixture f;
    arrange(&f, kStockList, 6);

    // Act
    int ok = convert(&f);

    // Assert
    ModelAnimation *a = (ModelAnimation *) (f.table + 0x1C);
    return expect(ok == 1 && a->bitDescriptors[1].bitCount == 7 && a->bitDescriptors[1].valueOffset == 0x10,
            "the second descriptor reads back through bitDescriptors");
}

static int testReplacesTheEntries(void)
{
    // Arrange
    static AnimFixture f;
    arrange(&f, kStockList, 6);

    // Act
    int ok = convert(&f);

    // Assert
    return expect(ok == 1 && (u8 *) f.list[0] == f.table + 0x1C && f.list[1] == 1
            && (u8 *) f.list[2] == f.table + 0x60 && (u8 *) f.list[3] == f.table + 0xA4 && f.list[4] == f.list[0]
            && f.list[5] == 0,
            "each entry is its header's address; 1, the repeat and the end stay what they mean");
}

static int testWritesNothingPastTheHeader(void)
{
    // Arrange
    static AnimFixture f;
    arrange(&f, kStockList, 6);

    // Act
    int ok = convert(&f);

    // Assert
    int outsideChanged = 0;
    u32 i;
    for (i = 0; i < IMAGE_SIZE; i++)
    {
        int inHeader = (i >= 0x1C && i < 0x3C) || (i >= 0x60 && i < 0x80) || (i >= 0xA4 && i < 0xC4);
        outsideChanged += !inHeader && f.table[i] != f.staged[i];
    }
    return expect(ok == 1 && f.table[0x1C + 0x20] == f.staged[0x1C + 0x20] && outsideChanged == 0,
            "no byte outside a header's 0x20 is written");
}

static int testLeavesTheStagingImage(void)
{
    // Arrange
    static AnimFixture f;
    static u8 before[IMAGE_SIZE];
    arrange(&f, kStockList, 6);
    memcpy(before, f.staged, IMAGE_SIZE);

    // Act
    int ok = convert(&f);

    // Assert
    return expect(ok == 1 && sameBytes(before, f.staged, IMAGE_SIZE), "the staging image is not written");
}

static int expectRefused(AnimFixture *f, const intptr_t *list, size_t count, const char *what)
{
    int ok;

    s_Log[0] = '\0';
    ok = convert(f);
    return expect(ok == 0 && sameBytes(f->table, f->staged, IMAGE_SIZE)
            && sameBytes(f->list, list, count * sizeof(*list))
            && startsWith(s_Log, "ge: lp64: cannot convert animation tables \"animation_data\": "),
            what);
}

static int testRefusesAHeaderPastTheEnd(void)
{
    // Arrange
    static AnimFixture f;
    static const intptr_t list[] = {0x1C, 0xF0, 0};
    arrange(&f, list, 3);

    // Act, Assert
    return expectRefused(&f, list, 3, "a header at 0xF0 (0x20 past it is outside) is refused, nothing written");
}

static int testRefusesOverlappingHeaders(void)
{
    // Arrange
    static AnimFixture f;
    static const intptr_t list[] = {0x1C, 0x30, 0};
    arrange(&f, list, 3);

    // Act, Assert
    return expectRefused(&f, list, 3, "headers at 0x1C and 0x30 (0x14 apart) are refused, nothing written");
}

int main(void)
{
    int failures = 0;

    failures += testWritesTheRunTimeHeader();
    failures += testPointsIntoTheStagingImage();
    failures += testDescriptorsReadThroughThePointer();
    failures += testReplacesTheEntries();
    failures += testWritesNothingPastTheHeader();
    failures += testLeavesTheStagingImage();
    failures += testRefusesAHeaderPastTheEnd();
    failures += testRefusesOverlappingHeaders();
    return failures == 0 ? 0 : 1;
}
