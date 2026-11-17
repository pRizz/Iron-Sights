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

/* ==== fonts (the fontbankgothic and fontzurichbold segments; bondtypes.h) == */

/**
 * A Latin font segment, big-endian: the 13 x 13 kerning table, the 94
 * characters' records, then the glyph bytes (8-bit intensity, read as bytes).
 * A character's pixel_offset is from the segment's start, or 0. bondtypes.h's
 * struct fontchar keeps it in a pointer slot (pixeldata), so at 64 bits a
 * record is 32 bytes and struct font 0x2F4 bytes longer than this table.
 */
typedef struct RomFontChar
{
    u32 index;
    u32 baseline;
    u32 height;
    u32 width;
    u32 kerningindex;
    u32 pixel_offset; /* struct fontchar.pixeldata */
} RomFontChar;
_Static_assert(sizeof(RomFontChar) == 24, "RomFontChar is 24 bytes");

typedef struct RomFont
{
    u32 kerning[13 * 13];
    RomFontChar chars[94];
    /* then the glyph bytes */
} RomFont;
_Static_assert(sizeof(RomFont) == 0xB74, "RomFont is 0xB74 bytes");

/* ==== models (C<name>Z, G<name>Z and P<name>Z; bondtypes.h's model records) == */

/*
 * A model file, big-endian: numSwitches u32 node references, numtextures
 * RomModelTexture entries, then the root RomModelNode; nodes, their records
 * and the vertex, point-usage, inline-texel and display-list blocks follow,
 * interleaved. Every reference is 0x05000000 plus an offset from the file's
 * start, or 0. The counts are not in the file: they come from the compiled
 * ModelFileHeader. Each record's size is romfile_model.c's modelRecordSize;
 * opcodes 3 and 17 use the group record and 20 the header record there.
 */
typedef struct RomModelNode
{
    u16 opcode; /* the low byte; the high byte holds flags */
    u16 pad;
    u32 data;
    u32 parent;
    u32 next;
    u32 prev;
    u32 child;
} RomModelNode;
_Static_assert(sizeof(RomModelNode) == 24, "RomModelNode is 24 bytes");

typedef struct RomModelTexture
{
    u32 textureid; /* a texture number, or a reference to inline texels */
    u8 width;
    u8 height;
    u8 mipmaptiles;
    u8 type;
    u8 renderdepth;
    u8 sflags;
    u8 tflags;
    u8 pad;
} RomModelTexture;
_Static_assert(sizeof(RomModelTexture) == 12, "RomModelTexture is 12 bytes");

/** A render or collision vertex: six half-words, then a render vertex's s/t
 * or a collision vertex's link, then four bytes (colour, or a collision
 * vertex's related index and a spare half-word). */
typedef struct RomModelVertex
{
    s16 coord[3];
    s16 index;
    u32 link;
    u8 tail[4];
} RomModelVertex;
_Static_assert(sizeof(RomModelVertex) == 16, "RomModelVertex is 16 bytes");

typedef struct RomModelHeader /* opcodes 1 and 20 */
{
    u16 animpart;
    s16 matrixindex;
    u32 firstgroup;
    u16 group1;
    u16 group2;
    u16 rwdataindex;
    u16 reserved;
} RomModelHeader;
_Static_assert(sizeof(RomModelHeader) == 16, "RomModelHeader is 16 bytes");

typedef struct RomModelGroup /* opcodes 2, 3 and 17 */
{
    u32 origin[3];
    u16 jointid;
    s16 matrixids[3];
    u32 childgroup;
    u32 radius;
} RomModelGroup;
_Static_assert(sizeof(RomModelGroup) == 28, "RomModelGroup is 28 bytes");

typedef struct RomModelDL /* opcode 4 */
{
    u32 primary;
    u32 secondary;
    u32 baseaddr;
    u32 vertices;
    u16 numvertices;
    u8 modeltype;
    u8 pad;
} RomModelDL;
_Static_assert(sizeof(RomModelDL) == 20, "RomModelDL is 20 bytes");

typedef struct RomModelDLCollision /* opcode 24 */
{
    u32 primary;
    u32 secondary;
    u32 vertices;
    u16 numvertices;
    u16 numcollisionvertices;
    u32 collisionvertices;
    u32 pointusage;
    u16 modeltype;
    u16 rwdataindex;
    u32 baseaddr;
} RomModelDLCollision;
_Static_assert(sizeof(RomModelDLCollision) == 32, "RomModelDLCollision is 32 bytes");

typedef struct RomModelDLPrimary /* opcode 22; numvertices counts quads */
{
    u32 numvertices;
    u32 vertices;
    u32 primary;
    u32 baseaddr;
} RomModelDLPrimary;
_Static_assert(sizeof(RomModelDLPrimary) == 16, "RomModelDLPrimary is 16 bytes");

typedef struct RomModelLOD /* opcode 8 */
{
    u32 mindistance;
    u32 maxdistance;
    u32 affects;
    u16 rwdataindex;
    u16 reserved;
} RomModelLOD;
_Static_assert(sizeof(RomModelLOD) == 16, "RomModelLOD is 16 bytes");

typedef struct RomModelBSP /* opcode 9 */
{
    u32 point[3];
    u32 vector[3];
    u32 leftchild;
    u32 rightchild;
    u16 reserved;
    u16 rwdataindex;
} RomModelBSP;
_Static_assert(sizeof(RomModelBSP) == 36, "RomModelBSP is 36 bytes");

typedef struct RomModelBBox /* opcode 10 */
{
    u32 modelnumber;
    u32 bounds[6];
} RomModelBBox;
_Static_assert(sizeof(RomModelBBox) == 28, "RomModelBBox is 28 bytes");

typedef struct RomModelGunfire /* opcode 12 */
{
    u32 offset[3];
    u32 size[3];
    u32 image; /* a texture entry */
    u32 scale;
    u16 rwdataindex;
    u16 reserved;
    u32 baseaddr;
} RomModelGunfire;
_Static_assert(sizeof(RomModelGunfire) == 40, "RomModelGunfire is 40 bytes");

typedef struct RomModelShadow /* opcode 13 */
{
    u32 pos[2];
    u32 size[2];
    u32 image; /* a texture entry */
    u32 header;
    u32 scale;
    u32 baseaddr;
} RomModelShadow;
_Static_assert(sizeof(RomModelShadow) == 32, "RomModelShadow is 32 bytes");

typedef struct RomModelSwitch /* opcode 18 */
{
    u32 controls;
    u16 rwdataindex;
    u16 reserved;
} RomModelSwitch;
_Static_assert(sizeof(RomModelSwitch) == 8, "RomModelSwitch is 8 bytes");

typedef struct RomModelOp06 /* opcode 6 */
{
    u32 unk[5];
    u32 baseaddr;
} RomModelOp06;
_Static_assert(sizeof(RomModelOp06) == 24, "RomModelOp06 is 24 bytes");

typedef struct RomModelGroupSimple /* opcode 21 */
{
    u32 origin[3];
    u16 group1;
    u16 group2;
    u32 radius;
} RomModelGroupSimple;
_Static_assert(sizeof(RomModelGroupSimple) == 20, "RomModelGroupSimple is 20 bytes");

typedef struct RomModelHead /* opcode 23 */
{
    u16 rwdataindex;
    u8 pad[2];
} RomModelHead;
_Static_assert(sizeof(RomModelHead) == 4, "RomModelHead is 4 bytes");

typedef struct RomModelInterlink /* opcode 15 */
{
    u32 pos[3];
    u32 pos2[3];
    u32 scale;
} RomModelInterlink;
_Static_assert(sizeof(RomModelInterlink) == 28, "RomModelInterlink is 28 bytes");

typedef struct RomModelOp11 /* opcode 11; unk0c[0..2] are the position */
{
    u32 unk0c[16]; /* [15] is a reference */
    u32 radius;
    u16 rwdataindex;
    u16 unk46;
    u32 baseaddr;
} RomModelOp11;
_Static_assert(sizeof(RomModelOp11) == 76, "RomModelOp11 is 76 bytes");

typedef struct RomModelOp05 /* opcode 5 */
{
    u32 numchildren;
    u32 children;
    u32 vertices;
    u32 images;
    u8 data[400];
    u32 unk1a0;
    u32 baseaddr;
} RomModelOp05;
_Static_assert(sizeof(RomModelOp05) == 424, "RomModelOp05 is 424 bytes");

typedef struct RomModelOp07 /* opcode 7 */
{
    u32 unk00; /* a node */
    u32 unk04; /* a node */
    u32 numchildren;
    u32 children;
    u32 vertices;
    u32 images;
    u8 data[400];
    u16 unk1a8;
    u16 rwdataindex;
    u32 baseaddr;
} RomModelOp07;
_Static_assert(sizeof(RomModelOp07) == 432, "RomModelOp07 is 432 bytes");

/* ==== backgrounds (bg/bg_<stage>_all_p.seg; src/game/bg.h) ============== */

/*
 * A bg file's front matter, as the cart holds it: the header, the room table
 * (from the header's rooms offset, a record of zeros first, the rooms, an end
 * marker whose three offsets mark where the last room's data ends, and a
 * record of zeros), the global visibility commands, the portal table (ended
 * by an entry whose offset is 0) and the portal points; the rooms'
 * compressed data follows. Every offset is a segment 0x0F word: 0x0F000000
 * plus the distance from the file's start. romfile_bg.c turns the words to
 * native byte order in place; the background converter (romconv_bg.c) reads
 * them after that pass.
 */
typedef struct RomBgHeader
{
    u32 reserved;
    u32 rooms;   /* the room table */
    u32 portals; /* the portal table */
    u32 vis;     /* the global visibility commands, or 0 */
    u32 padding; /* a float table on the N64 path, 0 in every stock file */
} RomBgHeader;
_Static_assert(sizeof(RomBgHeader) == 0x14, "RomBgHeader is 0x14 bytes");

typedef struct RomBgRoom
{
    u32 point;     /* the compressed vertex table, or 0 */
    u32 primary;   /* the compressed primary display list, or 0 */
    u32 secondary; /* the compressed secondary display list, or 0 */
    u32 pos[3];    /* the room's position, three big-endian floats */
} RomBgRoom;
_Static_assert(sizeof(RomBgRoom) == 24, "RomBgRoom is 24 bytes");

typedef struct RomBgPortal
{
    u32 offset; /* the portal's points: a byte count, three pad bytes, then
                 * that many coord3d; 0 ends the table */
    u8 room1;
    u8 room2;
    u8 cb1; /* the control bytes, in the run-time order (bg.h) */
    u8 cb2;
} RomBgPortal;
_Static_assert(sizeof(RomBgPortal) == 8, "RomBgPortal is 8 bytes");

#endif
