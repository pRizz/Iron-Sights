/**
 * The ROM data converters of the stage classes (milestone LP64-004):
 * collision tiles and setup files. Their file
 * layouts are in port_romlayout_stage.h. port_romconv.h, which holds the
 * shared helpers (portConvBe32, portConvRefuse and the rest) and the other
 * classes' converters, is at the repository's file-length limit, so these
 * declarations have their own header; its rules are theirs: measure before
 * writing, never read what is being written, zero-extend every offset into
 * its pointer-sized slot, and refuse by name. The sources, port/src/romconv_*.c,
 * are compiled only into 64-bit builds and into the opt-in 32-bit oracle
 * build (-DGE_LP64_ORACLE=ON); every call from shared code sits inside
 * `#if IS_64_BIT` or `#ifdef GE_LP64_ORACLE`.
 */
#ifndef PORT_ROMCONV_STAGE_H
#define PORT_ROMCONV_STAGE_H

#include <stdint.h>
#include <PR/ultratypes.h>
#include "bondtypes.h" /* StandTile */
#include "port_romconv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- collision tiles (romconv_stan.c) --------------------------------- */

/**
 * The header the game reads at a stan file's start, two pointers: the same
 * definition as src/game/stan.c's (the converter and its test cannot include
 * stan.c). stan.c includes neither this header nor any that includes it; a
 * file that did would get a redefinition error, not a silent mismatch.
 */
struct StanPrefixRecord
{
    struct StanPrefixRecord *stanfile;
    StandTile *ptr_firstroom;
};

/**
 * Converts the stan file DATA (SIZE bytes, named NAME, which will be read at
 * the address BASE: DATA itself, or the original when DATA is the oracle's
 * copy) in place. Every check comes first: the header (first_tile ==
 * 8 + (rooms + 1) * 4, with any number of room slots, none included, before a
 * zero slot inside the file; the first tile inside the file), every room
 * offset inside the tiles, and the tile walk to its zero word, each tile and
 * its points inside the file. Then each tile is converted exactly as
 * romfileStanConvert does (the word rotated to GCC's id:24 room:8, the mid
 * and tail nibbles reversed, the points' halfwords to native order).
 *
 * At 32 bits the room slots then become BASE plus their offsets and, last, a
 * struct StanPrefixRecord {NULL, BASE + first_tile} is written over the two
 * header words: the in-place result. At 64 bits the header words and room
 * slots stay as the file has them (big-endian, unrebased): the 16-byte record
 * does not fit before a first tile at 0x0C, as in 18 of the 27 stock files,
 * so it lives outside the file (portStanRecordFill).
 *
 * Returns 1, or 0 after portConvRefuse with nothing written when the file is
 * shorter than its header, the room table has no zero slot, first_tile does
 * not follow the table or lies outside the file, a room offset is outside
 * [first_tile, SIZE), or the tile walk finds no zero word inside the file, a
 * tile header cut by its end, a point count above 10, or points past its
 * end.
 */
int portStanConvert(u8 *data, u32 size, uintptr_t base, const char *name);

/**
 * The 64-bit loader's record for the stan file DATA (SIZE bytes, read at
 * BASE): checks the file's header by portStanConvert's rule and bounds (the
 * same code) and fills *RECORD with {NULL, BASE + first_tile}. Reads only the
 * header and the room slots, which portStanConvert leaves as the file has
 * them at 64 bits, so it gives the same record before and after the
 * conversion (at 32 bits only before it: the conversion rewrites the header).
 * Writes nothing in DATA.
 *
 * Returns 1, or 0 after portConvRefuse, leaving *RECORD untouched.
 */
int portStanRecordFill(const u8 *data, u32 size, uintptr_t base, struct StanPrefixRecord *record, const char *name);

/* ---- setup files (romconv_setup.c, _walk.c, _records.c, _intro.c) ------ */

/**
 * The bytes of the run-time image of the big-endian setup file FILE (SIZE
 * bytes, port_romlayout_stage.h's RomSetup* layouts), a multiple of 8. Reads
 * only FILE.
 *
 * The image keeps the file's order, as romfile_setup.c's in-place pass does:
 * a struct stagesetup whose ten fields hold offsets from the image's start,
 * zero-extended (0 stays 0 for the two optional name tables; prop.c's
 * SETUP_PROMOTE adds the base), then every table, list, intro and prop list
 * where the file has it, each record at its C size (stagesetup, PadRecord,
 * BoundPadRecord, waypoint, waygroup, PathRecord, AIListRecord, pname; the s32
 * lists and the CreditsEntry table at their file sizes), each table aligned
 * for its record type, with the bytes no table owns (the names, the AI byte
 * code, padding) copied between them as they are. Inside the intro, which the
 * game walks by sizeof, a camera or credits record that would start at 4 mod
 * 8 at 64 bits is preceded by one word of type INTROTYPE_MAX, which every
 * intro walk steps over as an unknown 4-byte record. At 32 bits every item
 * keeps its size and place, so the image is romfile_setup.c's in-place result.
 *
 * Returns 0 after portConvRefuse, naming NAME, for: a file shorter than its
 * header; one of the first eight header offsets 0, and any header offset not
 * a multiple of 4 or not inside the file; a table, list, name, intro or credits
 * table with no terminator inside the file (each walk bounded by SIZE, in
 * 64-bit arithmetic); an intro or prop record of an unknown type; two items
 * sharing a byte (an s32 list may share words with another list); a
 * reference that names nothing of the kind it must: a table's start for a
 * header field, the words of an s32 list for a list field, the unowned bytes
 * for a name or an AI list; a name whose NUL, or an AI list whose AI_EndList
 * byte, is not in the unowned bytes it starts in; prop records the prop
 * converter refuses (portSetupPropsSize); an image that would pass 4 GB.
 */
u32 portSetupConvertedSize(const u8 *file, u32 size, const char *name);

/**
 * Writes the image portSetupConvertedSize measures for FILE into OUT (OUTSIZE
 * bytes, 8-byte aligned, not overlapping FILE): exactly that many bytes, the
 * padding zero. Every reference (a header field, plink, a waypoint's,
 * waygroup's or path's list, an AI list's code, a name table's entry, the
 * credits record's table) is the new offset of what it names, looked up
 * through the converter's PortConvMap; a reference into the unowned bytes
 * (a name or AI list may start at any byte, or inside another) is the new
 * offset of its 4-byte slot plus its distance into it, the bytes moving
 * together. Scalars are swapped as romfile_setup.c swaps them. The camera's
 * two text IDs are in the low 32 bits of their unions and its prev link is
 * zero-extended; the credits record holds, at 64 bits, its table's offset for
 * bondview_r.c to add the base to and, at 32 bits, the table's address in OUT
 * (OUT + offset), as the in-place pass writes it.
 *
 * Returns 1, or 0 after portConvRefuse with OUT unwritten, for every refusal
 * of portSetupConvertedSize, an OUTSIZE below the measured size, and an OUT
 * that is NULL, misaligned or overlapping FILE. OUT is written only after a
 * checking pass has run every writer, its backstops and the prop converter
 * (into scratch memory) without writing; the writing pass repeats that walk
 * on the same input, so it refuses nothing the checks accepted.
 */
s32 portSetupConvert(const u8 *file, u32 size, u8 *out, u32 outsize, const char *name);

/**
 * One prop record type's sizes (romconv_setup_props.c), from the record the
 * game reads that type through at run time: FILEWORDS, the file's record in
 * 4-byte words (setup_kinds.h's kSetupPropdefKinds); WIDEWORDS, the
 * converted record in 4-byte words (FILEWORDS at 32 bits; a multiple of 2 at
 * 64); and the NPOINTERS file words, in rising order in POINTERWORD, that are
 * pointer-sized members of that record. The converter places a record's
 * words in order, each pointer word at the next multiple of the pointer size
 * and pointer-sized (its value zero-extended), every other word in 4 bytes,
 * and pads the record to WIDEWORDS. sizepropdef (loadobjectmodel.c) returns
 * WIDEWORDS at 64 bits, so every walk of the converted list steps by it.
 */
struct PortSetupWide
{
    u16 filewords;
    u16 widewords;
    u8 npointers;
    u8 pointerword[16];
};

/** The sizes of prop record TYPE (one of the 49 PROPDEF_ values below PROPDEF_MAX), or NULL for another value. */
const struct PortSetupWide *portSetupWide(u32 type);

/**
 * The prop records' converter (romconv_setup_props.c). PROPS is the file's
 * prop list, SIZE bytes from its first record to the end of its PROPDEF_END
 * record. portSetupPropsSize returns the converted bytes, the sum of each
 * record's WIDEWORDS (portSetupWide), or 0 for a list it cannot convert: a
 * record of an unknown type, a record past SIZE, or bytes after the end
 * record. portSetupPropsConvert writes each record into OUT (OUTSIZE bytes,
 * 8-byte aligned at 64 bits) with its words swapped by their kinds
 * (kSetupPropdefKinds; word 0 is always the KSK header, as romfile_setup.c
 * swaps it) and placed by portSetupWide's rule, the padding zero, and
 * returns 1, or 0 with OUT unwritten for a list portSetupPropsSize refuses or
 * an OUTSIZE below its size. At 32 bits every record keeps its size and
 * place, so OUT is romfile_setup.c's in-place result. Neither logs: the setup
 * converter refuses by the file's name. portSetupConvert's promise that OUT
 * stays unwritten on a refusal rests on one rule, which any replacement
 * keeps: whether portSetupPropsConvert converts, and what it writes, depends
 * only on PROPS and SIZE, so that the checking pass (converting into scratch
 * memory) and the writing pass agree; every list portSetupPropsSize accepts
 * it converts in exactly that many bytes.
 */
u32 portSetupPropsSize(const u8 *props, u32 size);
s32 portSetupPropsConvert(const u8 *props, u32 size, u8 *out, u32 outsize);

/**
 * The setup file oracle's sweep (romconv_setup_oracle.c; the 32-bit oracle
 * build only, empty elsewhere): counts as differing every setup file the
 * ROM's directory holds that the game does not serve. Each served one is
 * compared by romfileSetupOracle when GE_ROMPROBE=1 loads it at boot.
 */
void portSetupOracleSweep(void);

#ifdef __cplusplus
}
#endif

#endif
