/**
 * The ROM's file layouts of the stage classes (milestone LP64-004): collision
 * tiles and setup files. The rules are those of
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

/* ==== setup files (Usetup<stage>Z, Ump_setup<stage>Z; src/game/prop.c) === */

/*
 * A setup file starts with ten offsets from its start: the run-time struct
 * stagesetup's fields in its order. The first eight name tables the game
 * always reads; padnames and boundpadnames may be 0 for none. Each table is
 * a run of records ended by a terminator record that is itself converted:
 * pads and bound pads at plink 0, waypoints at a negative padID, waygroups at
 * neighbours 0, paths at waypoints 0, AI lists at ailist 0, the two name
 * tables at a 0 word. The intro is a run of records chosen by their first
 * word's low byte (port/generated/setup_kinds.h's kSetupIntroKinds gives each
 * type's word count) up to the end record; the prop records likewise by
 * kSetupPropdefKinds up to PROPDEF_END. Neighbour and waypoint lists are s32
 * words ended by a negative one; names are NUL-ended bytes; AI lists are byte
 * code ended by the command AI_EndList; the credits table is RomCreditsEntry
 * records ended by one whose two text IDs are 0.
 */
typedef struct RomSetupHeader
{
    u32 pathwaypoints;  /* RomWaypoint table */
    u32 waypointgroups; /* RomWaygroup table */
    u32 intro;          /* intro records */
    u32 propDefs;       /* prop records */
    u32 patrolpaths;    /* RomPath table */
    u32 ailists;        /* RomAiList table */
    u32 pads;           /* RomPad table */
    u32 boundpads;      /* RomBoundPad table */
    u32 padnames;       /* name offsets, ended by 0; or 0 */
    u32 boundpadnames;  /* name offsets, ended by 0; or 0 */
} RomSetupHeader;
_Static_assert(sizeof(RomSetupHeader) == 40, "RomSetupHeader is ten words");

/* A pad: three float vectors, its name's offset (0 ends the table) and a
 * word the game overwrites with the pad's tile. */
typedef struct RomPad
{
    u32 pos[3];
    u32 up[3];
    u32 look[3];
    u32 plink;
    u32 stan;
} RomPad;
_Static_assert(sizeof(RomPad) == 44, "RomPad is 44 bytes");

/* A bound pad: a pad, then its bounding box's six floats. */
typedef struct RomBoundPad
{
    RomPad pad;
    u32 bbox[6];
} RomBoundPad;
_Static_assert(sizeof(RomBoundPad) == 68, "RomBoundPad is 68 bytes");

/* A waypoint: its pad (negative ends the table), its neighbour list's
 * offset, its group and a distance the game initialises. */
typedef struct RomWaypoint
{
    u32 padID;
    u32 neighbours;
    u32 groupNum;
    u32 dist;
} RomWaypoint;
_Static_assert(sizeof(RomWaypoint) == 16, "RomWaypoint is 16 bytes");

/* A waygroup: its neighbour list's offset (0 ends the table), its waypoint
 * list's offset and a distance. */
typedef struct RomWaygroup
{
    u32 neighbours;
    u32 waypoints;
    u32 dist;
} RomWaygroup;
_Static_assert(sizeof(RomWaygroup) == 12, "RomWaygroup is 12 bytes");

/* A patrol path: its waypoint list's offset (0 ends the table), two bytes
 * and a length the game recomputes. */
typedef struct RomPath
{
    u32 waypoints;
    u8 ID;
    u8 isLoop;
    u16 len;
} RomPath;
_Static_assert(sizeof(RomPath) == 8, "RomPath is 8 bytes");

/* An AI list: its byte code's offset (0 ends the table) and its ID. */
typedef struct RomAiList
{
    u32 ailist;
    u32 ID;
} RomAiList;
_Static_assert(sizeof(RomAiList) == 8, "RomAiList is 8 bytes");

/* One line of the intro's credits: two text IDs, then a position and an
 * alignment for each. */
typedef struct RomCreditsEntry
{
    u16 TextId1;
    u16 TextId2;
    u16 Position1;
    u16 Alignment1;
    u16 Position2;
    u16 Alignment2;
} RomCreditsEntry;
_Static_assert(sizeof(RomCreditsEntry) == 12, "RomCreditsEntry is 12 bytes");

/* The intro records by their word counts. Word 0 holds the type in its low
 * byte. Two differ from their run-time structs at 64 bits: the camera's two
 * text IDs and its prev link become pointer-sized unions and a pointer, and
 * the credits record's table offset a pointer-sized offset. */
typedef struct RomIntroSpawn { u32 type; u32 index; u32 demo; } RomIntroSpawn;
typedef struct RomIntroItem { u32 type; u32 right; u32 left; u32 demo; } RomIntroItem;
typedef struct RomIntroAmmo { u32 type; u32 ammo; u32 amount; u32 demo; } RomIntroAmmo;
typedef struct RomIntroSwirl { u32 type; u32 flags; u32 offset[3]; u32 scale; u32 duration; u32 lookat; } RomIntroSwirl;
typedef struct RomIntroAnim { u32 type; u32 anim; } RomIntroAnim;
typedef struct RomIntroCuff { u32 type; u32 bondtype; } RomIntroCuff;
typedef struct RomIntroCamera { u32 type; u32 coords[3]; u32 yaw; u32 pitch; u32 unk18; u32 text1; u32 text2; u32 prev; } RomIntroCamera;
typedef struct RomIntroWatch { u32 type; u32 hours; u32 minutes; } RomIntroWatch;
typedef struct RomIntroCredits { u32 type; u32 table; } RomIntroCredits;
typedef struct RomIntroEnd { u32 type; } RomIntroEnd;
_Static_assert(sizeof(RomIntroSpawn) == 12 && sizeof(RomIntroItem) == 16 && sizeof(RomIntroAmmo) == 16
                   && sizeof(RomIntroSwirl) == 32 && sizeof(RomIntroAnim) == 8 && sizeof(RomIntroCuff) == 8
                   && sizeof(RomIntroCamera) == 40 && sizeof(RomIntroWatch) == 12 && sizeof(RomIntroCredits) == 8
                   && sizeof(RomIntroEnd) == 4,
               "intro records are 3, 4, 4, 8, 2, 2, 10, 3, 2 and 1 words");

#endif
