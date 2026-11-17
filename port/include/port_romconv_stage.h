/**
 * The ROM data converters of the stage classes (milestone LP64-004):
 * collision tiles now, setup files in the tasks that follow. Their file
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

#ifdef __cplusplus
}
#endif

#endif
