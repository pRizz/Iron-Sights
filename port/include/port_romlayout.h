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

/* ==== animation tables (animation_data; bondtypes.h's ModelAnimation) === */

/**
 * An animation's header inside the animation_data segment, at the offset a
 * PTR_ANIM_* constant names: three offsets and four small fields, 0x14 bytes.
 * portAnimDataInit (animdata.c) turns its fields to native byte order at
 * every width, so the converter reads it as stored, not through portConvBe*.
 * entries_offset is into the animation_entries segment (the frame streams);
 * descriptors_offset and stream_offset are into animation_data itself.
 */
typedef struct RomAnimHeader
{
    u32 entries_offset;     /* ModelAnimation.address */
    u16 frames;             /* unk04 */
    u8 unk06;
    u8 unk07;               /* bit 0: loop */
    u32 descriptors_offset; /* bitDescriptors: 6-byte ModelAnimBitField records */
    u16 unk0c;
    u16 frame_bits;         /* unk0E: the bits of one frame */
    u32 stream_offset;      /* bitStream */
} RomAnimHeader;
_Static_assert(sizeof(RomAnimHeader) == 0x14, "RomAnimHeader is 0x14 bytes");

/* ==== audio banks (the sfxctl and instrumentsctl images; libaudio.h) ==== */

/**
 * The five records of an SGI bank file (revision 'B1') that hold offsets, as
 * the cart holds them, big-endian. Each offset is from the file's start, except
 * a wave table's base, which is into the matching .tbl segment. Four more
 * kinds hold no offset and stay in the file image at every width: ALEnvelope
 * (three s32, two u8), ALKeyMap (six u8), ALADPCMBook (two s32, then
 * order * npredictors * 8 s16), ALADPCMloop (three u32, then 16 s16) and
 * ALRawLoop (three u32).
 */
typedef struct RomBankFile
{
    u16 revision; /* AL_BANK_VERSION */
    u16 bankCount;
    /* then bankCount u32 offsets of RomBank */
} RomBankFile;
_Static_assert(sizeof(RomBankFile) == 4, "RomBankFile is 4 bytes");

typedef struct RomBank
{
    u16 instCount;
    u8 flags;
    u8 pad;
    u32 sampleRate;
    u32 percussion; /* a RomInstrument, or 0 for none */
    /* then instCount u32 offsets of RomInstrument (0 names the file's start) */
} RomBank;
_Static_assert(sizeof(RomBank) == 12, "RomBank is 12 bytes");

typedef struct RomInstrument
{
    u8 volume;
    u8 pan;
    u8 priority;
    u8 flags;
    u8 tremType;
    u8 tremRate;
    u8 tremDepth;
    u8 tremDelay;
    u8 vibType;
    u8 vibRate;
    u8 vibDepth;
    u8 vibDelay;
    u16 bendRange;
    u16 soundCount;
    /* then soundCount u32 offsets of RomSound */
} RomInstrument;
_Static_assert(sizeof(RomInstrument) == 16, "RomInstrument is 16 bytes");

typedef struct RomSound
{
    u32 envelope;  /* an ALEnvelope */
    u32 keyMap;    /* an ALKeyMap */
    u32 wavetable; /* a RomWaveTable */
    u8 samplePan;
    u8 sampleVolume;
    u8 flags;
    u8 pad;
} RomSound;
_Static_assert(sizeof(RomSound) == 16, "RomSound is 16 bytes");

typedef struct RomWaveTable
{
    u32 base; /* into the .tbl segment */
    u32 len;
    u8 type;  /* AL_ADPCM_WAVE or AL_RAW16_WAVE */
    u8 flags;
    u8 pad[2];
    u32 loop; /* an ALADPCMloop or ALRawLoop, or 0 */
    u32 book; /* an ALADPCMBook, or 0; ADPCM only */
} RomWaveTable;
_Static_assert(sizeof(RomWaveTable) == 20, "RomWaveTable is 20 bytes");

/* The bytes of the in-file kinds that the game reads (libaudio.h's structs
 * have the same fields at every width); a book's entries follow its header. */
#define ROM_BANK_ENVELOPE_BYTES 14u  /* three s32, two u8 */
#define ROM_BANK_KEYMAP_BYTES 6u
#define ROM_BANK_BOOK_HEAD 8u        /* order, npredictors */
#define ROM_BANK_ADPCMLOOP_BYTES 44u /* three u32, 16 s16 */
#define ROM_BANK_RAWLOOP_BYTES 12u

/* ==== music sequences (the musicsampletbl segment, music.sbk; music.h) == */

/**
 * The sequence bank's header, big-endian: a count, a half-word nothing reads,
 * then count entries, each the sequence's offset from the file's start and
 * its two lengths (after and before Rare's 1172 decompression). The
 * compressed streams follow the entries. music.h's RareALSeqData puts the
 * offset in a pointer slot, so at 64 bits a run-time entry is 16 bytes and the
 * header 8.
 */
typedef struct RomSeqTable
{
    u16 count;
    u16 unk;
    /* then count RomSeqEntry */
} RomSeqTable;
_Static_assert(sizeof(RomSeqTable) == 4, "RomSeqTable is 4 bytes");

typedef struct RomSeqEntry
{
    u32 offset; /* RareALSeqData.address, from the file's start */
    u16 uncompressed_len;
    u16 len;
} RomSeqEntry;
_Static_assert(sizeof(RomSeqEntry) == 8, "RomSeqEntry is 8 bytes");

#endif
