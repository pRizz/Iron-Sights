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

#include <PR/ultratypes.h>
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

#ifdef __cplusplus
}
#endif

#endif
