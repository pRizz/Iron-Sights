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
#include <PR/gbi.h>      /* Gfx, for the display-list expanders */
#include "port_lp64.h"
#include "music.h" /* RareALSeqBankFile */

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
 * file FILE (SIZE bytes, port_romlayout.h's RomBankFile) whose waves lead into
 * a wave table of TABLESIZE bytes: one ALBankFile and one record for each
 * distinct ALBank, ALInstrument, ALSound and ALWaveTable the file reaches, each
 * rounded up to 8. Reads only. Returns 0 after portConvRefuse, naming NAME (the
 * bank, as the caller knows it), when the revision is not AL_BANK_VERSION, a
 * count is negative, a record or array runs past the file's end, an offset is
 * not a multiple of 4, one offset is reached as two kinds of record, two
 * records reached share a byte (an instrument's or bank's offset array is part
 * of its record), or an ADPCM or raw wave's base + len passes TABLESIZE.
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
 * FILE (port_romlayout.h's RomSeqTable): offsetof(RareALSeqBankFile, seqArray)
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
 * or OUTSIZE is less than portSeqTableSize. Each entry's offset and lengths
 * are copied as they are: nothing here bounds offset + len by FILESIZE, since
 * the entries' streams are read later by musicTrack1Play and its two twins.
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

/* ---- display lists: the expander of the textures class (romconv_gfx.c) -- */

/*
 * A ROM display-list command is 8 bytes, two big-endian 32-bit words; a host
 * Gfx is two pointer-sized words, 8 bytes at 32 bits and 16 at 64. The three
 * expanders below turn COMMANDS commands into COMMANDS Gfx, each word
 * zero-extended (an unsigned 32-bit value, never a signed one), so a
 * command's upper halves are zero at 64 bits and a segment word stays a
 * segment word.
 *
 * Each reads exactly 8 * COMMANDS bytes from its source and writes exactly
 * COMMANDS * sizeof(Gfx) bytes at DST, nothing before or after; COMMANDS 0
 * reads and writes nothing (either pointer may then be NULL). The functions
 * take no size for DST: the caller guarantees COMMANDS * sizeof(Gfx) bytes
 * there, which at 64 bits is twice the source's size. Each command's two
 * words are read before either is written, so a command may overlap its own
 * destination.
 *
 * Overlap. With G = sizeof(Gfx) - 8 (0 at 32 bits, 8 at 64), the source and
 * DST may share a buffer only as follows; any other overlap corrupts commands
 * not yet read:
 *   portGfxFromRom, portGfxFromWords (first command to last): DST at or
 *     before SRC - G * (COMMANDS - 1). At 32 bits, DST == SRC (in place).
 *     At 64 bits the tightest is DST 8 * (COMMANDS - 1) bytes before SRC;
 *     a source at the end of room for COMMANDS Gfx, with DST at that room's
 *     start, is 8 * COMMANDS bytes before and so legal.
 *   portGfxFromRomBackward (last command to first): DST at or after SRC - G.
 *     At 64 bits the tightest is DST 8 bytes before SRC; DST == SRC, or DST
 *     later in the same buffer (texCopyGdls's room lists), is legal.
 * Buffers that do not overlap are legal for all three.
 */

/** Expands COMMANDS big-endian ROM commands at SRC into host Gfx at DST,
 * first to last. Overlap: see above. */
void portGfxFromRom(Gfx *dst, const u8 *src, u32 commands);

/** As portGfxFromRom, last command to first, for a DST at or after SRC - G
 * (see above). */
void portGfxFromRomBackward(Gfx *dst, const u8 *src, u32 commands);

/** As portGfxFromRom for commands already in native 4-byte words: WORDS[2i]
 * and WORDS[2i + 1] are command i's w0 and w1. Overlap: as portGfxFromRom. */
void portGfxFromWords(Gfx *dst, const u32 *words, u32 commands);

#ifdef GE_LP64_ORACLE
/**
 * The oracle (32-bit, -DGE_LP64_ORACLE=ON) around texCopyGdls's 32-bit copy
 * of BYTES bytes of ROM commands from SRC to DST: before it, copies the span
 * both cover when they overlap (else the source alone) and runs
 * portGfxFromRomBackward in the copy with the same offsets; after it, compares DST's BYTES bytes with the converter's, as the
 * textures file "room list N" (N counts the calls). Only one copy is checked
 * at a time: Before then After, with nothing between.
 */
void portGfxCopyOracleBefore(const Gfx *src, const Gfx *dst, s32 bytes);
void portGfxCopyOracleAfter(void);
#endif

/* ---- fonts (romconv_font.c) ------------------------------------------- */

/** The commands of the font list (the fontdl segment, 0xC0 bytes in the ROM):
 * 0xC0 bytes of Gfx at 32 bits, 0x180 at 64. */
#define PORT_FONTDL_COMMANDS 24u

/**
 * The bytes of the run-time image of a font segment of ROMSIZE bytes
 * (port_romlayout.h's RomFont, then glyph bytes): ROMSIZE - sizeof(RomFont) +
 * sizeof(struct font), which is ROMSIZE at 32 bits and ROMSIZE + 0x2F4 at 64
 * (0x24B0 + 0x2F4 for Bank Gothic, 0x3540 + 0x2F4 for Zurich Bold). Returns 0
 * when ROMSIZE is less than sizeof(RomFont) or the size would pass
 * 0xFFFFFFFF. Pure.
 */
u32 portFontConvertedSize(u32 romsize);

/**
 * The span segments.c stages a font image of CONVERTED bytes in, which
 * load_font_tables allocates from the stage bank: CONVERTED rounded up to 16.
 * The bank does not round, so this keeps the next allocation as aligned as at
 * 32 bits, where both stock sizes are multiples of 16: 0x24B0 + 0x300 and
 * 0x3540 + 0x300 at 64 bits. CONVERTED is at most 0xFFFFFFF0 here.
 */
#define PORT_FONT_STAGED_SIZE(converted) (((converted) + 15u) & ~15u)

/**
 * Writes the run-time image of the big-endian font segment ROM (ROMSIZE
 * bytes) into OUT (OUTSIZE bytes; any alignment, and not overlapping ROM): a
 * struct font with native kerning and character fields, each pixeldata the
 * character's pixel_offset - sizeof(RomFont) + sizeof(struct font)
 * zero-extended (0 stays 0), its padding zero, then the glyph bytes as they
 * are. Nothing past portFontConvertedSize(ROMSIZE) bytes is written. The
 * game's own fix-up in load_font_tables then adds the copy's address.
 *
 * Returns 1, or 0 after portConvRefuse with nothing written when ROMSIZE is
 * less than sizeof(RomFont), portFontConvertedSize(ROMSIZE) would wrap,
 * OUTSIZE is less than it, or a non-zero pixel_offset lies outside the glyph
 * bytes (below sizeof(RomFont) or at or past ROMSIZE). Only where a glyph
 * starts is checked: its width and height are copied as they are, and
 * nothing here bounds the bytes the text renderer reads from pixeldata.
 */
int portFontConvert(const u8 *rom, u32 romsize, u8 *out, u32 outsize);

#ifdef GE_LP64_ORACLE
/**
 * The oracle (32-bit, -DGE_LP64_ORACLE=ON), called after portFontBuildSegments
 * staged the font segment NAME: converts ROM (ROMSIZE bytes, the segment the
 * staging image was built from) into new memory and compares its
 * portFontConvertedSize bytes with STAGED, as the fonts file NAME.
 */
void portFontOracle(const char *name, const u8 *staged, const u8 *rom, u32 romsize);

/** As portFontOracle for the font list: expands its PORT_FONTDL_COMMANDS
 * commands at ROM with portGfxFromRom and compares them with STAGED, as the
 * fonts file "fontdl". */
void portFontListOracle(const u8 *staged, const u8 *rom);
#endif

/* ---- models (romconv_model.c, _walk.c, _refs.c, _records.c) ------------ */

/**
 * The bytes of the run-time image of the big-endian model file FILE (SIZE
 * bytes, port_romlayout.h's RomModel* layouts) with NUMSWITCHES switch slots
 * and NUMTEXTURES texture entries (the compiled ModelFileHeader's counts), a
 * multiple of 8. Reads only FILE.
 *
 * The image keeps the file's order: the switch slots (sizeof(ModelNode *)
 * each), the texture entries (sizeof(ModelFileTextures) each), the root node
 * and then every other item where the file has it, each node, record, block
 * and list at its run-time size and aligned to a pointer, with the bytes no
 * item owns copied between them as they are. At 32 bits every item keeps its
 * size and place, so the image is romfile_model.c's in-place result.
 *
 * Returns 0 after portConvRefuse, naming NAME, for: a file too short for its
 * counts and the root node; a node below the root; a node, record, block or
 * list that runs past the file (each in 64-bit arithmetic); a list with no
 * end command within 32,768 commands; more than 0x2000 quads in a DLPRIMARY;
 * an inline texture's texel depth above 3; a file with no vertices and no
 * lists (romfile_model.c refuses each of these). Beyond it: an item not at a
 * multiple of 4; two items sharing a byte, or one place reached as two kinds;
 * a reference neither 0 nor a segment-5 offset, or naming nothing of the kind
 * it must (G_VTX words must load inside one vertex block or unclaimed bytes);
 * a node whose opcode has a record and no data, or data and no record; more
 * than 4,096 nodes pending in the walk; a list that
 * calls itself, or calls nested more than 32 deep; node links the game's
 * promotion would walk forever; a collision vertex linked to anything but a
 * node, or to the node at new offset 0; children in an opcode 5 or 7 record;
 * a block counted above 32,768 (romfile_model.c leaves one unswapped); a
 * segment-5 word in a list command that carries an address the converter
 * does not move (below); an item that would end past 16 MB in the image,
 * where 0x05000000 | offset would run into the segment byte.
 */
u32 portModelConvertedSize(const u8 *file, u32 size, s32 numSwitches, s32 numtextures, const char *name);

/**
 * Writes the image portModelConvertedSize measures for FILE into OUT (OUTSIZE
 * bytes, 8-byte aligned, not overlapping FILE): exactly that many bytes, the
 * padding zero. Each reference to a node, record, texture entry, block or list
 * is 0x05000000 plus the new offset, zero-extended into its field (0 stays 0),
 * for the game's promotion; BaseAddr fields are copied as the file has them.
 * Scalars are swapped into their C fields by romfile_model.c's field maps.
 * Lists are expanded by portGfxFromRom. In GE's F3D, w1 is an address in
 * G_VTX (0x04), G_SETTIMG (0xFD), the list calls (0x06, and F3DEX2's 0xDE),
 * whose segment-5 words are remapped, and in G_MTX (0x01), G_MOVEMEM (0x03),
 * G_SPRITE2D_BASE (0x09), G_INVALTEXCACHE_EXT (0x34), G_SETZIMG (0xFE) and
 * G_SETCIMG (0xFF), whose segment-5 words refuse the file (no stock list has
 * one). Every other command's w1 is not an address (G_TRI4's vertex indices,
 * G_SETTILE's and G_SETTILESIZE's settings, colours, masks) and is copied,
 * as are segment 3 and 4 words. At 64 bits a collision
 * vertex's link is its node's new offset and its related index is swapped; at
 * 32 bits both stay as the in-place path leaves them.
 *
 * Returns 1, or 0 after portConvRefuse with nothing written when
 * portModelConvertedSize refuses, OUTSIZE is less than it, OUT is not 8-byte
 * aligned or OUT overlaps FILE.
 */
s32 portModelConvert(const u8 *file, u32 size, u8 *out, u32 outsize, s32 numSwitches, s32 numtextures,
        const char *name);

#ifdef GE_LP64_ORACLE
/**
 * The oracle (32-bit, -DGE_LP64_ORACLE=ON; romconv_model_oracle.c): converts
 * the model file FILE (SIZE bytes, never written) by romfile_model.c's
 * in-place path in a copy and by portModelConvert into new memory, and
 * compares the whole images, both before the game's promotion, as the models
 * file NAME. A refusal by either path, or an image of another size, is a
 * difference.
 */
void portModelOracle(const u8 *file, u32 size, s32 numSwitches, s32 numtextures, const char *name);

/**
 * Runs portModelOracle for every model file the compiled header tables name
 * (gun, chr and prop), each loaded through the game's loader with its lookup
 * entry put back, then logs
 * ge: lp64: oracle: models sweep: the header tables name N files, M swept
 * romdataFilesApply calls it when the oracle's sweeps are asked for.
 */
void portModelOracleSweep(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
