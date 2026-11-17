/**
 * The collision tile converter (port_romconv_stage.h; LP64-004 Task 13).
 *
 * A stan file is a two-word header, one 4-byte slot a room and a zero slot,
 * then the tiles back to back to a zero word (port_romlayout_stage.h). The
 * game reads the header as struct StanPrefixRecord, two pointers, and the
 * tiles in place: the tiles hold no address (a point's link is an offset in
 * 8-byte units from standTileStart), so only the header depends on the
 * pointer width. At 32 bits the record is the two header words, written over
 * them, and the room slots are rebased to addresses, as romfileStanConvert
 * has always done. At 64 bits the record is 16 bytes, and 18 of the 27 stock
 * files have no room slot, their first tile at 0x0C: the record cannot be
 * written over the file, so the header stays as the file has it and
 * portStanRecordFill builds the record outside the file, from that header,
 * as often as the file is loaded.
 *
 * The rewrite is in place, so everything is measured and checked before the
 * first write: the header, every room offset, and the whole tile walk up to
 * its zero word, each tile with its points inside the file. The tiles are
 * converted next, each point count read before its tile is touched, and at
 * 32 bits the header is written last, so that nothing written is read again
 * and a refusal leaves every byte as it was.
 *
 * Compiled only at 64 bits and in the 32-bit oracle build, where its output
 * must equal romfileStanConvert's byte for byte.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "port_romconv_stage.h"
#include "port_romlayout_stage.h"

#define STAN_MAX_POINTS 10u
#define STAN_SLOT 4u

/* The bytes a tile of COUNT points takes, 0 to 10: stan.c's
 * list_of_tilesizes, the header and at least three points (0x20 for 0 to 3
 * points, then 8 more a point, to 0x58). romconv sources do not link stan.c;
 * the oracle compares the result with the in-place pass, which uses the
 * game's table. */
static u32 stanTileBytes(u32 count)
{
    return (u32) sizeof(RomStanTile) + (count < 3 ? 3 : count) * (u32) sizeof(RomStanPoint);
}

/* What a valid file measures to. */
struct StanShape
{
    u32 rooms;
    u32 first;
    u32 tiles;
};

/* The point count of the big-endian tile at TILE: the tail's top nibble. */
static u32 stanPointCount(const u8 *tile)
{
    return portConvBe16(tile + offsetof(RomStanTile, tail)) >> 12;
}

/* Counts the room slots before the zero slot (none is allowed), checks
 * first_tile against them and inside the file, and every room offset against
 * the tiles; refuses otherwise. Reads only the header and the room slots,
 * which the converter leaves as the file has them at 64 bits. */
static int stanMeasureRooms(const u8 *data, u32 size, const char *name, struct StanShape *shape)
{
    u64 at = offsetof(RomStanHeader, rooms);
    u32 rooms = 0, first, i;

    if (size < sizeof(RomStanHeader))
    {
        portConvRefuse(PORT_LP64_COLLISION, name, "the %u-byte file is shorter than its 8-byte header", size);
        return 0;
    }
    for (;; at += STAN_SLOT, rooms++)
    {
        if (at + STAN_SLOT > size)
        {
            portConvRefuse(PORT_LP64_COLLISION, name, "the room table has no zero slot inside the %u-byte file", size);
            return 0;
        }
        if (portConvBe32(data + at) == 0)
        {
            break;
        }
    }
    first = portConvBe32(data + offsetof(RomStanHeader, first_tile));
    if ((u64) first != at + STAN_SLOT)
    {
        portConvRefuse(PORT_LP64_COLLISION, name,
                "the first tile is at 0x%X but the room table of %u slots ends at 0x%llX", first, rooms,
                (unsigned long long) (at + STAN_SLOT));
        return 0;
    }
    if ((u64) first + STAN_SLOT > size)
    {
        portConvRefuse(PORT_LP64_COLLISION, name, "the first tile at 0x%X is outside the %u-byte file", first, size);
        return 0;
    }
    for (i = 0; i < rooms; i++)
    {
        u32 off = portConvBe32(data + offsetof(RomStanHeader, rooms) + (u64) i * STAN_SLOT);

        if (off < first || off >= size)
        {
            portConvRefuse(PORT_LP64_COLLISION, name,
                    "room %u's first tile at 0x%X is outside the tiles (0x%X to 0x%X)", i, off, first, size);
            return 0;
        }
    }
    shape->rooms = rooms;
    shape->first = first;
    return 1;
}

/* Walks the tiles from the first to the zero word that ends them, as the
 * game's walks do (stanDetermineEOF, stanBuildRoomData: while the tile's
 * first word is not 0, step by list_of_tilesizes[point count]). Every step is
 * at least 0x20 bytes and every position is checked against SIZE, so the walk
 * ends within SIZE / 0x20 tiles. Refuses a file whose tiles have no zero word
 * inside it, a tile header cut by the file's end, a point count above 10 and
 * points that run past the file. */
static int stanMeasureTiles(const u8 *data, u32 size, const char *name, struct StanShape *shape)
{
    u64 at = shape->first;
    u32 tiles = 0;

    for (;;)
    {
        u32 count, bytes;

        if (at + STAN_SLOT > size)
        {
            portConvRefuse(PORT_LP64_COLLISION, name,
                    "the tiles have no zero word inside the %u-byte file (%u tiles to 0x%llX)", size, tiles,
                    (unsigned long long) at);
            return 0;
        }
        if (portConvBe32(data + at) == 0)
        {
            break;
        }
        if (at + sizeof(RomStanTile) > size)
        {
            portConvRefuse(PORT_LP64_COLLISION, name, "tile %u at 0x%llX is cut off by the end of the %u-byte file",
                    tiles, (unsigned long long) at, size);
            return 0;
        }
        count = stanPointCount(data + at);
        if (count > STAN_MAX_POINTS)
        {
            portConvRefuse(PORT_LP64_COLLISION, name, "tile %u at 0x%llX has %u points; a tile has at most %u", tiles,
                    (unsigned long long) at, count, STAN_MAX_POINTS);
            return 0;
        }
        bytes = stanTileBytes(count);
        if (at + bytes > size)
        {
            portConvRefuse(PORT_LP64_COLLISION, name,
                    "tile %u at 0x%llX has %u points (%u bytes), past the end of the %u-byte file", tiles,
                    (unsigned long long) at, count, bytes, size);
            return 0;
        }
        at += bytes;
        tiles++;
    }
    shape->tiles = tiles;
    return 1;
}

static void stanPut16(u8 *p, u16 value)
{
    memcpy(p, &value, sizeof(value));
}

/* The N64 halfword at P with its four nibbles reversed: GCC packs the four
 * 4-bit fields from the least significant end (stan.h's STAN_PTCOUNT). */
static void stanNibbleReverse(u8 *p)
{
    u16 v = portConvBe16(p);

    stanPut16(p, (u16) (((v & 0xFu) << 12) | ((v & 0xF0u) << 4) | ((v >> 4) & 0xF0u) | (v >> 12)));
}

/* One tile of COUNT points in place, as romfileStanConvert does: the word
 * becomes id:24 then room:8 in GCC's order (the N64 word rotated right by
 * 8), mid and tail have their nibbles reversed, and each point's four
 * halfwords are turned to native order. */
static void stanConvertTile(u8 *tile, u32 count)
{
    u32 w0 = portConvBe32(tile);
    u32 host = (w0 >> 8) | (w0 << 24);
    u32 p, h;

    memcpy(tile, &host, sizeof(host));
    stanNibbleReverse(tile + offsetof(RomStanTile, mid));
    stanNibbleReverse(tile + offsetof(RomStanTile, tail));
    for (p = 0; p < count; p++)
    {
        u8 *point = tile + sizeof(RomStanTile) + p * sizeof(RomStanPoint);

        for (h = 0; h < sizeof(RomStanPoint); h += 2)
        {
            stanPut16(point + h, portConvBe16(point + h));
        }
    }
}

int portStanRecordFill(const u8 *data, u32 size, uintptr_t base, struct StanPrefixRecord *record, const char *name)
{
    struct StanShape shape;

    if (!stanMeasureRooms(data, size, name, &shape))
    {
        return 0;
    }
    record->stanfile = NULL;
    record->ptr_firstroom = (StandTile *) (base + shape.first);
    return 1;
}

int portStanConvert(u8 *data, u32 size, uintptr_t base, const char *name)
{
    struct StanShape shape;
    struct StanPrefixRecord record;
    u32 at, t, i;

    if (!stanMeasureRooms(data, size, name, &shape) || !stanMeasureTiles(data, size, name, &shape))
    {
        return 0;
    }

    /* The tiles, each count read from its own unconverted tail. */
    at = shape.first;
    for (t = 0; t < shape.tiles; t++)
    {
        u32 count = stanPointCount(data + at);

        stanConvertTile(data + at, count);
        at += stanTileBytes(count);
    }

    /* Where the record and a room's address fit in the file's 4-byte words
     * (32 bits): each room slot becomes its tile's address, as the game's
     * N64 rebase would make it, and last the record goes over the two header
     * words. At 64 bits the header stays as the file has it. */
    if (sizeof(StandTile *) == STAN_SLOT)
    {
        for (i = 0; i < shape.rooms; i++)
        {
            u8 *slot = data + offsetof(RomStanHeader, rooms) + i * STAN_SLOT;
            StandTile *room = (StandTile *) (base + portConvBe32(slot));

            memcpy(slot, &room, sizeof(room));
        }
        record.stanfile = NULL;
        record.ptr_firstroom = (StandTile *) (base + shape.first);
        memcpy(data, &record, sizeof(record));
    }
    return 1;
}
