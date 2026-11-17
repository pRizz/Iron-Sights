/**
 * The ROM's file layouts of the stage classes (milestone LP64-004): collision
 * tiles now, setup files in the tasks that follow. The rules are those of
 * port_romlayout.h, whose 521 lines leave no room for these: each struct is
 * the bytes of a ROM file or record exactly as the cart holds them, the same
 * at every pointer width, with u8, u16, u32 and s16 fields only (an offset is
 * a u32, never a pointer) and a _Static_assert on its size beside it. The
 * values are big-endian on the cart; a converter reads them through
 * portConvBe16/portConvBe32 (port_romconv.h).
 *
 * One section a class; each converter task adds its own.
 */
#ifndef PORT_ROMLAYOUT_STAGE_H
#define PORT_ROMLAYOUT_STAGE_H

#include <PR/ultratypes.h>

/* ==== collision tiles (Tbg_<stage>_all_p_stanZ; src/game/stan.c) ======== */

/*
 * A stan file, as the cart holds it: this header, one u32 slot a room holding
 * the offset of the room's first tile, a zero slot, then the tiles back to
 * back from first_tile, ended by a zero word. first_tile is therefore
 * 8 + (rooms + 1) * 4. Every offset is the distance from the file's start.
 */
typedef struct RomStanHeader
{
    u32 unk;        /* 0 in every stock file; the game's stanfile slot */
    u32 first_tile; /* the first tile */
    u32 rooms[];    /* each room's first tile, then 0 */
} RomStanHeader;
_Static_assert(sizeof(RomStanHeader) == 8, "RomStanHeader is 8 bytes before its room slots");

/*
 * A tile's header: the N64 word with the 24-bit id high and the 8-bit room
 * low, then two halfwords of four nibbles each, most significant first: mid
 * {special, r, g, b} and tail {point count, the three extreme points}. The
 * points follow: a tile takes stan.c's list_of_tilesizes[point count] bytes,
 * the header and at least three points (0x20 to 0x58 bytes for 0 to 10
 * points; a count above 10 has no size).
 */
typedef struct RomStanTile
{
    u32 idroom;
    u16 mid;
    u16 tail;
} RomStanTile;
_Static_assert(sizeof(RomStanTile) == 8, "RomStanTile is 8 bytes");

/* One point of a tile: its position and the link to the tile across the edge
 * that starts at it (an offset in 8-byte units from 0x80 bytes before the
 * first tile, the game's standTileStart; 0 for none). */
typedef struct RomStanPoint
{
    s16 x;
    s16 y;
    s16 z;
    u16 link;
} RomStanPoint;
_Static_assert(sizeof(RomStanPoint) == 8, "RomStanPoint is 8 bytes");

#endif
