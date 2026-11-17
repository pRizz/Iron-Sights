/**
 * romfile_bg.c - the bg (level geometry) preprocess pass.
 *
 * A bg file arrives from the cart as the N64 memory image: an uncompressed
 * big-endian front matter (bg_header, room_data_table,
 * global_visibility_commands, portal_data_table, portal point structs) with
 * segment-0x0F offsets, followed by per-room geometry as 1172-compressed blobs.
 * The bg file is used in place (obGetBGFileData); BG_SEG_TO_PTR resolves the
 * offsets and the existing room-streaming path handles the compressed geometry,
 * so this pass only byteswaps the front matter to native and leaves offsets as
 * offsets.
 *
 * Field widths (verified against the cart and the compiled bg object, docs/port/romdata.md 12b):
 *   bg_header            5 x W4 (RESERVED, room*, portal*, vis*, PADDING)
 *   room_data_table      6 x W4 per entry (3 offsets + f32 x/y/z)
 *   global vis commands {u8 type; u8 length; u8 pad[2]; s32 arg}: word0 is
 *   bytes (no swap), only the s32 arg swaps
 *   portal_data_table offset_portal W4; room1/room2 bytes; control_bytes is the
 *   cart's [cb1][cb2] pair, already in runtime order
 *   portal_N_point       u8 numpoints + 3 pad, then N x coord3d (3 x f32 = W4)
 *
 * The compiled bg needs load-time fixups (portSwapGlobalVisCommands /
 * portSwapPortalControlBytes) because its data was baked as native-endian
 * literals; for a served bg bg.c skips them (romdataFileServed) and this pass
 * replaces them. It converts in place in the writable ROM arena, once per file.
 */
#include <stdio.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "port_lp64.h" /* LP64-003: portLp64RequireConverter */
#if IS_64_BIT || defined(GE_LP64_ORACLE)
#include <stdint.h>
#include "bg.h"
#include "memp.h"
#include "port_romconv_stage.h" /* LP64-004: the background converter and its oracle */
#endif

/* port_console.c: same output as the fprintf(stderr, ...) this replaced,
 * plus a silent copy in the tilde console ring (spec section 7). */
extern void portConLogf(const char *fmt, ...);

static u32 bg_be32(const u8 *p)
{
    return ((u32) p[0] << 24) | ((u32) p[1] << 16) | ((u32) p[2] << 8) | (u32) p[3];
}

static void bg_swap4(u8 *p)
{
    u8 t = p[0]; p[0] = p[3]; p[3] = t;
    t = p[1]; p[1] = p[2]; p[2] = t;
}

static void bg_swap4run(u8 *p, u32 words)
{
    while (words--) {
        bg_swap4(p);
        p += 4;
    }
}

#define BG_MAX_CONVERTED 64
static const u8 *bg_converted[BG_MAX_CONVERTED];
static s32 bg_convertedCount;

/* Convert a served bg file's front matter to host layout, in place. Returns 1
 * (idempotent - a second call on the same file is a no-op). */
s32 romfileBgConvert(u8 *data, u32 size, const char *name)
{
    u32 roomOff, portalTableOff, visOff, o;
    u32 binStart, portalRegion;
    s32 i;
    u32 rooms = 0, vis = 0, portals = 0;

    portLp64RequireConverter(PORT_LP64_BACKGROUNDS); /* 4-byte offset words in the front matter */
    if (data == NULL || size < 0x14) {
        return 0;
    }
    for (i = 0; i < bg_convertedCount; i++) {
        if (bg_converted[i] == data) {
#ifdef GE_LP64_ORACLE
            portBgTablesReloadOracle(data, name ? name : "?"); /* LP64-004: what this load sees */
#endif
            return 1; /* already converted (stage reload) */
        }
    }

    /* header offsets are still big-endian here; strip the 0x0F segment tag */
    roomOff = bg_be32(data + 4) & 0x00FFFFFFu;
    portalTableOff = bg_be32(data + 8) & 0x00FFFFFFu;
    visOff = bg_be32(data + 0xc) & 0x00FFFFFFu;

    /* the room binaries begin right after the portal region; find their start
     * from the smallest room-table binary offset (read BE, before the swap) so
     * the portal walk below knows where to stop */
    binStart = size;
    if (roomOff <= visOff && visOff <= size) {
        for (o = roomOff; o + 24 <= visOff; o += 24) {
            u32 k;
            for (k = 0; k < 3; k++) {          /* point / pri / sec binary offsets */
                u32 b = bg_be32(data + o + k * 4) & 0x00FFFFFFu;
                if (b != 0 && b < binStart) {
                    binStart = b;
                }
            }
        }
    }

    /* bg_header: 5 x W4 */
    bg_swap4run(data, 5);

    /* room_data_table [roomOff, visOff): every field is W4 (3 offsets + f32
     * x/y/z), so the whole span swaps as W4 words */
    if (roomOff <= visOff && visOff <= size) {
        bg_swap4run(data + roomOff, (visOff - roomOff) / 4u);
        rooms = (visOff - roomOff) / 24u;
    }

    /* global_visibility_commands [visOff, portalTableOff): 8-byte commands,
     * word 0 is the {type,length,pad,pad} byte image (no swap), word 1 is the
     * s32 arg (W4). Trailing padding swaps harmlessly (zero). */
    if (visOff <= portalTableOff && portalTableOff <= size) {
        for (o = visOff; o + 8 <= portalTableOff; o += 8) {
            bg_swap4(data + o + 4);
            vis++;
        }
    }

    /* portal_data_table [portalTableOff, portalRegion): 8-byte entries
     * {offset_portal W4; u8 room1; u8 room2; u8 cb1; u8 cb2} until the NULL
     * offset_portal terminator. Only offset_portal swaps; the room and control
     * bytes keep the cart order. */
    portalRegion = size;
    if (portalTableOff <= size) {
        for (o = portalTableOff; o + 8 <= size; o += 8) {
            if (bg_be32(data + o) == 0) {
                portalRegion = o + 8; /* portals begin after the NULL entry */
                break;
            }
            bg_swap4(data + o);                    /* offset_portal */
        }
    }

    /* Portal point structs are laid out consecutively as {u8 numpoints; u8
     * pad[3]; numpoints x coord3d}, so walk them linearly and swap the floats.
     * This covers every portal exactly once, including ones shared by several
     * table entries and orphans no entry references, where a reference-driven
     * swap would double-swap or miss them. */
    if (portalRegion < binStart && binStart <= size) {
        o = portalRegion;
        while (o + 4 <= binStart) {
            u32 n = data[o];
            u32 span;
            if (n == 0) {
                o += 4; /* zero padding between the table and the portals, or
                         * between portals (cradle) - skip and keep scanning */
                continue;
            }
            if (n > 32) {
                break; /* not a portal header - misalignment guard */
            }
            span = 4u + n * 12u;
            if (o + span > binStart) {
                break;
            }
            bg_swap4run(data + o + 4, n * 3u);
            o += span;
            portals++;
        }
    }

    if (bg_convertedCount < BG_MAX_CONVERTED) {
        bg_converted[bg_convertedCount++] = data;
    }
    portConLogf("ge: romdata: bg %-20s converted (%u rooms, %u vis cmds, %u portals)\n",
            name ? name : "?", rooms, vis, portals);
#ifdef GE_LP64_ORACLE
    portBgTablesOracle(data, size, name ? name : "?"); /* LP64-004: before load_bg_file's passes */
#endif
    return 1;
}

#if IS_64_BIT
extern void sysFatalError(const char *format, ...);

/* LP64-004, at 64 bits: load_bg_file's room and portal tables. The 32-bit
 * game reads them in the file, 24- and 8-byte records; at 64 bits they are 40
 * and 16 bytes, so they are built from the file (after the byte-order pass
 * above) in the stage pool at each load, where the 32-bit game takes their
 * address, before any of load_bg_file's passes. DATA is the bg file NAME
 * (SIZE bytes; SERVED 0 for the compiled-in copy, which holds no ROM layout
 * a converter reads). Every failure stops the game by name: the compiled-in
 * copy, a file the converter refuses, and (inside mempAllocBytesInBank) a
 * stage pool that cannot give the bytes. */
void portBgTablesLoad(u8 *data, u32 size, s32 served, const char *name, bg_room_data **rooms,
        bg_portal_data_entry **portals)
{
    u32 n, nrooms;
    u8 *out;

    if (!served) {
        sysFatalError("The bg file %s is not served from the ROM; a 64-bit build cannot read its compiled-in copy",
                name);
    }
    n = portBgTablesSize(data, size, name);
    if (n == 0) {
        sysFatalError("Cannot convert the room and portal tables of the bg file %s", name);
    }
    /* The pool does not round: 8 more bytes let the tables start 8-byte
     * aligned wherever it stands, and keep its position's alignment. */
    out = (u8 *) mempAllocBytesInBank(n + 8, MEMPOOL_STAGE);
    out = (u8 *) (((uintptr_t) out + 7) & ~(uintptr_t) 7);
    if (!portBgTablesConvert(data, size, out, n, rooms, portals)) {
        sysFatalError("Cannot write the room and portal tables of the bg file %s", name);
    }
    nrooms = (u32) ((u8 *) *portals - (u8 *) *rooms) / (u32) sizeof(bg_room_data);
    portConLogf("ge: lp64: bg tables %s: %u room records, %u portal entries, %u bytes of the stage pool\n", name,
            nrooms, (n - nrooms * (u32) sizeof(bg_room_data)) / (u32) sizeof(bg_portal_data_entry), n + 8);
}
#endif
