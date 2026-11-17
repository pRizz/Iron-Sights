/* The background converter's input checks (LP64-004 Task 12):
 * portBgTablesSize and portBgTablesConvert refuse by name, with nothing
 * written, a front matter whose tables do not end where they must, an offset
 * that is not a segment 0x0F word, points past the file and a short or
 * misaligned room; and portBgRoomListExpand places a room's list at the end of
 * its block, expanded, or refuses. Built and run at each pointer width by
 * test_romconv.sh. Every input is written here and in
 * test_romconv_bg_fixture.c. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "bg.h"
#include "port_romconv.h"
#include "test_romconv_bg_fixture.h"

/* ==== the tables ======================================================== */

static int testRefusesARoomTableWithoutItsZeroRecord(void)
{
    // Arrange: the closing record gets a primary offset, so the table runs
    // into the visibility commands with no record whose primary offset is 0.
    buildFrontMatter();
    put32(FM_ROOMS + 4 * 24 + 4, 0x0F000A00u);

    // Act, Assert
    return expectRefused(FM_SIZE, TABLES,
            "the room table at 0x14 reaches 0x8C with no record whose primary offset is 0",
            "a room table that runs into the visibility commands is refused, nothing written");
}

static int testAcceptsAClosingRecordWithItsOtherWordsSet(void)
{
    // Arrange: as in the game, only the closing record's primary offset ends
    // the table; its point and secondary words are not read.
    u32 size;
    int result;

    buildFrontMatter();
    put32(FM_ROOMS + 4 * 24 + 0, 0x0F000A00u);
    put32(FM_ROOMS + 4 * 24 + 8, 0x0F000B00u);
    freshOut();

    // Act
    size = portBgTablesSize(s_Fm, FM_SIZE, "test");
    result = portBgTablesConvert(s_Fm, FM_SIZE, s_Out.bytes, TABLES, &s_Rooms, &s_Portals);

    // Assert
    return expect(size == TABLES && result == 1 && (u8 *) s_Portals - (u8 *) s_Rooms == 5 * sizeof(bg_room_data)
            && s_Rooms[4].pPriMappingBin == NULL && (uintptr_t) s_Rooms[4].pPointTableBin == 0x0F000A00u
            && (uintptr_t) s_Rooms[4].pSecMappingBin == 0x0F000B00u,
            "a closing record with only its primary offset 0 ends the table and is copied");
}

static int testRefusesARoomEightBytesShort(void)
{
    // Arrange
    int result;

    buildFrontMatter();
    freshOut();
    s_Log[0] = '\0';

    // Act
    result = portBgTablesConvert(s_Fm, FM_SIZE, s_Out.bytes, TABLES - 8, &s_Rooms, &s_Portals);

    // Assert
    return expect(result == 0 && outUntouched(0) && s_Rooms == NULL && s_Portals == NULL
            && startsWith(s_Log, CONVERT_REFUSAL "the tables need "),
            "a room 8 bytes short is refused, nothing written");
}

static int testRefusesAMisalignedRoom(void)
{
    // Arrange
    int result;

    buildFrontMatter();
    freshOut();
    s_Log[0] = '\0';

    // Act
    result = portBgTablesConvert(s_Fm, FM_SIZE, s_Out.bytes + 4, TABLES, &s_Rooms, &s_Portals);

    // Assert
    return expect(result == 0 && outUntouched(0) && s_Rooms == NULL
            && startsWith(s_Log, CONVERT_REFUSAL "the tables' room is not 8-byte aligned"),
            "a room at 4 mod 8 is refused, nothing written");
}

static int testRefusesAFileShorterThanItsHeader(void)
{
    buildFrontMatter();
    return expectRefused(0x13, TABLES, "the 19-byte file is shorter than its 20-byte header",
            "a file shorter than the header is refused, nothing written");
}

static int testRefusesAHeaderRoomOffsetOutsideTheFile(void)
{
    // Arrange
    buildFrontMatter();
    put32(0x04, 0x0F000110u);

    // Act, Assert
    return expectRefused(FM_SIZE, TABLES, "the header's room offset 0xF000110 is outside the 272-byte file",
            "a room table at the file's end is refused, nothing written");
}

static int testRefusesAHeaderPortalOffsetOfZero(void)
{
    // Arrange
    buildFrontMatter();
    put32(0x08, 0);

    // Act, Assert
    return expectRefused(FM_SIZE, TABLES, "the header's portal offset 0x0 is outside the 272-byte file",
            "a file with no portal table is refused, nothing written");
}

static int testRefusesARoomOffsetThatIsNotASegmentWord(void)
{
    // Arrange: an address-like word BG_SEG_TO_PTR would not take as an offset.
    buildFrontMatter();
    put32(FM_ROOMS + 2 * 24 + 4, 0x80000500u);

    // Act, Assert
    return expectRefused(FM_SIZE, TABLES, "room 2's offset 0x80000500 is not a segment 0x0F word",
            "a room offset outside segment 0x0F is refused, nothing written");
}

static int testRefusesAnEndMarkerWithAZeroOffset(void)
{
    // Arrange
    buildFrontMatter();
    put32(FM_ROOMS + 3 * 24 + 8, 0);

    // Act, Assert
    return expectRefused(FM_SIZE, TABLES, "the end marker, room 3, has an offset of 0",
            "an end marker the room walks would pass is refused, nothing written");
}

static int testRefusesAPortalTableWithoutItsZeroEntry(void)
{
    // Arrange: the zero entry and every byte after it get offsets.
    u32 at;

    buildFrontMatter();
    for (at = FM_PORTALS + 2 * 8; at + 8 <= FM_SIZE; at += 8)
    {
        put32(at, 0x0F000000u | FM_POINTS1);
    }

    // Act, Assert
    return expectRefused(FM_SIZE, TABLES,
            "the portal table at 0x9C reaches the file's end with no entry whose offset is 0",
            "a portal table that runs to the file's end is refused, nothing written");
}

static int testRefusesAPortalOffsetThatIsNotASegmentWord(void)
{
    // Arrange
    buildFrontMatter();
    put32(FM_PORTALS + 8, 0x0E0000E8u);

    // Act, Assert
    return expectRefused(FM_SIZE, TABLES, "portal 1's offset 0xE0000E8 is not a segment 0x0F word",
            "a portal offset outside segment 0x0F is refused, nothing written");
}

static int testRefusesPointsPastTheFile(void)
{
    // Arrange: portal 1 claims four points; the file holds three.
    buildFrontMatter();
    s_Fm[FM_POINTS1] = 4;

    // Act, Assert
    return expectRefused(FM_SIZE, TABLES, "portal 1's 4 points at 0xE8 run past the 272-byte file",
            "points past the file's end are refused, nothing written");
}

static int testRefusesAPortalOffsetAtTheFilesEnd(void)
{
    // Arrange
    buildFrontMatter();
    put32(FM_PORTALS + 8, 0x0F000110u);

    // Act, Assert
    return expectRefused(FM_SIZE, TABLES, "portal 1's points at 0x110 run past the 272-byte file",
            "a portal whose count byte is past the file is refused, nothing written");
}

/* ==== the room lists ==================================================== */

#define LIST_COMMANDS 4u
#define LIST_BYTES (LIST_COMMANDS * 8u)
#define HOST_BYTES (LIST_COMMANDS * (u32) sizeof(Gfx))
#define BLOCK_SPARE 16u /* FILL after the block, to catch a write past it */

static union
{
    u64 align;
    u8 bytes[2 * LIST_COMMANDS * 16 + 64 + BLOCK_SPARE];
} s_Block;

/* The block of ALLOCSIZE bytes with FILL everywhere and, at its start, four
 * big-endian commands: command i is {0xB1000000 | i, 0x0E000010 * (i + 1)}. */
static void buildBlock(void)
{
    u32 i;

    memset(s_Block.bytes, FILL, sizeof(s_Block.bytes));
    for (i = 0; i < LIST_COMMANDS; i++)
    {
        u32 w0 = 0xB1000000u | i, w1 = 0x0E000010u * (i + 1);
        u8 *p = s_Block.bytes + i * 8;

        p[0] = (u8) (w0 >> 24), p[1] = (u8) (w0 >> 16), p[2] = (u8) (w0 >> 8), p[3] = (u8) w0;
        p[4] = (u8) (w1 >> 24), p[5] = (u8) (w1 >> 16), p[6] = (u8) (w1 >> 8), p[7] = (u8) w1;
    }
}

static int blockUntouchedFrom(u32 from)
{
    u32 i;

    for (i = from; i < sizeof(s_Block.bytes); i++)
    {
        if (s_Block.bytes[i] != FILL)
        {
            return 0;
        }
    }
    return 1;
}

static int testReturnsTheHostBytes(void)
{
    // Arrange
    Gfx *list = NULL;

    buildBlock();

    // Act
    s32 bytes = portBgRoomListExpand(s_Block.bytes, (s32) (HOST_BYTES + 32), LIST_BYTES, "test", 7, &list);

    // Assert
    return expect(bytes == (s32) HOST_BYTES, "four commands are four host Gfx");
}

static int testPlacesTheListAtTheBlocksEnd(void)
{
    // Arrange
    Gfx *list = NULL;

    buildBlock();

    // Act
    portBgRoomListExpand(s_Block.bytes, (s32) (HOST_BYTES + 32), LIST_BYTES, "test", 7, &list);

    // Assert
    return expect((u8 *) list == s_Block.bytes + 32 && blockUntouchedFrom(HOST_BYTES + 32),
            "the list ends at the block's end, nothing written after it");
}

static int testExpandsEachCommand(void)
{
    // Arrange
    Gfx *list = NULL;
    u32 i, wrong = 0;

    buildBlock();

    // Act
    portBgRoomListExpand(s_Block.bytes, (s32) (HOST_BYTES + 32), LIST_BYTES, "test", 7, &list);

    // Assert
    for (i = 0; i < LIST_COMMANDS; i++)
    {
        wrong += list[i].words.w0 != (uintptr_t) (0xB1000000u | i);
        wrong += list[i].words.w1 != (uintptr_t) (0x0E000010u * (i + 1));
    }
    return expect(wrong == 0, "each command's two words, zero-extended, segment words kept");
}

static int testExpandsIntoABlockOfExactlyTheListsSize(void)
{
    // Arrange: at 32 bits the list stays where it is; at 64 it fills the
    // block, destination and source sharing the start.
    Gfx *list = NULL;
    s32 bytes;

    buildBlock();

    // Act
    bytes = portBgRoomListExpand(s_Block.bytes, (s32) HOST_BYTES, LIST_BYTES, "test", 7, &list);

    // Assert
    return expect(bytes == (s32) HOST_BYTES && (u8 *) list == s_Block.bytes
            && list[LIST_COMMANDS - 1].words.w1 == (uintptr_t) (0x0E000010u * LIST_COMMANDS)
            && list[0].words.w0 == (uintptr_t) 0xB1000000u && blockUntouchedFrom(HOST_BYTES),
            "a block of exactly the expanded size holds the list");
}

static int testRefusesABlockEightBytesShort(void)
{
    // Arrange
    Gfx *list = NULL;
    u8 before[sizeof(s_Block.bytes)];
    s32 bytes;

    buildBlock();
    memcpy(before, s_Block.bytes, sizeof(before));

    // Act
    bytes = portBgRoomListExpand(s_Block.bytes, (s32) HOST_BYTES - 8, LIST_BYTES, "test", 7, &list);

    // Assert
    return expect(bytes == -1 && list == NULL && sameBytes(before, s_Block.bytes, sizeof(before)),
            "a block 8 bytes short of the expanded list is refused, nothing written");
}

/* LISTSIZE must be refused by name, nothing written. */
static int expectListRefused(s32 listsize, const char *why, const char *what)
{
    Gfx *list = NULL;
    u8 before[sizeof(s_Block.bytes)];
    s32 bytes;

    buildBlock();
    memcpy(before, s_Block.bytes, sizeof(before));
    s_Log[0] = '\0';
    bytes = portBgRoomListExpand(s_Block.bytes, (s32) (HOST_BYTES + 32), listsize, "test", 7, &list);
    return expect(bytes == -1 && list == NULL && sameBytes(before, s_Block.bytes, sizeof(before))
            && startsWith(s_Log, REFUSAL) && startsWith(s_Log + sizeof(REFUSAL) - 1, why), what);
}

static int testAcceptsAnEmptyList(void)
{
    // Arrange: a 0-byte list loads at 32 bits (texCopyGdls copies nothing).
    Gfx *list = NULL;
    u8 before[sizeof(s_Block.bytes)];
    s32 bytes;

    buildBlock();
    memcpy(before, s_Block.bytes, sizeof(before));
    s_Log[0] = '\0';

    // Act
    bytes = portBgRoomListExpand(s_Block.bytes, (s32) (HOST_BYTES + 32), 0, "test", 7, &list);

    // Assert
    return expect(bytes == 0 && (u8 *) list == s_Block.bytes + HOST_BYTES + 32
            && sameBytes(before, s_Block.bytes, sizeof(before)) && s_Log[0] == '\0',
            "a list of 0 bytes is empty at the block's end, nothing written or refused");
}

static int testRefusesANegativeList(void)
{
    return expectListRefused(-8, "room 7's list is -8 bytes", "a negative list size is refused, nothing written");
}

static int testRefusesAListOfPartCommands(void)
{
    return expectListRefused(LIST_BYTES - 4, "room 7's list is 28 bytes",
            "a list size that is not a multiple of 8 is refused, nothing written");
}

int main(void)
{
    int failures = 0;

    failures += testRefusesARoomTableWithoutItsZeroRecord();
    failures += testAcceptsAClosingRecordWithItsOtherWordsSet();
    failures += testRefusesARoomEightBytesShort();
    failures += testRefusesAMisalignedRoom();
    failures += testRefusesAFileShorterThanItsHeader();
    failures += testRefusesAHeaderRoomOffsetOutsideTheFile();
    failures += testRefusesAHeaderPortalOffsetOfZero();
    failures += testRefusesARoomOffsetThatIsNotASegmentWord();
    failures += testRefusesAnEndMarkerWithAZeroOffset();
    failures += testRefusesAPortalTableWithoutItsZeroEntry();
    failures += testRefusesAPortalOffsetThatIsNotASegmentWord();
    failures += testRefusesPointsPastTheFile();
    failures += testRefusesAPortalOffsetAtTheFilesEnd();
    failures += testReturnsTheHostBytes();
    failures += testPlacesTheListAtTheBlocksEnd();
    failures += testExpandsEachCommand();
    failures += testExpandsIntoABlockOfExactlyTheListsSize();
    failures += testRefusesABlockEightBytesShort();
    failures += testAcceptsAnEmptyList();
    failures += testRefusesANegativeList();
    failures += testRefusesAListOfPartCommands();
    if (failures != 0)
    {
        printf("test_romconv_bg_checks: %d failed\n", failures);
        return 1;
    }
    printf("test_romconv_bg_checks: PASS (%u-bit)\n", (unsigned) (sizeof(void *) * 8));
    return 0;
}
