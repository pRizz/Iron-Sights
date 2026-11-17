/**
 * The setup file converter (port_romconv_stage.h; LP64-004 Task 14).
 *
 * A setup file holds a ten-offset header, eight tables, s32 lists, names, AI
 * byte code, the intro and the prop records, cross-referenced by file
 * offsets. At 32 bits romfile_setup.c swaps them in place; at 64 bits the
 * header, most table records, the intro's camera and credits records and
 * most prop records grow, so the converter writes a new image.
 *
 * Planning reads only the file: the claims mark which item owns each 4 bytes
 * (every table to its terminator, every list to its negative word, the intro
 * and the credits tables, the prop list), refusing items that share a byte.
 * The layout then places every item in file order, each at its run-time size
 * and alignment, with the bytes no item owns (names, AI code, padding) copied
 * between them as they are; at 32 bits every size and place is the file's,
 * so the image is the in-place one. The offset map holds each new place, a
 * checking pass resolves every reference through it, and the writing pass,
 * which runs the same code, finds nothing to refuse. Compiled only at 64 bits
 * and in the 32-bit oracle build.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "port_romconv.h"
#include "port_romconv_stage.h"
#include "romconv_setup_walk.h"
#include "romconv_census.h"
#include "platform_info.h"
#include "port_romlayout_stage.h"
#include "bondconstants.h"

extern void portConLogf(const char *fmt, ...);

#define SETUP_ROUND8(n) (((n) + 7u) & ~(u64) 7u)

/* ==== the layout and the writing ====================================== */

/* The item starting at SLOT; 0 after refusing one the image cannot hold. */
static int itemAt(const struct SetupWalk *w, u32 slot, struct SetupItem *it)
{
    const u8 kind = w->kinds[slot];
    const u8 inside = kind == SETUP_RAW || kind == SETUP_LIST ? kind : (u8) (kind | SETUP_INSIDE);
    u32 end = slot + 1;
    u64 out;

    it->at = slot * 4;
    it->kind = kind;
    while (end < w->slots && w->kinds[end] == inside)
    {
        end++;
    }
    it->bytes = (u64) end * 4 <= w->size ? (end - slot) * 4 : w->size - it->at; /* the file's last partial slot */
    switch (kind)
    {
    case SETUP_RAW:
    case SETUP_LIST: out = it->bytes; break;
    case SETUP_INTRO: out = portSetupIntroOutBytes(w, it->at); break;
    case SETUP_PROPS: out = portSetupPropsOutBytes(w, it->at, it->bytes); break;
    default: out = (u64) (it->bytes / portSetupRecordFileSize(kind)) * portSetupRecordOutSize(kind); break;
    }
    if (out == 0 || out > 0xFFFFFFF0u)
    {
        if (out != 0)
        {
            setupRefuse(w, "the %s at 0x%X would need 0x%llX bytes", portSetupKindName(kind), it->at,
                    (unsigned long long) out);
        }
        return 0;
    }
    it->outBytes = (u32) out;
    return 1;
}

/* Gives every item its new place, in file order, and every slot of a list run
 * or of unowned bytes its own, so a reference into one is remapped. */
static int layout(struct SetupWalk *w)
{
    u64 cursor = 0;
    u32 slot = 0, s, span;
    struct SetupItem it;

    while (slot < w->slots)
    {
        u64 to;
        u32 align;

        if (!itemAt(w, slot, &it))
        {
            return 0;
        }
        span = (it.bytes + 3) / 4;
        align = it.kind == SETUP_RAW ? 1 : portSetupKindAlign(it.kind);
        to = (cursor + align - 1) & ~(u64) (align - 1);
        if (SETUP_ROUND8(to + it.outBytes) > 0xFFFFFFF8u)
        {
            setupRefuse(w, "the %s at 0x%X would end past 4 GB in the converted file", portSetupKindName(it.kind),
                    it.at);
            return 0;
        }
        portConvMapPut(&w->map, it.at, (u32) to);
        for (s = 1; s < span && (it.kind == SETUP_RAW || it.kind == SETUP_LIST); s++)
        {
            portConvMapPut(&w->map, it.at + s * 4, (u32) to + s * 4);
        }
        cursor = to + it.outBytes;
        slot += span;
    }
    w->total = (u32) SETUP_ROUND8(cursor);
    return 1;
}

/* Checks (W->out NULL) or writes every item at its place. The places come
 * from the map the layout filled, by the same item boundaries. */
static int writeAll(const struct SetupWalk *w)
{
    u32 slot = 0, to;
    struct SetupItem it;
    int ok = 1;

    while (ok && slot < w->slots)
    {
        if (!itemAt(w, slot, &it))
        {
            return 0;
        }
        /* A backstop: the layout gave this item these bytes. */
        if (!portConvMapGet(&w->map, it.at, &to) || (u64) to + it.outBytes > w->total)
        {
            setupRefuse(w, "the %s at 0x%X outgrows the %u bytes measured", portSetupKindName(it.kind), it.at,
                    w->total);
            return 0;
        }
        switch (it.kind)
        {
        case SETUP_RAW:
            if (w->out != NULL)
            {
                memcpy(w->out + to, w->file + it.at, it.bytes);
            }
            break;
        case SETUP_HEADER: ok = portSetupWriteHeader(w, &it, to); break;
        case SETUP_LIST: ok = portSetupWriteList(w, &it, to); break;
        case SETUP_INTRO: ok = portSetupWriteIntro(w, &it, to); break;
        case SETUP_CREDITS: ok = portSetupWriteCredits(w, &it, to); break;
        case SETUP_PROPS: ok = portSetupWriteProps(w, &it, to); break;
        default: ok = portSetupWriteTable(w, &it, to); break;
        }
        slot += (it.bytes + 3) / 4;
    }
    return ok;
}

#if IS_64_BIT
/* The line romfile_setup.c's in-place pass prints, with the file's counts
 * (prop list and intro words to their end records, pads before the
 * terminator), for the load census (lp64_census.py), and the census line of
 * the prop record types; the in-place pass no longer runs here. */
static void setupReport(const struct SetupWalk *w)
{
    u32 slot = 0, propWords = 0, introWords = 0, pads = 0, types[PROPDEF_MAX] = {0};
    struct SetupItem it;

    while (slot < w->slots && itemAt(w, slot, &it))
    {
        if (it.kind == SETUP_PROPS)
        {
            propWords = it.bytes / 4;
            portSetupPropsCount(w->file + it.at, it.bytes, types);
        }
        else if (it.kind == SETUP_INTRO)
        {
            introWords = it.bytes / 4;
        }
        else if (it.kind == SETUP_PADS)
        {
            pads = it.bytes / (u32) sizeof(RomPad) - 1;
        }
        slot += (it.bytes + 3) / 4;
    }
    portConLogf("ge: romdata: setup %-16s converted (%u propdef words, %u intro words, %u pads)\n", w->name, propWords,
            introWords, pads);
    portCensusLine("setup", w->name, "props", types, PROPDEF_MAX);
}
#endif

/* ==== the interface ==================================================== */

/* Claims, lays out and checks FILE; the map stays live on success. */
static int setupPlan(struct SetupWalk *w, const u8 *file, u32 size, const char *name)
{
    int ok;

    memset(w, 0, sizeof(*w));
    w->file = file;
    w->size = size;
    w->name = name;
    if (file == NULL)
    {
        setupRefuse(w, "no file");
        return 0;
    }
    if (!portSetupKindsInit(w))
    {
        return 0;
    }
    if (!portConvMapInit(&w->map, size))
    {
        setupRefuse(w, "no offset map for the %u-byte file", size);
        return 0;
    }
    ok = portSetupClaimTables(w) && portSetupClaimIntroAndProps(w) && layout(w) && writeAll(w);
    if (!ok)
    {
        portConvMapFree(&w->map);
    }
    return ok;
}

u32 portSetupConvertedSize(const u8 *file, u32 size, const char *name)
{
    struct SetupWalk w;

    if (!setupPlan(&w, file, size, name))
    {
        return 0;
    }
    portConvMapFree(&w.map);
    return w.total;
}

s32 portSetupConvert(const u8 *file, u32 size, u8 *out, u32 outsize, const char *name)
{
    struct SetupWalk w;
    int ok;

    if (!setupPlan(&w, file, size, name))
    {
        return 0;
    }
    if (w.total > outsize)
    {
        setupRefuse(&w, "the converted file needs %u bytes, the buffer has %u", w.total, outsize);
        ok = 0;
    }
    else if (out == NULL || (uintptr_t) out % 8 != 0)
    {
        setupRefuse(&w, "the output buffer at %p is NULL or not 8-byte aligned", (void *) out);
        ok = 0;
    }
    else if ((uintptr_t) out < (uintptr_t) file + size && (uintptr_t) file < (uintptr_t) out + w.total)
    {
        setupRefuse(&w, "the output buffer overlaps the file");
        ok = 0;
    }
    else
    {
        memset(out, 0, w.total);
        w.out = out;
        ok = writeAll(&w);
    }
#if IS_64_BIT
    if (ok)
    {
        setupReport(&w);
    }
#endif
    portConvMapFree(&w.map);
    return ok;
}
