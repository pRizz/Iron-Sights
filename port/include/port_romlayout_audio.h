/**
 * The ROM's file layouts of the audio classes (milestone LP64-004): the audio
 * bank images and the music sequence bank, split from port_romlayout.h, whose
 * rules they keep: each struct is the bytes of a ROM file or record exactly as
 * the cart holds them, so it is the same at every pointer width, with u8, u16,
 * u32 and s16 fields only and a _Static_assert on its size beside it. The
 * values are big-endian on the cart; the converters read them through
 * portConvBe16/portConvBe32 (port_romconv.h).
 */
#ifndef PORT_ROMLAYOUT_AUDIO_H
#define PORT_ROMLAYOUT_AUDIO_H

#include <PR/ultratypes.h>

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
