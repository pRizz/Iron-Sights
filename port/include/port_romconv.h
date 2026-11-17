/**
 * The ROM data converters (scripts/port/native_64bit_plan.md, "ROM data: file
 * layouts and converters"; milestone LP64-004).
 *
 * A converter reads a file in its ROM layout (port_romlayout.h) and writes
 * the run-time layout through the C types, so at 32 bits its output is what
 * the game's in-place conversion produces and at 64 bits it is the wider
 * layout. The sources, port/src/romconv_*.c, are compiled only into 64-bit
 * builds and into the opt-in 32-bit oracle build (-DGE_LP64_ORACLE=ON), never
 * into a shipped 32-bit build; every call from shared code sits inside
 * `#if IS_64_BIT` or `#ifdef GE_LP64_ORACLE`.
 *
 * The rules every converter keeps: measure before writing (a first pass
 * returns the output size and the caller checks it against its room); never
 * read what is being written (an in-place rewrite reads from a copy in
 * portConvScratch); zero-extend every offset into its pointer-sized slot; and
 * look every reference up through a PortConvMap, refusing by name on a miss.
 *
 * This header holds the helpers every converter shares; each converter task
 * adds its own prototypes below them.
 */
#ifndef PORT_ROMCONV_H
#define PORT_ROMCONV_H

#include <stdint.h>
#include <PR/ultratypes.h>
#include <PR/libaudio.h> /* the audio bank records */
#include "port_lp64.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==== shared helpers (romconv_common.c) ================================ */

/** The big-endian 16- and 32-bit values at P (ROM byte order). Pure. */
u16 portConvBe16(const u8 *p);
u32 portConvBe32(const u8 *p);

/**
 * A grow-only buffer for a converter's input copy. The returned BYTES bytes
 * are writable and valid until the next call (a larger request replaces the
 * buffer; its contents are not kept). Stops the game, named, when the memory
 * cannot be had.
 */
u8 *portConvScratch(u32 bytes);

/**
 * Grows the scratch buffer to at least BYTES now, and the offset map's store
 * to a file of BYTES, so that no later scratch request or map up to that size
 * allocates. romdataInit calls it once with 262,144: the largest stock model
 * file is 43,120 bytes and the largest setup file 103,424, so no model or
 * setup load during play allocates ("Load before play").
 */
void portConvScratchReserve(u32 bytes);

/**
 * Where each record of a file moved: one slot for each 4 bytes of the file,
 * holding the record's new offset. Converters look every reference up here,
 * so an offset the remap missed is a miss, never a stale address. The grain is
 * 4 bytes: a reference that can point at any byte is remapped as its owning
 * block's new offset plus the distance into it.
 */
struct PortConvMap
{
    u32 *newOffset; /* one per 4 bytes of the file */
    u8 *present;    /* 1 once that slot was put */
    u32 slots;
};

/**
 * Sets MAP up for a file of FILESIZE bytes, every slot empty, in the one store
 * all maps share. Returns 1, or 0 (MAP then empty and safe to free) when the
 * memory cannot be had, when FILESIZE's slots cannot be addressed (a file of
 * 0xFFFFFFFD bytes or more at 32 bits), or when another map is still live:
 * converters handle one file at a time, and that case logs
 * "ge: lp64: converter map: a second map while one is in use".
 */
int portConvMapInit(struct PortConvMap *map, u32 filesize);

/**
 * Records that the record at file offset OLD is now at offset TO. An OLD past the
 * file's end or not a multiple of 4 has no slot and is not recorded, so a
 * later portConvMapGet of it misses.
 */
void portConvMapPut(struct PortConvMap *map, u32 old, u32 to);

/**
 * Stores OLD's new offset in *TO and returns 1; returns 0, leaving *TO
 * untouched, when OLD was never put. An offset that is odd, or otherwise not
 * a multiple of 4, or past the file's end, is a miss.
 */
int portConvMapGet(const struct PortConvMap *map, u32 old, u32 *to);

/** Empties MAP and, if it is the live map, gives the store back for the next
 * one (the store is kept, not freed); safe on an empty map. */
void portConvMapFree(struct PortConvMap *map);

/**
 * Logs why a converter refuses a file:
 * ge: lp64: cannot convert CLASS "FILE": WHY
 * WHY is a printf format for the arguments that follow. The caller then takes
 * its existing failure path; lp64_boot.py fails a run that logs this line.
 */
void portConvRefuse(enum PortLp64Asset asset, const char *file, const char *why, ...)
    __attribute__((format(printf, 3, 4)));

/* ==== converters (one section a class, added by each converter task) ==== */

/* ---- animation tables (romconv_anim.c) -------------------------------- */

/**
 * Writes the run-time ModelAnimation of every header the animation tables
 * list. TABLES holds NTABLES lists of TABLESIZE-relative offsets, each ended
 * by 0, in which 1 is an empty entry. For each other entry it reads the
 * native-order RomAnimHeader at STAGED + offset and writes, at TABLE + offset,
 * address = ENTRIES + entries_offset, bitDescriptors = STAGED +
 * descriptors_offset and bitStream = STAGED + stream_offset (each offset
 * zero-extended), then replaces the entry with that header's address. A header
 * listed twice is written once.
 *
 * At 64 bits ModelAnimation is packed and 4-byte aligned, so a header takes
 * 0x20 bytes of TABLE and overwrites the 0xC bytes after its file header:
 * the pointers lead into STAGED, which keeps the file layout. STAGED (of
 * STAGEDSIZE bytes) is never written.
 *
 * Returns 1, or 0 after portConvRefuse with nothing written when an offset or
 * offset + 0x20 is outside TABLE, the file header is outside STAGED, a
 * header's descriptor or stream offset is at or past STAGEDSIZE, or two
 * distinct headers are less than 0x20 apart.
 *
 * Precondition: ENTRIES + entries_offset, and the frames read from it, lie in
 * the entries image. The converter has no entries size and does not check it;
 * nor does any code today: portAnimDataInit (animdata.c) checks that the
 * animation_entries segment is its full 0x169EC0 bytes but not the offsets
 * into it, and at 32 bits expand_ani_table_entries adds them unchecked too.
 */
int portAnimHeadersConvert(u8 *table, u32 tablesize, const u8 *staged, u32 stagedsize, const u8 *entries,
        intptr_t *const *tables, int ntables);

#ifdef GE_LP64_ORACLE
/**
 * The oracle (32-bit, -DGE_LP64_ORACLE=ON), called after the game's
 * expand_ani_table_entries has relocated INPLACE (the permanent copy, of
 * TABLESIZE bytes) and the NTABLES TABLES: converts a copy of STAGED with
 * copies of the tables and compares each header's 0x14 bytes, one "file" a
 * header, and each table entry. Pointers into a table are compared as offsets
 * from it (the in-place ones lead into INPLACE, the converter's into STAGED
 * and the copy).
 */
void portAnimHeadersOracle(const u8 *inplace, u32 tablesize, const u8 *staged, const u8 *entries,
        intptr_t *const *tables, int ntables);
#endif

/** The staging images portAnimDataInit fills (animdata.c), read through their
 * own declarations rather than the game's externs. */
const u8 *portAnimStagedData(void);
const u8 *portAnimStagedEntries(void);

/* ---- text banks (romconv_text.c) -------------------------------------- */

/** A port relabel: slot SLOT of the bank named BANK reads TEXT (romdata.c's
 * sTextPatches). Declared at both widths, since the 32-bit table uses it. */
struct PortTextPatch
{
    const char *bank;
    u32 slot;
    const char *text;
};

/**
 * Converts the text bank DATA (SIZE bytes, named BANK) in place. The file is a
 * table of big-endian, bank-relative offsets followed by the strings; the
 * table's size is the smallest non-zero offset, which must be a multiple of 4
 * and at most SIZE. A slot of 0 stays 0, and a slot whose offset is at or past
 * SIZE becomes 0 with a log line.
 *
 * At 64 bits each slot becomes the offset in native byte order (langGet adds
 * the bank's address), and each of the NPATCHES PATCHES that names BANK and a
 * slot of the table is copied, with its terminator, over the string that slot
 * names. At 32 bits each slot becomes (u32) (BASE + offset) and a patched slot
 * the patch's address, the in-place result; the strings are not written.
 *
 * Returns the slot count; 0, writing nothing, for an empty bank (every byte
 * zero); or -1 after portConvRefuse, with nothing written, when no offset
 * table is found, or when a patched slot names no string, a string with no
 * terminator inside the bank, a string shorter than the patch, or bytes that
 * another slot's string also covers. The patch checks apply at both widths, so
 * the 32-bit oracle checks the stock relabels against the 64-bit rule.
 */
s32 portTextBankConvert(u8 *data, u32 size, uintptr_t base, const char *bank, const struct PortTextPatch *patches,
        u32 npatches);

/* ---- audio banks (romconv_bank.c) ------------------------------------- */

/**
 * The bytes of run-time records portBankConvert writes for the big-endian bank
 * file FILE (SIZE bytes, port_romlayout.h's RomBankFile): one ALBankFile and
 * one record for each distinct ALBank, ALInstrument, ALSound and ALWaveTable
 * the file reaches, each rounded up to 8. Reads only. Returns 0 after
 * portConvRefuse when the revision is not AL_BANK_VERSION, a count is negative,
 * a record or array runs past the file's end, an offset is not a multiple of
 * 4, or one offset is reached as two kinds of record.
 */
u32 portBankConvertedSize(const u8 *file, u32 size);

/**
 * Builds the run-time graph of the bank file FILE (SIZE bytes) in OUT (OUTSIZE
 * bytes, 8-byte aligned) and returns OUT as its ALBankFile. Each distinct
 * bank, instrument, sound and wave table is written once, its flags byte 1;
 * envelopes, key maps, ADPCM books and loops stay in FILE and are turned to
 * native byte order there, once each, as the 32-bit alBnkfNew does. A wave's
 * base is TABLE + its offset, and a 16-bit raw wave's samples are swapped in
 * TABLE, once a base. An instrument or bank offset of 0 gives a pointer to
 * FILE itself and is not followed, as alBnkfNew does.
 *
 * Returns NULL after portConvRefuse, having written nothing to FILE, TABLE or
 * OUT, when portBankConvertedSize refuses, when OUTSIZE is less than it, or
 * when OUT is not 8-byte aligned. The file's flags bytes are not read: a
 * record already marked converted is converted again. TABLE's size is not
 * known here and not checked, as at 32 bits.
 */
ALBankFile *portBankConvert(u8 *file, u32 size, u8 *table, u8 *out, u32 outsize);

/** audio_stubs.c, at 64 bits: converts the copied bank file FILE (SIZE bytes)
 * into records allocated from HEAP and returns them; stops the game, naming
 * NAME, when the file is refused. */
ALBankFile *portBnkfConvert(ALHeap *heap, ALBankFile *file, u32 size, u8 *table, const char *name);

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

#ifdef __cplusplus
}
#endif

#endif
