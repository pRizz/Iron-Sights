/**
 * romfile_stan.c - the stan (collision tile) preprocess pass for ROM loading.
 *
 * A stan file (Tbg_<lvl>_all_p_stanZ) is laid out as:
 *   [u32 unk1][u32 firstTile offset][u32 room offsets ... 0][tiles back to back]
 * Each tile is an 8-byte header {u32 id:24 room:8; u16 mid; u16 tail} followed
 * by pointCount x {s16 x,y,z; u16 link} (list_of_tilesizes[pointCount] bytes).
 *
 * stan.c's portStanPack builds exactly this packed shape from the compiled-in
 * stans, with real pointers in the header. The ROM file already has that shape,
 * so this pass converts it in place and the game uses it directly:
 *  - header: unk1 = 0, and firstTile plus every room slot become base + offset;
 *  - tile word 0: N64 packs id high and room low, GCC packs the bitfields
 *    LSB-first, so the host word is the N64 word rotated right by 8;
 *  - mid / tail: four 4-bit fields each, so the host halfword is the N64
 *    halfword with its nibbles reversed (stan.h's STAN_PTCOUNT / STAN_MID_NIB match);
 *  - points: s16 x y z and u16 link are byte-swapped.
 *
 * GE_ROMDATA_FALLBACK builds compare the converted image byte-for-byte against
 * portStanPack's output (pointer words compared as offsets).
 */
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "port_lp64.h" /* LP64-003: portLp64RequireConverter */
#if IS_64_BIT || defined(GE_LP64_ORACLE)
#include <stdint.h>
#include <stdlib.h>
#include "port_lp64_oracle.h"
#include "port_romconv_stage.h" /* LP64-004: portStanConvert */
#endif

/* port_console.c: same output as the fprintf(stderr, ...) this replaced,
 * plus a silent copy in the tilde console ring (spec section 7). */
extern void portConLogf(const char *fmt, ...);

extern u8 list_of_tilesizes[];                                  /* stan.c: bytes per pointCount */
extern struct StanPrefixRecord *portStanPackForProbe(struct StanPrefixRecord *file); /* stan.c */

#if IS_64_BIT
extern void sysFatalError(const char *format, ...);

/* LP64-004: the record the game reads is two 8-byte pointers, which do not
 * fit before a first tile at 0x0C (18 of the 27 stock files). The converter
 * checks the whole file and converts the tiles, leaving the header as the
 * file has it; a refused file stops the game by name (its bytes are
 * untouched). No compiled-in stan is linked at 64 bits, so COMPILED is NULL;
 * load_bg_file refuses a stan the cart does not serve before portStanPack. */
s32 romfileStanConvert(u8 *data, u32 size, const char *name, const u8 *compiled)
{
    (void) compiled;
    if (!portStanConvert(data, size, (uintptr_t) data, name)) {
        sysFatalError("Cannot convert the collision file %s", name);
    }
    /* portStanConvert logged the line the in-place pass prints, with its counts */
    return 1;
}

/* The record of the one stan the game has loaded: load_bg_file is the only
 * caller of stanDetermineEOF and stanLoadFile, and stan.c reaches the file
 * only through the global stan_prefix, which they set to this record. */
static struct StanPrefixRecord s_StanRecord;

/* LP64-004: fills the record for the stan file DATA (SIZE bytes, named NAME)
 * that load_bg_file has just loaded, and returns it in place of the file's
 * address; a refused header stops the game by name. */
struct StanPrefixRecord *romfileStanRecord(u8 *data, u32 size, const char *name)
{
    if (!portStanRecordFill(data, size, (uintptr_t) data, &s_StanRecord, name)) {
        sysFatalError("Cannot read the header of the collision file %s", name);
    }
    return &s_StanRecord;
}
#else
static u32 be32(const u8 *p)
{
    return ((u32) p[0] << 24) | ((u32) p[1] << 16) | ((u32) p[2] << 8) | (u32) p[3];
}

static void put32(u8 *p, u32 v)
{
    memcpy(p, &v, 4);
}

static void swap16(u8 *p)
{
    u8 t = p[0];
    p[0] = p[1];
    p[1] = t;
}

/* N64 halfword (BE bytes) with nibbles reversed -> host u16 */
static void nibbleReverse(u8 *p)
{
    u16 v = (u16) (((u16) p[0] << 8) | p[1]);
    u16 r = (u16) (((v & 0xF) << 12) | ((v & 0xF0) << 4) | ((v >> 4) & 0xF0) | (v >> 12));
    memcpy(p, &r, 2);
}

s32 romfileStanConvert(u8 *data, u32 size, const char *name, const u8 *compiled)
{
    u32 first, o, nrooms, ntiles = 0;
    u32 i;

    portLp64RequireConverter(PORT_LP64_COLLISION); /* 4-byte room pointer slots */
    if (size < 16) {
        portConLogf("ge: romdata: stan %s: %u bytes is no stan file\n", name, size);
        return 0;
    }
    first = be32(data + 4);
    for (nrooms = 0; 8 + nrooms * 4 < size && be32(data + 8 + nrooms * 4) != 0; nrooms++) {
    }
    if (first != 8 + (nrooms + 1) * 4 || first >= size) {
        portConLogf("ge: romdata: stan %s: first tile at 0x%X but the room table ends at 0x%X (%u rooms) - not a stan?\n",
                name, first, 8 + (nrooms + 1) * 4, nrooms);
        return 0;
    }
    put32(data, 0);
    put32(data + 4, (u32) (uintptr_t) (data + first));
    for (i = 0; i < nrooms; i++) {
        u32 off = be32(data + 8 + i * 4);
        if (off < first || off >= size) {
            portConLogf("ge: romdata: stan %s: room %u tile offset 0x%X outside the tiles (0x%X..0x%X)\n", name, i, off, first, size);
            return 0;
        }
        put32(data + 8 + i * 4, (u32) (uintptr_t) (data + off));
    }
    /* the zero terminator after the rooms stays zero */

    o = first;
    while (o + 8 <= size) {
        u32 w0 = be32(data + o);
        u32 count, tsize, p;
        if (w0 == 0) {
            break; /* the tile walk's end sentinel (stanBuildRoomData: while (*(u32 *)tile)) */
        }
        count = ((u32) data[o + 6] >> 4) & 0xF; /* N64 tail: pointCount is the top nibble */
        tsize = (count <= 10) ? list_of_tilesizes[count] : 0;
        if (tsize == 0 || o + tsize > size) {
            portConLogf("ge: romdata: stan %s: tile %u at 0x%X has %u points (size %u) - walk stopped\n", name, ntiles, o, count, tsize);
            return 0;
        }
        put32(data + o, (w0 >> 8) | (w0 << 24)); /* id:24 | room:8 -> GCC LSB-first unit */
        nibbleReverse(data + o + 4);
        nibbleReverse(data + o + 6);
        for (p = 0; p < count; p++) {
            u8 *pt = data + o + 8 + p * 8;
            swap16(pt);
            swap16(pt + 2);
            swap16(pt + 4);
            swap16(pt + 6);
        }
        o += tsize;
        ntiles++;
    }

    if (compiled != NULL) {
        const u8 *packed = (const u8 *) portStanPackForProbe((struct StanPrefixRecord *) compiled);
        u32 m = 0, hdrbad = 0;
        if (packed != NULL) {
            /* header: compare pointer words as offsets from each image's base */
            for (i = 1; i < 2 + nrooms; i++) {
                u32 a = *(const u32 *) (packed + i * 4) - (u32) (uintptr_t) packed;
                u32 b = *(const u32 *) (data + i * 4) - (u32) (uintptr_t) data;
                if (a != b) hdrbad++;
            }
            for (i = first; i < o; i++) {
                if (packed[i] != data[i]) {
                    if (m < 4) { /* name the first few: which tile, which byte */
                        u32 t = first, ti = 0;
                        while (t + 8 <= o) {
                            u32 cnt = data[t + 6] & 0xF; /* host tail: pointCount low nibble */
                            u32 sz = (cnt <= 10) ? list_of_tilesizes[cnt] : 8;
                            if (i < t + sz) break;
                            t += sz;
                            ti++;
                        }
                        portConLogf("ge: romdata: stan  %s: diff at 0x%X = tile %u +%u: packed %02X rom-derived %02X\n",
                                name, i, ti, i - t, packed[i], data[i]);
                    }
                    m++;
                }
            }
        }
        if (packed != NULL && m == 0 && hdrbad == 0) {
            portConLogf("ge: romdata: stan  %-18s identical to the packed compiled-in stan (%u tiles, %u rooms, %u bytes)\n", name, ntiles, nrooms, o);
        } else {
            portConLogf("ge: romdata: stan  %-18s DIFFERS from the packed compiled-in stan: %u header slots, %u tile bytes (a mod - expected - or a converter bug)\n",
                    name, hdrbad, m);
        }
    } else {
        portConLogf("ge: romdata: stan  %-18s converted (%u tiles, %u rooms, %u bytes)\n", name, ntiles, nrooms, o);
    }
    return 1;
}

#ifdef GE_LP64_ORACLE
/* LP64-004 oracle (32-bit, -DGE_LP64_ORACLE=ON): romdataFilePreprocess calls
 * this in place of romfileStanConvert. It converts a copy of the file with
 * portStanConvert, read at DATA's own address, converts DATA in place as
 * always, and compares the whole file byte for byte. */
s32 romfileStanOracle(u8 *data, u32 size, const char *name, const u8 *compiled)
{
    u8 *copy = (u8 *) malloc(size);
    s32 inplace;

    if (copy == NULL) {
        portLp64OracleResult(PORT_LP64_COLLISION, name, 0, "the oracle could not copy the file");
        return romfileStanConvert(data, size, name, compiled);
    }
    memcpy(copy, data, size);
    inplace = romfileStanConvert(data, size, name, compiled);
    if (!portStanConvert(copy, size, (uintptr_t) data, name)) {
        portLp64OracleResult(PORT_LP64_COLLISION, name, 0, "the converter refused the file");
    } else if (!inplace) {
        portLp64OracleResult(PORT_LP64_COLLISION, name, 0, "the in-place pass refused the file");
    } else {
        portLp64OracleBytes(PORT_LP64_COLLISION, name, data, copy, size);
    }
    free(copy);
    return inplace;
}
#endif
#endif
