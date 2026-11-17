/* The background converter (LP64-004 Task 12): portBgTablesSize measures the
 * room and portal tables of a bg file's front matter (native byte order, as
 * romfileBgConvert leaves it), and portBgTablesConvert writes them at the
 * run-time layout, each offset kept as its segment 0x0F word; both refuse by
 * name, with nothing written, what they cannot convert
 * (test_romconv_bg_checks.c). Built and run at each pointer width by
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

/* ==== measuring and writing ============================================= */

static int testMeasuresBothTables(void)
{
    // Arrange
    buildFrontMatter();

    // Act
    u32 n = portBgTablesSize(s_Fm, FM_SIZE, "test");

    // Assert
    return expect(n == TABLES, "five room records and three portal entries");
}

static int testConverts(void)
{
    return expect(convertStock() == 1, "the front matter converts");
}

static int testWritesExactlyTheMeasuredBytes(void)
{
    // Arrange
    u32 i, filled = 0;

    // Act
    convertStock();

    // Assert
    for (i = 0; i < TABLES; i++)
    {
        filled += s_Out.bytes[i] == FILL;
    }
    return expect(filled == 0 && outUntouched(TABLES), "every measured byte written, none after them");
}

static int testPlacesTheRoomsFirst(void)
{
    convertStock();
    return expect((u8 *) s_Rooms == s_Out.bytes, "the room table starts at OUT");
}

static int testPlacesThePortalsAfterTheRooms(void)
{
    convertStock();
    return expect((u8 *) s_Portals == s_Out.bytes + ROOMS * sizeof(bg_room_data),
            "the portal table follows the five room records");
}

static int testKeepsTheRoomsPointOffset(void)
{
    convertStock();
    return expect((uintptr_t) s_Rooms[1].pPointTableBin == 0x0F000200u, "room 1's point table word");
}

static int testKeepsTheRoomsPrimaryOffset(void)
{
    convertStock();
    return expect((uintptr_t) s_Rooms[1].pPriMappingBin == 0x0F000280u, "room 1's primary list word");
}

static int testKeepsTheRoomsSecondaryOffset(void)
{
    convertStock();
    return expect((uintptr_t) s_Rooms[1].pSecMappingBin == 0x0F000300u, "room 1's secondary list word");
}

static int testKeepsAZeroOffsetZero(void)
{
    convertStock();
    return expect(s_Rooms[2].pSecMappingBin == NULL, "room 2 has no secondary list");
}

static int testCopiesTheRoomsX(void)
{
    convertStock();
    return expect(s_Rooms[1].pos.x == 1.5f, "room 1's x");
}

static int testCopiesTheRoomsY(void)
{
    convertStock();
    return expect(s_Rooms[1].pos.y == 12.5f, "room 1's y");
}

static int testCopiesTheRoomsZ(void)
{
    convertStock();
    return expect(s_Rooms[1].pos.z == -3.25f, "room 1's z");
}

static int testWritesTheEndMarker(void)
{
    convertStock();
    return expect((uintptr_t) s_Rooms[3].pPointTableBin == 0x0F000900u
            && (uintptr_t) s_Rooms[3].pSecMappingBin == 0x0F000990u, "the end marker's words");
}

static int testWritesTheZeroRecords(void)
{
    // Arrange
    static const u8 zero[sizeof(bg_room_data)];

    // Act
    convertStock();

    // Assert
    return expect(sameBytes(&s_Rooms[0], zero, sizeof(zero)) && sameBytes(&s_Rooms[4], zero, sizeof(zero)),
            "the first and the closing room records are zeros, padding included");
}

static int testKeepsThePortalsOffset(void)
{
    convertStock();
    return expect((uintptr_t) s_Portals[0].offset_portal == 0x0F0000B4u, "portal 0's points word");
}

static int testLeavesThePortalOffsetForThePromotion(void)
{
    convertStock();
    return expect(BG_SEG_IS_OFFSET((uintptr_t) s_Portals[0].offset_portal), "BG_SEG_TO_PTR takes it as an offset");
}

static int testCopiesTheConnectedRooms(void)
{
    convertStock();
    return expect(s_Portals[0].connectedRoom1 == 3 && s_Portals[0].connectedRoom2 == 4, "portal 0 joins 3 and 4");
}

static int testCopiesTheControlBytes(void)
{
    convertStock();
    return expect(s_Portals[1].controlbytes1 == 0x80 && s_Portals[1].controlbytes2 == 0x40,
            "portal 1's control bytes in the file's order");
}

static int testWritesTheZeroPortal(void)
{
    // Arrange
    static const u8 zero[sizeof(bg_portal_data_entry)];

    // Act
    convertStock();

    // Assert
    return expect(sameBytes(&s_Portals[2], zero, sizeof(zero)), "the closing portal entry is zeros");
}

static int testLeavesTheFileAlone(void)
{
    // Arrange
    u8 before[FM_SIZE];

    buildFrontMatter();
    memcpy(before, s_Fm, sizeof(before));
    freshOut();

    // Act
    portBgTablesConvert(s_Fm, FM_SIZE, s_Out.bytes, TABLES, &s_Rooms, &s_Portals);

    // Assert
    return expect(sameBytes(before, s_Fm, sizeof(before)), "the file is not written");
}

int main(void)
{
    int failures = 0;

    failures += testMeasuresBothTables();
    failures += testConverts();
    failures += testWritesExactlyTheMeasuredBytes();
    failures += testPlacesTheRoomsFirst();
    failures += testPlacesThePortalsAfterTheRooms();
    failures += testKeepsTheRoomsPointOffset();
    failures += testKeepsTheRoomsPrimaryOffset();
    failures += testKeepsTheRoomsSecondaryOffset();
    failures += testKeepsAZeroOffsetZero();
    failures += testCopiesTheRoomsX();
    failures += testCopiesTheRoomsY();
    failures += testCopiesTheRoomsZ();
    failures += testWritesTheEndMarker();
    failures += testWritesTheZeroRecords();
    failures += testKeepsThePortalsOffset();
    failures += testLeavesThePortalOffsetForThePromotion();
    failures += testCopiesTheConnectedRooms();
    failures += testCopiesTheControlBytes();
    failures += testWritesTheZeroPortal();
    failures += testLeavesTheFileAlone();
    if (failures != 0)
    {
        printf("test_romconv_bg: %d failed\n", failures);
        return 1;
    }
    printf("test_romconv_bg: PASS (%u-bit)\n", (unsigned) (sizeof(void *) * 8));
    return 0;
}
