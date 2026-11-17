/* The collision tile converter (LP64-004 Task 13): portStanConvert turns a
 * stan file's tiles in place and at 32 bits writes the StanPrefixRecord over
 * the header, equal to romfile_stan.c's in-place pass; at 64 bits the header
 * stays and portStanRecordFill gives the record. Bad headers, room offsets
 * and tile walks are refused by name, writing nothing. Built and run at each
 * pointer width by test_romconv.sh; every input is written here. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "port_romconv_stage.h"

/* The test file: the header, three room slots and the zero slot, two tiles
 * of three points (0x20 bytes each), the zero word. */
#define STAN_FIRST 0x18u
#define STAN_TILE0 0x18u
#define STAN_TILE1 0x38u
#define STAN_END 0x58u
#define STAN_SIZE 0x5Cu
#define REFUSAL "ge: lp64: cannot convert collision tiles \"test\": "

#define STAN_ROOM 0x100u /* the buffer the files are built in */

static char s_Log[512];
static _Alignas(struct StanPrefixRecord) u8 s_Stan[STAN_ROOM];
static u8 s_Before[STAN_ROOM];

#if !IS_64_BIT
/* romfile_stan.c's in-place pass, linked at 32 bits for the comparison, and
 * what it needs of stan.c. */
extern s32 romfileStanConvert(u8 *data, u32 size, const char *name, const u8 *compiled);
u8 list_of_tilesizes[] = {0x20, 0x20, 0x20, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x00};
struct StanPrefixRecord *portStanPackForProbe(struct StanPrefixRecord *file)
{
    (void) file;
    return NULL;
}
#endif

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

/* The game's <string.h>, first on the include path, declares no memcmp. */
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
        printf("test_romconv_stan: FAIL: %s\n", what);
        return 1;
    }
    return 0;
}

static void putBe16(u32 at, u16 value)
{
    s_Stan[at] = (u8) (value >> 8);
    s_Stan[at + 1] = (u8) value;
}

static void putBe32(u32 at, u32 value)
{
    putBe16(at, (u16) (value >> 16));
    putBe16(at + 2, (u16) value);
}

/* A big-endian tile at AT: id and room, mid {special, r, g, b}, tail
 * {COUNT, c, d, e}, then COUNT points (x, y, z, link). */
static void putTile(u32 at, u32 id, u8 room, u16 mid, u16 tail, const s16 (*points)[4], u32 count)
{
    u32 p, k;

    putBe32(at, (id << 8) | room);
    putBe16(at + 4, mid);
    putBe16(at + 6, tail);
    for (p = 0; p < count; p++)
    {
        for (k = 0; k < 4; k++)
        {
            putBe16(at + 8 + p * 8 + k * 2, (u16) points[p][k]);
        }
    }
}

/* The test file in s_Stan (STAN_SIZE bytes, then bytes of 0xEE), copied to
 * s_Before. Room slots: 0x18, 0x38, 0x38. Tile 0: id 0x010203, room 5, mid
 * 0x1234, tail 0x3012, points (10, -20, 30, 0), (-40, 50, -60, 0x33),
 * (70, 80, 90, 0). Tile 1: id 0x0A0B0C, room 6, mid 0x0000, tail 0x3021,
 * points (1, 2, 3, 0x7), (4, 5, 6, 0), (7, 8, 9, 0). */
static void buildStan(void)
{
    static const s16 kPoints0[3][4] = {{10, -20, 30, 0}, {-40, 50, -60, 0x33}, {70, 80, 90, 0}};
    static const s16 kPoints1[3][4] = {{1, 2, 3, 0x7}, {4, 5, 6, 0}, {7, 8, 9, 0}};

    memset(s_Stan, 0, sizeof(s_Stan));
    memset(s_Stan + STAN_SIZE, 0xEE, sizeof(s_Stan) - STAN_SIZE);
    putBe32(0x04, STAN_FIRST);
    putBe32(0x08, STAN_TILE0);
    putBe32(0x0C, STAN_TILE1);
    putBe32(0x10, STAN_TILE1);
    putTile(STAN_TILE0, 0x010203u, 5, 0x1234, 0x3012, kPoints0, 3);
    putTile(STAN_TILE1, 0x0A0B0Cu, 6, 0x0000, 0x3021, kPoints1, 3);
    memcpy(s_Before, s_Stan, sizeof(s_Stan));
}

static int convert(u32 size)
{
    s_Log[0] = '\0';
    return portStanConvert(s_Stan, size, (uintptr_t) s_Stan, "test");
}

static const StandTile *tileAt(u32 at)
{
    return (const StandTile *) (s_Stan + at);
}

/* The record the game reads once s_Stan (SIZE bytes) is converted: the
 * file's first bytes at 32 bits, portStanRecordFill's at 64. */
static struct StanPrefixRecord recordAfter(u32 size)
{
    struct StanPrefixRecord record = {(struct StanPrefixRecord *) s_Log, NULL};

#if IS_64_BIT
    portStanRecordFill(s_Stan, size, (uintptr_t) s_Stan, &record, "test");
#else
    (void) size;
    memcpy(&record, s_Stan, sizeof(record));
#endif
    return record;
}

/* At 32 bits: converts s_Before's SIZE bytes into s_Stan with
 * portStanConvert and with romfile_stan.c's in-place pass, at the same
 * address, and compares the whole file. True at 64 bits. */
static int sameAsInPlace(u32 size)
{
#if IS_64_BIT
    (void) size;
    return 1;
#else
    static u8 converted[STAN_ROOM];

    memcpy(s_Stan, s_Before, sizeof(s_Stan));
    portStanConvert(s_Stan, size, (uintptr_t) s_Stan, "test");
    memcpy(converted, s_Stan, sizeof(converted));
    memcpy(s_Stan, s_Before, sizeof(s_Stan));
    return romfileStanConvert(s_Stan, size, "test", NULL) == 1 && sameBytes(converted, s_Stan, sizeof(s_Stan));
#endif
}

/* ==== the converted file ================================================= */

static int testStockFile(void)
{
    // Arrange
    struct StanPrefixRecord record;
    int result;

    buildStan();

    // Act
    result = convert(STAN_SIZE);
    record = recordAfter(STAN_SIZE);

    // Assert
    return expect(result == 1, "the test file converts")
        + expect(record.stanfile == NULL, "the record's stanfile is NULL")
        + expect((u8 *) record.ptr_firstroom == s_Stan + STAN_FIRST, "the record's ptr_firstroom is the first tile")
        + expect(tileAt(STAN_FIRST)->room == 5, "tile 0's room")
        + expect(tileAt(STAN_FIRST)->points[1].link == 0x0033, "tile 0's point 1 link")
        + expect(sizeof(void *) == 4 ? s_Log[0] == '\0' : __builtin_strncmp(s_Log, "ge: romdata: stan  test ", 24) == 0,
                "nothing is logged but the census line, at 64 bits (test_romconv_census.c)")
        + expect(sameAsInPlace(STAN_SIZE), "at 32 bits the file equals the in-place pass's");
}

static int testTileHeaders(void)
{
    // Arrange
    buildStan();

    // Act
    convert(STAN_SIZE);

    // Assert
    return expect(tileAt(STAN_TILE0)->id == 0x010203u, "tile 0's id")
        + expect((u16) tileAt(STAN_TILE0)->mid.half == 0x4321, "tile 0's mid has its nibbles reversed")
        + expect((u16) tileAt(STAN_TILE0)->tail.half == 0x2103, "tile 0's tail has its nibbles reversed")
        + expect((tileAt(STAN_TILE0)->tail.half & 0xF) == 3, "tile 0's point count reads as 3")
        + expect(tileAt(STAN_TILE1)->id == 0x0A0B0Cu, "tile 1's id")
        + expect(tileAt(STAN_TILE1)->room == 6, "tile 1's room")
        + expect((u16) tileAt(STAN_TILE1)->tail.half == 0x1203, "tile 1's tail has its nibbles reversed");
}

static int testTilePoints(void)
{
    // Arrange
    const StandTilePoint *p0, *p1;

    buildStan();

    // Act
    convert(STAN_SIZE);
    p0 = tileAt(STAN_TILE0)->points;
    p1 = tileAt(STAN_TILE1)->points;

    // Assert
    return expect(p0[0].x == 10 && p0[0].y == -20 && p0[0].z == 30 && p0[0].link == 0, "tile 0's point 0")
        + expect(p0[1].x == -40 && p0[1].y == 50 && p0[1].z == -60, "tile 0's point 1 position")
        + expect(p0[2].x == 70 && p0[2].y == 80 && p0[2].z == 90 && p0[2].link == 0, "tile 0's point 2")
        + expect(p1[0].x == 1 && p1[0].y == 2 && p1[0].z == 3 && p1[0].link == 0x7, "tile 1's point 0")
        + expect(p1[2].x == 7 && p1[2].y == 8 && p1[2].z == 9, "tile 1's point 2");
}

static int testEndAndPastTheFile(void)
{
    // Arrange
    buildStan();

    // Act
    convert(STAN_SIZE);

    // Assert
    return expect(sameBytes(s_Stan + STAN_END, s_Before + STAN_END, 4), "the zero word that ends the tiles stays")
        + expect(sameBytes(s_Stan + STAN_SIZE, s_Before + STAN_SIZE, 8), "nothing past the file is written");
}

/* At 32 bits each room slot becomes its tile's address; at 64 bits the
 * header words and room slots stay as the file has them. */
static int testRoomSlots(void)
{
    // Arrange
    StandTile *slot[3];

    buildStan();

    // Act
    convert(STAN_SIZE);

    // Assert
    if (sizeof(StandTile *) == 4)
    {
        memcpy(slot, s_Stan + 8, sizeof(slot));
        return expect((u8 *) slot[0] == s_Stan + STAN_TILE0 && (u8 *) slot[1] == s_Stan + STAN_TILE1
                    && (u8 *) slot[2] == s_Stan + STAN_TILE1,
                       "at 32 bits each room slot holds its tile's address")
            + expect(sameBytes(s_Stan + 0x14, s_Before + 0x14, 4), "the zero slot stays");
    }
    return expect(sameBytes(s_Stan, s_Before, STAN_FIRST), "at 64 bits the header and room slots stay the file's");
}

/* portStanRecordFill reads only the header, which the 64-bit conversion
 * leaves alone, so the loader gets the same record from a file converted at
 * an earlier load; at 32 bits the record it gives before the conversion is
 * the one the conversion writes. */
static int testRecordBeforeAndAfterConversion(void)
{
    // Arrange
    struct StanPrefixRecord before = {NULL, NULL}, after;
    int filled;

    buildStan();
    filled = portStanRecordFill(s_Stan, STAN_SIZE, (uintptr_t) s_Stan, &before, "test");

    // Act
    convert(STAN_SIZE);
    after = recordAfter(STAN_SIZE);

    // Assert
    return expect(filled == 1 && before.stanfile == NULL && (u8 *) before.ptr_firstroom == s_Stan + STAN_FIRST,
                   "the record filled before the conversion")
        + expect(sameBytes(&before, &after, sizeof(before)), "the record is the same after the conversion");
}

/* A file of R room slots, its one tile (room 9, three points) at the first
 * tile, 8 + (R + 1) * 4, then the zero word; its size. */
static u32 buildSmallStan(u32 rooms)
{
    static const s16 kPoints[3][4] = {{1, 2, 3, 0}, {4, 5, 6, 0}, {7, 8, 9, 0}};
    u32 first = 8 + (rooms + 1) * 4, i;

    memset(s_Stan, 0, sizeof(s_Stan));
    putBe32(0x04, first);
    for (i = 0; i < rooms; i++)
    {
        putBe32(0x08 + i * 4, first);
    }
    putTile(first, 0x000001u, 9, 0, 0x3000, kPoints, 3);
    memcpy(s_Before, s_Stan, sizeof(s_Stan));
    return first + 0x20 + 4;
}

/* One room slot: the tiles start at 0x10, where a 64-bit record would end. */
static int testOneRoom(void)
{
    // Arrange
    u32 size = buildSmallStan(1);
    struct StanPrefixRecord record;
    int result;

    // Act
    result = convert(size);
    record = recordAfter(size);

    // Assert
    return expect(result == 1 && (u8 *) record.ptr_firstroom == s_Stan + 0x10 && record.stanfile == NULL
                   && tileAt(0x10)->room == 9 && tileAt(0x10)->points[2].z == 9,
                   "a one-room file converts, its record pointing at 0x10")
        + expect(sameAsInPlace(size), "at 32 bits a one-room file equals the in-place pass's");
}

/* No room slot, as in 18 of the 27 stock files: the first tile at 0x0C. */
static int testNoRoomSlot(void)
{
    // Arrange
    u32 size = buildSmallStan(0);
    struct StanPrefixRecord record;
    int result;

    // Act
    result = convert(size);
    record = recordAfter(size);

    // Assert
    return expect(result == 1 && (u8 *) record.ptr_firstroom == s_Stan + 0x0C && record.stanfile == NULL,
                   "a file with no room slot converts, its record pointing at 0x0C")
        + expect(sizeof(StandTile *) == 4 || sameBytes(s_Stan, s_Before, 0x0C),
                   "at 64 bits the 12-byte header stays the file's")
        + expect(tileAt(0x0C)->id == 1 && tileAt(0x0C)->room == 9 && (tileAt(0x0C)->tail.half & 0xF) == 3,
                   "the first tile's first words are the converted tile")
        + expect(sameAsInPlace(size), "at 32 bits a file with no room slot equals the in-place pass's");
}

/* ==== refusals =========================================================== */

static int expectRefused(u32 size, const char *why, const char *what)
{
    char want[256];
    int result = convert(size);

    snprintf(want, sizeof(want), REFUSAL "%s", why);
    return expect(result == 0 && sameBytes(s_Stan, s_Before, sizeof(s_Stan)) && startsWith(s_Log, want), what);
}

static int testRefusesAFirstTileBeforeTheRoomTableEnds(void)
{
    buildStan();
    putBe32(0x04, 0x14);
    memcpy(s_Before, s_Stan, sizeof(s_Stan));
    return expectRefused(STAN_SIZE, "the first tile is at 0x14 but the room table of 3 slots ends at 0x18",
            "first_tile 0x14 with three room slots is refused, nothing written");
}

static int testRefusesAShortFile(void)
{
    buildStan();
    return expectRefused(7, "the 7-byte file is shorter than its 8-byte header",
            "a file shorter than the header is refused, nothing written");
}

static int testRefusesARoomTableWithoutItsZeroSlot(void)
{
    buildStan();
    return expectRefused(0x14, "the room table has no zero slot inside the 20-byte file",
            "a room table cut before its zero slot is refused, nothing written");
}

static int testRefusesARoomOffsetPastTheFile(void)
{
    buildStan();
    putBe32(0x10, STAN_SIZE);
    memcpy(s_Before, s_Stan, sizeof(s_Stan));
    return expectRefused(STAN_SIZE, "room 2's first tile at 0x5C is outside the tiles (0x18 to 0x5C)",
            "a room offset at the file's end is refused, nothing written");
}

static int testRefusesARoomOffsetInTheRoomTable(void)
{
    buildStan();
    putBe32(0x08, 0x14);
    memcpy(s_Before, s_Stan, sizeof(s_Stan));
    return expectRefused(STAN_SIZE, "room 0's first tile at 0x14 is outside the tiles (0x18 to 0x5C)",
            "a room offset before the first tile is refused, nothing written");
}

static int testRefusesAFileWithNoTerminator(void)
{
    buildStan();
    return expectRefused(STAN_END, "the tiles have no zero word inside the 88-byte file (2 tiles to 0x58)",
            "tiles that run to the file's end with no zero word are refused, nothing written");
}

static int testRefusesATruncatedTile(void)
{
    buildStan();
    return expectRefused(STAN_TILE1 + 6, "tile 1 at 0x38 is cut off by the end of the 62-byte file",
            "a tile header cut by the file's end is refused, nothing written");
}

static int testRefusesPointsPastTheFile(void)
{
    buildStan();
    putBe16(STAN_TILE1 + 6, 0xA021);
    memcpy(s_Before, s_Stan, sizeof(s_Stan));
    return expectRefused(STAN_SIZE, "tile 1 at 0x38 has 10 points (88 bytes), past the end of the 92-byte file",
            "a point count that runs past the file is refused, nothing written");
}

/* An 11-point tile that would fit, 0x60 bytes then the zero word, so only
 * the count check refuses it. */
static int testRefusesElevenPoints(void)
{
    // Arrange
    buildSmallStan(0);
    putBe16(0x0C + 6, 0xB012);
    memcpy(s_Before, s_Stan, sizeof(s_Stan));

    // Act, Assert
    return expectRefused(0x0C + 0x60 + 4, "tile 0 at 0xC has 11 points; a tile has at most 10",
            "a point count above 10 is refused, nothing written");
}

static int testRefusesAFirstTileOutsideTheFile(void)
{
    buildSmallStan(0);
    return expectRefused(0x0E, "the first tile at 0xC is outside the 14-byte file",
            "a first tile with no room for a word is refused, nothing written");
}

static int testRecordFillRefusesAndLeavesTheRecord(void)
{
    // Arrange
    struct StanPrefixRecord record = {(struct StanPrefixRecord *) s_Log, (StandTile *) s_Log};
    int result;

    buildStan();
    putBe32(0x04, 0x14);
    s_Log[0] = '\0';

    // Act
    result = portStanRecordFill(s_Stan, STAN_SIZE, (uintptr_t) s_Stan, &record, "test");

    // Assert
    return expect(result == 0 && (void *) record.stanfile == s_Log && (void *) record.ptr_firstroom == s_Log
                   && startsWith(s_Log, REFUSAL "the first tile is at 0x14"),
                   "portStanRecordFill refuses a bad header by name, leaving the record");
}

int main(void)
{
    int failed = testStockFile() + testTileHeaders() + testTilePoints() + testEndAndPastTheFile() + testRoomSlots()
        + testRecordBeforeAndAfterConversion() + testOneRoom() + testNoRoomSlot()
        + testRefusesAFirstTileBeforeTheRoomTableEnds() + testRefusesAShortFile()
        + testRefusesARoomTableWithoutItsZeroSlot() + testRefusesARoomOffsetPastTheFile()
        + testRefusesARoomOffsetInTheRoomTable() + testRefusesAFileWithNoTerminator() + testRefusesATruncatedTile()
        + testRefusesPointsPastTheFile() + testRefusesElevenPoints() + testRefusesAFirstTileOutsideTheFile()
        + testRecordFillRefusesAndLeavesTheRecord();

    if (failed != 0)
    {
        printf("test_romconv_stan: %d failed\n", failed);
        return 1;
    }
    return 0;
}
