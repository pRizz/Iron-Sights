/**
 * The ROM data converters of the audio classes (milestone LP64-004): audio
 * banks and music sequences. Their file layouts are in port_romlayout_audio.h.
 * They were split from port_romconv.h, which holds the shared helpers
 * (portConvBe32, portConvRefuse and the rest) and the other classes'
 * converters, to keep each header under 500 lines; its rules are theirs:
 * measure before writing, never read what is being written, zero-extend every
 * offset into its pointer-sized slot, and refuse by name. The sources,
 * port/src/romconv_*.c, are compiled only into 64-bit builds and into the
 * opt-in 32-bit oracle build (-DGE_LP64_ORACLE=ON); every call from shared
 * code sits inside `#if IS_64_BIT` or `#ifdef GE_LP64_ORACLE`.
 */
#ifndef PORT_ROMCONV_AUDIO_H
#define PORT_ROMCONV_AUDIO_H

#include <stdint.h>
#include <PR/ultratypes.h>
#include <PR/libaudio.h> /* the audio bank records */
#include "music.h"       /* RareALSeqBankFile */
#include "port_romconv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- audio banks (romconv_bank.c) ------------------------------------- */

/**
 * The bytes of run-time records portBankConvert writes for the big-endian bank
 * file FILE (SIZE bytes, port_romlayout_audio.h's RomBankFile) whose waves lead into
 * a wave table of TABLESIZE bytes: one ALBankFile and one record for each
 * distinct ALBank, ALInstrument, ALSound and ALWaveTable the file reaches, each
 * rounded up to 8. Reads only. Returns 0 after portConvRefuse, naming NAME (the
 * bank, as the caller knows it), when the revision is not AL_BANK_VERSION, a
 * count is negative, a record or array runs past the file's end, an offset is
 * not a multiple of 4, one offset is reached as two kinds of record, two
 * records reached share a byte (an instrument's or bank's offset array is part
 * of its record), or a wave's base + len passes TABLESIZE, whatever its type
 * (types other than ADPCM and raw since LP64-007 Task 7).
 */
u32 portBankConvertedSize(const u8 *file, u32 size, u32 tablesize, const char *name);

/**
 * Builds the run-time graph of the bank file FILE (SIZE bytes) in OUT (OUTSIZE
 * bytes, 8-byte aligned) and returns OUT as its ALBankFile. Each distinct
 * bank, instrument, sound and wave table is written once, its flags byte 1;
 * envelopes, key maps, ADPCM books and loops stay in FILE and are turned to
 * native byte order there, once each, as the 32-bit alBnkfNew does. A wave's
 * base is TABLE + its offset, and a 16-bit raw wave's samples are swapped in
 * TABLE (TABLESIZE bytes), once a base. An instrument or bank offset of 0 gives
 * a pointer to FILE itself and is not followed, as alBnkfNew does.
 *
 * Returns NULL after portConvRefuse, naming NAME, having written nothing to
 * FILE, TABLE or OUT, when portBankConvertedSize refuses, when OUTSIZE is less
 * than it, or when OUT is not 8-byte aligned. The file's flags bytes are not
 * read: a record already marked converted is converted again.
 */
ALBankFile *portBankConvert(u8 *file, u32 size, u8 *table, u32 tablesize, u8 *out, u32 outsize, const char *name);

/** audio_stubs.c, at 64 bits: converts the copied bank file FILE (SIZE bytes),
 * whose wave table TABLE is the ROM segment TBLNAME, into records allocated
 * from HEAP and returns them; stops the game, naming NAME, when the file is
 * refused or TABLE is not that segment. */
ALBankFile *portBnkfConvert(ALHeap *heap, ALBankFile *file, u32 size, u8 *table, const char *name,
        const char *tblname);

#ifdef GE_LP64_ORACLE
/** One bank graph for portBankGraphsEqual: its ALBankFile, the file image its
 * in-file records lie in (SIZE bytes), and the wave table its bases lead into. */
struct PortBankGraph
{
    const ALBankFile *root;
    const u8 *file;
    u32 size;
    const u8 *table;
};

/**
 * Walks A and B from their bank files down and compares every scalar field.
 * A pointer to a bank, instrument, sound or wave table is equal when both lead
 * to records that compare equal, or both to their file's start; one to an
 * envelope, key map, book or loop when both lie at the same offset of their
 * file and hold the same bytes; a wave's base when both are at the same offset
 * of their table. Returns 1, or 0 with WHY (WHYSIZE bytes) naming the first
 * difference.
 */
int portBankGraphsEqual(const struct PortBankGraph *a, const struct PortBankGraph *b, char *why, u32 whysize);

/**
 * The oracle (32-bit, -DGE_LP64_ORACLE=ON), called after alBnkfNew converted
 * INPLACE (the bank file NAME, SIZE bytes) with INPLACETABLE (TABLESIZE bytes):
 * converts PRISTINE, a copy of the file made before, with TABLECOPY, a copy of
 * the table made before, compares the two graphs and the two tables, and logs
 * one verdict for NAME.
 */
void portBankOracle(const char *name, const ALBankFile *inplace, const u8 *inplacetable, u8 *pristine, u32 size,
        u8 *tablecopy, u32 tablesize);

/** audio_stubs.c, in the oracle build: alBnkfNew(FILE, TABLE) with
 * portBankOracle beside it; TBLNAME is the table's ROM segment. */
void portBnkfOracle(ALBankFile *file, u32 size, u8 *table, const char *name, const char *tblname);
#endif

/* ---- music sequences (romconv_seq.c) ---------------------------------- */

/**
 * The bytes of the run-time RareALSeqBankFile for the big-endian sequence bank
 * FILE (port_romlayout_audio.h's RomSeqTable): offsetof(RareALSeqBankFile, seqArray)
 * + count * sizeof(RareALSeqData). Reads the count only. The count is 16-bit,
 * so the size is at most 1,048,568 bytes at 64 bits and never wraps.
 *
 * Precondition: FILE has at least sizeof(RomSeqTable) (4) readable bytes.
 * The function has no size to check that against, so the caller checks the
 * file's size first, as portSeqTableLoad and portSeqTableOracle do. The size
 * returned says nothing about the file's length: only portSeqTableConvert
 * checks that the count's entries fit the file.
 */
u32 portSeqTableSize(const u8 *file);

/**
 * Writes the run-time table of the sequence bank FILE (FILESIZE bytes) into
 * OUT (OUTSIZE bytes): seqCount, then for each entry its offset zero-extended
 * into address (the game's musicSeqFileNew adds the segment's address) and
 * its two lengths, in native byte order. The bytes OUT's padding and unk
 * occupy are written as zero, except unk itself, which keeps the file's two
 * bytes as stored, as the 32-bit in-place copy leaves them; nothing reads it.
 *
 * Returns 1, or 0 after portConvRefuse, with nothing written to OUT, when
 * FILESIZE is less than the header, or than the header and count entries,
 * the count is less than NUM_MUSIC_TRACKS (63, the entries musicInit reads
 * whatever the count; more are converted and never read), an entry's
 * offset + len passes FILESIZE (the bytes musicTrack1Play and its two twins
 * read), an entry's len rounded up to 16 is more than the bytes its
 * uncompressed_len leaves (made even, rounded up to 16, plus 64: the copy
 * ends there and would start before the track buffer), or OUTSIZE is less
 * than portSeqTableSize (the count, the copy and the len bounds since
 * LP64-007 Task 7). Each entry's offset and lengths are otherwise copied as
 * they are. Left to music.c, which reads them: it copies len rounded up to 16
 * bytes (9 bytes past the stock file for its last sequence, inside the ROM
 * image), and whether the uncompressed sequence fits its track buffer
 * depends on which of the three players plays it (FORK-089's LP64-007 Task 7
 * note).
 */
int portSeqTableConvert(const u8 *file, u32 filesize, RareALSeqBankFile *out, u32 outsize);

/** audio_stubs.c, at 64 bits: converts the sequence bank SEGMENT, the ROM
 * segment NAME, into a table allocated from HEAP at portSeqTableSize, and
 * returns it; stops the game, naming NAME, when SEGMENT is not that segment,
 * the segment is shorter than the header, or the converter refuses. */
RareALSeqBankFile *portSeqTableLoad(ALHeap *heap, const u8 *segment, const char *name);

#ifdef GE_LP64_ORACLE
/**
 * The oracle (32-bit, -DGE_LP64_ORACLE=ON), called after the game copied and
 * swapped INPLACE and before musicSeqFileNew rebased it: converts FILE
 * (FILESIZE bytes, the segment INPLACE was copied from) into new memory and
 * compares the portSeqTableSize bytes of both, as the file musicsampletbl.
 */
void portSeqTableOracle(const RareALSeqBankFile *inplace, const u8 *file, u32 filesize);

/** audio_stubs.c, in the oracle build: portSeqTableOracle for SEGMENT, the ROM
 * segment NAME, with the segment's size from romdataSeg. */
void portSeqOracle(const RareALSeqBankFile *inplace, const u8 *segment, const char *name);
#endif

#ifdef __cplusplus
}
#endif

#endif
