/**
 * The ROM's file layouts (scripts/port/native_64bit_plan.md, "ROM data: file
 * layouts and converters"; milestone LP64-004).
 *
 * Each struct is the bytes of a ROM file or record exactly as the cart holds
 * them, so it is the same at every pointer width: named Rom..., with u8, u16,
 * u32 and s16 fields only (an offset is a u32, never a pointer), and a
 * _Static_assert on its size beside it. The values are big-endian on the cart;
 * a converter reads them through portConvBe16/portConvBe32 (port_romconv.h)
 * or after the game's own byte swap, whichever the class's task says.
 *
 * One section a class; each converter task adds its own.
 */
#ifndef PORT_ROMLAYOUT_H
#define PORT_ROMLAYOUT_H

#include <PR/ultratypes.h>

/* ==== attract demo header (src/game/ramromreplay.h) ===================== */

/**
 * The header of an attract demo recording, as the cart holds it: 0xE8 bytes.
 * The game's ramromfilestructure has two u64 fields, so it is 232 (0xE8) bytes
 * wherever a u64 is 8-byte aligned in a struct (the N64, MinGW and every
 * 64-bit build) and 228 on i686 Linux, which leaves out the last four padding
 * bytes; the field offsets are the same everywhere. Playback steps past the
 * header by RAMROM_HEADER_ROMSIZE (0xE8, ramromreplay.c), not by sizeof, and
 * the header holds no pointer, so no converter is needed.
 */
typedef struct RomDemoHeader
{
    u32 randomseed[2]; /* a u64, high word first */
    u32 randomizer[2]; /* a u64, high word first */
    u32 stagenum;
    u32 difficulty;
    u32 size_cmds;
    /* save_data, 96 bytes */
    u32 save_chksum1;
    u32 save_chksum2;
    u8 save_completion_bitflags;
    u8 save_flag_007;
    u8 save_music_vol;
    u8 save_sfx_vol;
    u16 save_options;
    u8 save_unlocked_cheats[3];
    u8 save_padding;
    u8 save_times[76];
    u8 save_tail[2]; /* the C struct's trailing padding */
    u32 totaltime_ms;
    u32 filesize;
    u32 mode;
    u32 slotnum;
    u32 numplayers;
    u32 scenario;
    u32 mpstage_sel;
    u32 gamelength;
    u32 mp_weapon_set;
    u32 mp_char[4];
    u32 mp_handi[4];
    u32 mp_contstyle[4];
    u32 aim_option;
    u32 mp_flags[4];
    u8 tail[4]; /* the N64 struct's trailing padding, to 8-byte alignment */
} RomDemoHeader;
_Static_assert(sizeof(RomDemoHeader) == 0xE8, "RomDemoHeader is 0xE8 bytes");

/* ==== briefings (Ubrief<level>Z; front.c's struct BriefingData) ========= */

/** One objective line of a briefing: a text ID and the difficulties it shows on. */
typedef struct RomBriefObjective
{
    u16 textid;
    u16 enabled_difficulty;
} RomBriefObjective;
_Static_assert(sizeof(RomBriefObjective) == 4, "RomBriefObjective is 4 bytes");

/** 24 half-words and no pointer: romdata.c swaps them in place at every width. */
typedef struct RomBrief
{
    u16 brief[4];
    RomBriefObjective objective[10];
} RomBrief;
_Static_assert(sizeof(RomBrief) == 48, "RomBrief is 48 bytes");

#endif
