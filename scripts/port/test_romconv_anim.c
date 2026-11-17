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
/* The bytes the converter writes at a header: 0x20 at 64 bits, the file
 * header's 0x14 at 32. */
#define HEADER_WRITTEN offsetof(ModelAnimation, unk14)

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

static int convertStaged(AnimFixture *f, u32 stagedsize)
{
    return portAnimHeadersConvert(f->table, IMAGE_SIZE, f->staged, stagedsize, s_Entries, f->lists, 1);
}

/* Replaces the header at OFFSET in both images (the table is a copy). */
static void replaceHeader(AnimFixture *f, u32 offset, u32 descriptors, u32 stream)
{
    putHeader(f->staged, offset, 0x300, 9, descriptors, 0x40, stream);
    memcpy(f->table + offset, f->staged + offset, sizeof(RomAnimHeader));
}

static int inWritten(u32 i, const u32 *headers, int count)
{
    int h;

    for (h = 0; h < count; h++)
    {
        if (i >= headers[h] && i < headers[h] + HEADER_WRITTEN)
        {
            return 1;
        }
    }
    return 0;
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

static int testWritesEveryHeader(void)
{
    // Arrange
    static AnimFixture f;
    arrange(&f, kStockList, 6);

    // Act
    int ok = convert(&f);

    // Assert
    ModelAnimation *b = (ModelAnimation *) (f.table + 0x60);
    ModelAnimation *c = (ModelAnimation *) (f.table + 0xA4);
    return expect(ok == 1 && b->unk04 == 10 && b->address == (intptr_t) (s_Entries + 0x100)
            && b->bitStream == f.staged + 0x90 && c->unk04 == 12 && c->unk0E == 0x50
            && (u8 *) c->bitDescriptors == f.staged + 0xB8,
            "the headers at 0x60 and 0xA4 are written too");
}

static int testConvertsAcrossTwoTables(void)
{
    // Arrange
    static AnimFixture f;
    static intptr_t first[] = {0x1C, 0x60, 0};
    static intptr_t second[] = {0xA4, 1, 0x1C, 0};
    intptr_t *lists[2] = {first, second};
    arrange(&f, kStockList, 6);

    // Act
    int ok = portAnimHeadersConvert(f.table, IMAGE_SIZE, f.staged, IMAGE_SIZE, s_Entries, lists, 2);

    // Assert
    ModelAnimation *c = (ModelAnimation *) (f.table + 0xA4);
    return expect(ok == 1 && (u8 *) first[0] == f.table + 0x1C && (u8 *) first[1] == f.table + 0x60
            && (u8 *) second[0] == f.table + 0xA4 && second[1] == 1 && second[2] == first[0] && second[3] == 0
            && c->unk04 == 12 && c->address == (intptr_t) (s_Entries + 0x200),
            "a second table is converted, and a header repeated across the tables is the same address");
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
    static const u32 headers[] = {0x1C, 0x60, 0xA4};
    int outsideChanged = 0;
    u32 i;
    for (i = 0; i < IMAGE_SIZE; i++)
    {
        outsideChanged += !inWritten(i, headers, 3) && f.table[i] != f.staged[i];
    }
    return expect(ok == 1 && f.table[0x1C + 0x20] == f.staged[0x1C + 0x20] && outsideChanged == 0,
            "no byte past a header's written fields (0x20 at 64 bits, 0x14 at 32) is written");
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

static int expectRefusedStaged(AnimFixture *f, u32 stagedsize, const intptr_t *list, size_t count,
        const char *what)
{
    int ok;

    s_Log[0] = '\0';
    ok = convertStaged(f, stagedsize);
    return expect(ok == 0 && sameBytes(f->table, f->staged, IMAGE_SIZE)
            && sameBytes(f->list, list, count * sizeof(*list))
            && startsWith(s_Log, "ge: lp64: cannot convert animation tables \"animation_data\": "),
            what);
}

static int expectRefused(AnimFixture *f, const intptr_t *list, size_t count, const char *what)
{
    return expectRefusedStaged(f, IMAGE_SIZE, list, count, what);
}

static int testRefusesAHeaderOutsideTheStagedImage(void)
{
    // Arrange: the table holds 0x20 at 0xA4, the staged image ends at 0xB0,
    // and the header's own references lie inside it.
    static AnimFixture f;
    arrange(&f, kStockList, 6);
    replaceHeader(&f, 0xA4, 0x40, 0x40);

    // Act, Assert
    return expectRefusedStaged(&f, 0xB0, kStockList, 6,
            "a file header past the staged image's end is refused, nothing written");
}

static int testRefusesDescriptorsOutsideTheStagedImage(void)
{
    // Arrange
    static AnimFixture f;
    arrange(&f, kStockList, 6);
    replaceHeader(&f, 0x60, IMAGE_SIZE, 0x90);

    // Act, Assert
    return expectRefused(&f, kStockList, 6,
            "a descriptor offset at the staged image's end is refused, nothing written");
}

static int testRefusesAStreamOutsideTheStagedImage(void)
{
    // Arrange
    static AnimFixture f;
    arrange(&f, kStockList, 6);
    replaceHeader(&f, 0xA4, 0xB8, 0x1F0);

    // Act, Assert
    return expectRefused(&f, kStockList, 6, "a stream offset past the staged image is refused, nothing written");
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
    failures += testWritesEveryHeader();
    failures += testConvertsAcrossTwoTables();
    failures += testReplacesTheEntries();
    failures += testWritesNothingPastTheHeader();
    failures += testLeavesTheStagingImage();
    failures += testRefusesAHeaderPastTheEnd();
    failures += testRefusesOverlappingHeaders();
    failures += testRefusesAHeaderOutsideTheStagedImage();
    failures += testRefusesDescriptorsOutsideTheStagedImage();
    failures += testRefusesAStreamOutsideTheStagedImage();
    return failures == 0 ? 0 : 1;
}
