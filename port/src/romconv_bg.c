/**
 * The background converter (port_romconv.h; LP64-004 Task 12).
 *
 * A bg file's room table (24-byte records of three offsets and a position)
 * and portal table (8-byte entries of an offset and four bytes) are read at
 * 32 bits in place, inside the ROM image, as bg_room_data and
 * bg_portal_data_entry. At 64 bits those records hold 8-byte pointer slots
 * and are 40 and 16 bytes, so load_bg_file builds both tables here, in the
 * stage pool, at each load (decision 6 of the plan: fresh, as on the N64).
 * Every offset is kept as the file's segment 0x0F word, zero-extended, so
 * BG_SEG_TO_PTR, the room loaders' file positions and the portal promotion
 * run unchanged; the visibility commands and the portal points stay in the
 * ROM image, where the promoted portal offsets lead.
 *
 * Also the 64-bit room loaders' display-list placement: bgDecompress leaves a
 * room's 8-byte commands at its block's start, and texCopyGdls's 32-bit
 * placement, the same number of bytes before the block's end, has no room
 * for 16-byte commands.
 *
 * Compiled only at 64 bits and in the 32-bit oracle build, where the tables
 * must equal the in-place ones byte for byte.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "bg.h"
#include "port_romconv_stage.h"
#include "port_romlayout.h"

#define BG_CONVERT_FILE "bg file"
#define BG_SEGMENT 0x0Fu

/* The u32 at P, in native byte order (romfileBgConvert has swapped it);
 * P may have any alignment. */
static u32 bgWord(const u8 *p)
{
    u32 value;

    memcpy(&value, p, sizeof(value));
    return value;
}

/* 0, or a segment 0x0F word: the forms the file's offsets take. */
static int bgIsOffsetWord(u32 word)
{
    return word == 0 || (word >> 24) == BG_SEGMENT;
}

/* What a valid front matter measures to: the record counts, each including
 * its closing record (a room record whose primary offset is 0, a portal entry
 * whose offset is 0), and where the two tables start in the file. */
struct BgShape
{
    u32 roomsAt;
    u32 rooms;
    u32 portalsAt;
    u32 portals;
};

/* Reads the header's offset at OFFSET into *AT, a file offset inside the
 * file and past the header; refuses otherwise. */
static int bgHeaderTable(const u8 *file, u32 size, u32 offset, const char *what, const char *name, u32 *at)
{
    u32 word = bgWord(file + offset);
    u32 off = word & 0x00FFFFFFu;

    if (!bgIsOffsetWord(word))
    {
        portConvRefuse(PORT_LP64_BACKGROUNDS, name, "the header's %s offset 0x%X is not a segment 0x0F word", what,
                word);
        return 0;
    }
    if (word == 0 || off < sizeof(RomBgHeader) || off >= size)
    {
        portConvRefuse(PORT_LP64_BACKGROUNDS, name, "the header's %s offset 0x%X is outside the %u-byte file", what,
                word, size);
        return 0;
    }
    *at = off;
    return 1;
}

/* Where the room table must have ended: the first of the visibility commands
 * and the portal table the header places after it, or the file's end. */
static u32 bgRoomLimit(const u8 *file, u32 size, u32 roomsAt)
{
    u32 limit = size;
    u32 i;

    for (i = 0; i < 2; i++)
    {
        u32 off = bgWord(file + (i == 0 ? offsetof(RomBgHeader, vis) : offsetof(RomBgHeader, portals))) & 0x00FFFFFFu;

        if (off > roomsAt && off < limit)
        {
            limit = off;
        }
    }
    return limit;
}

/* Counts the room records up to and including the first, after record 0,
 * whose primary offset is 0: the game's own end test (load_bg_file's room
 * count reads pPriMappingBin and nothing else of that closing record).
 * Checks every offset word up to it and the end marker before it; refuses
 * otherwise. */
static int bgMeasureRooms(const u8 *file, u32 size, const char *name, struct BgShape *shape)
{
    u32 limit = bgRoomLimit(file, size, shape->roomsAt);
    u64 at = shape->roomsAt;
    u32 room, k;

    for (room = 0;; room++, at += sizeof(RomBgRoom))
    {
        const u8 *rec = file + at;

        if (at + sizeof(RomBgRoom) > limit)
        {
            portConvRefuse(PORT_LP64_BACKGROUNDS, name,
                    "the room table at 0x%X reaches 0x%X with no record whose primary offset is 0", shape->roomsAt,
                    limit);
            return 0;
        }
        for (k = 0; k < 3; k++)
        {
            u32 word = bgWord(rec + k * 4);

            if (!bgIsOffsetWord(word))
            {
                portConvRefuse(PORT_LP64_BACKGROUNDS, name, "room %u's offset 0x%X is not a segment 0x0F word", room,
                        word);
                return 0;
            }
        }
        if (room > 0 && bgWord(rec + offsetof(RomBgRoom, primary)) == 0)
        {
            break;
        }
    }
    /* RECORD ROOM closes the table; ROOM - 1, when it is a room, is the end
     * marker, which stops getPointTableBinCount and its twins. */
    if (room >= 2)
    {
        const u8 *end = file + at - sizeof(RomBgRoom);

        for (k = 0; k < 3; k++)
        {
            if (bgWord(end + k * 4) == 0)
            {
                portConvRefuse(PORT_LP64_BACKGROUNDS, name, "the end marker, room %u, has an offset of 0", room - 1);
                return 0;
            }
        }
    }
    shape->rooms = room + 1;
    return 1;
}

/* Counts the portal entries up to and including the first whose offset is 0,
 * then checks each portal's offset word and that its points lie in the file;
 * refuses otherwise. */
static int bgMeasurePortals(const u8 *file, u32 size, const char *name, struct BgShape *shape)
{
    u64 at;
    u32 portal;

    for (portal = 0, at = shape->portalsAt;; portal++, at += sizeof(RomBgPortal))
    {
        if (at + sizeof(RomBgPortal) > size)
        {
            portConvRefuse(PORT_LP64_BACKGROUNDS, name,
                    "the portal table at 0x%X reaches the file's end with no entry whose offset is 0",
                    shape->portalsAt);
            return 0;
        }
        if (bgWord(file + at + offsetof(RomBgPortal, offset)) == 0)
        {
            break;
        }
    }
    shape->portals = portal + 1;
    for (portal = 0, at = shape->portalsAt; portal + 1 < shape->portals; portal++, at += sizeof(RomBgPortal))
    {
        u32 word = bgWord(file + at + offsetof(RomBgPortal, offset));
        u64 points = word & 0x00FFFFFFu;

        if (!bgIsOffsetWord(word))
        {
            portConvRefuse(PORT_LP64_BACKGROUNDS, name, "portal %u's offset 0x%X is not a segment 0x0F word", portal,
                    word);
            return 0;
        }
        if (points + 4 > size)
        {
            portConvRefuse(PORT_LP64_BACKGROUNDS, name, "portal %u's points at 0x%X run past the %u-byte file", portal,
                    (u32) points, size);
            return 0;
        }
        if (points + 4 + (u64) file[points] * sizeof(coord3d) > size)
        {
            portConvRefuse(PORT_LP64_BACKGROUNDS, name, "portal %u's %u points at 0x%X run past the %u-byte file",
                    portal, file[points], (u32) points, size);
            return 0;
        }
    }
    return 1;
}

/* The tables' bytes for SHAPE, a multiple of 8. */
static u64 bgTablesBytes(const struct BgShape *shape)
{
    u64 bytes = (u64) shape->rooms * sizeof(bg_room_data) + (u64) shape->portals * sizeof(bg_portal_data_entry);

    return (bytes + 7) & ~(u64) 7;
}

/* Measures FILE into *SHAPE; returns the tables' bytes, or 0 after a refusal
 * naming NAME. Reads only FILE. */
static u32 bgMeasure(const u8 *file, u32 size, const char *name, struct BgShape *shape)
{
    u64 bytes;

    if (size < sizeof(RomBgHeader))
    {
        portConvRefuse(PORT_LP64_BACKGROUNDS, name, "the %u-byte file is shorter than its %u-byte header", size,
                (u32) sizeof(RomBgHeader));
        return 0;
    }
    if (!bgHeaderTable(file, size, offsetof(RomBgHeader, rooms), "room", name, &shape->roomsAt)
            || !bgHeaderTable(file, size, offsetof(RomBgHeader, portals), "portal", name, &shape->portalsAt)
            || !bgMeasureRooms(file, size, name, shape) || !bgMeasurePortals(file, size, name, shape))
    {
        return 0;
    }
    /* At most size / 24 + size / 8 records of at most 40 bytes: no wrap. */
    bytes = bgTablesBytes(shape);
    if (bytes > UINT32_MAX)
    {
        portConvRefuse(PORT_LP64_BACKGROUNDS, name, "the tables would take %llu bytes", (unsigned long long) bytes);
        return 0;
    }
    return (u32) bytes;
}

u32 portBgTablesSize(const u8 *file, u32 size, const char *name)
{
    struct BgShape shape;

    return bgMeasure(file, size, name, &shape);
}

int portBgTablesConvert(const u8 *file, u32 size, u8 *out, u32 outsize, struct bg_room_data **rooms,
        struct bg_portal_data_entry **portals)
{
    struct BgShape shape;
    bg_room_data *room;
    bg_portal_data_entry *portal;
    u32 need = bgMeasure(file, size, BG_CONVERT_FILE, &shape);
    u32 i;

    if (need == 0)
    {
        return 0;
    }
    if (outsize < need)
    {
        portConvRefuse(PORT_LP64_BACKGROUNDS, BG_CONVERT_FILE, "the tables need %u bytes, the room is %u", need,
                outsize);
        return 0;
    }
    if (((uintptr_t) out & 7) != 0)
    {
        portConvRefuse(PORT_LP64_BACKGROUNDS, BG_CONVERT_FILE, "the tables' room is not 8-byte aligned");
        return 0;
    }
    memset(out, 0, need);
    room = (bg_room_data *) out;
    portal = (bg_portal_data_entry *) (out + shape.rooms * sizeof(bg_room_data));
    for (i = 0; i < shape.rooms; i++)
    {
        const u8 *rec = file + shape.roomsAt + i * sizeof(RomBgRoom);
        u32 k;

        room[i].pPointTableBin = (void *) (uintptr_t) bgWord(rec + offsetof(RomBgRoom, point));
        room[i].pPriMappingBin = (void *) (uintptr_t) bgWord(rec + offsetof(RomBgRoom, primary));
        room[i].pSecMappingBin = (void *) (uintptr_t) bgWord(rec + offsetof(RomBgRoom, secondary));
        for (k = 0; k < 3; k++)
        {
            memcpy(&room[i].pos.f[k], rec + offsetof(RomBgRoom, pos) + k * 4, sizeof(f32));
        }
    }
    for (i = 0; i < shape.portals; i++)
    {
        const u8 *rec = file + shape.portalsAt + i * sizeof(RomBgPortal);

        portal[i].offset_portal = (bg_portal_entry *) (uintptr_t) bgWord(rec + offsetof(RomBgPortal, offset));
        portal[i].connectedRoom1 = rec[offsetof(RomBgPortal, room1)];
        portal[i].connectedRoom2 = rec[offsetof(RomBgPortal, room2)];
        portal[i].controlbytes1 = rec[offsetof(RomBgPortal, cb1)];
        portal[i].controlbytes2 = rec[offsetof(RomBgPortal, cb2)];
    }
    *rooms = room;
    *portals = portal;
    return 1;
}

s32 portBgRoomListExpand(u8 *block, s32 allocsize, s32 listsize, const char *name, s32 roomnum, Gfx **list)
{
    u32 commands;
    s64 bytes;

    if (listsize < 0 || listsize % 8 != 0)
    {
        portConvRefuse(PORT_LP64_BACKGROUNDS, name, "room %d's list is %d bytes, not a multiple of 8 of 0 or more",
                roomnum, listsize);
        return -1;
    }
    if (listsize == 0)
    {
        /* As at 32 bits, where texCopyGdls copies nothing to the block's
         * end and the loaders treat a 0-byte list as loaded. */
        *list = (Gfx *) (block + allocsize);
        return 0;
    }
    commands = (u32) listsize / 8u;
    bytes = (s64) commands * (s64) sizeof(Gfx);
    if (bytes > allocsize)
    {
        return -1;
    }
    /* The source is [BLOCK, BLOCK + LISTSIZE); the destination ends at the
     * block's end and, with ALLOCSIZE >= BYTES, starts at or after BLOCK,
     * which portGfxFromRomBackward allows (port_romconv.h). */
    *list = (Gfx *) (block + (allocsize - bytes));
    portGfxFromRomBackward(*list, block, commands);
    return (s32) bytes;
}
