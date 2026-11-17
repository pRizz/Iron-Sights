/**
 * The model file converter (port_romconv.h; LP64-004 Task 10).
 *
 * A model file holds switch slots, texture entries, nodes, records, vertex,
 * point-usage and inline-texel blocks and display lists, all cross-referenced
 * by segment-5 offsets. At 32 bits romfile_model.c swaps them in place; at 64
 * bits the slots, entries, nodes, most records and every list command grow, so
 * the converter writes a new image.
 *
 * Planning reads only the file: the walk claims each item reachable as
 * romfile_model.c's walk reaches it (Next, Child, a LOD's Affects, a switch's
 * Controls, a BSP's two children, each record's lists and blocks, the list
 * calls), marking which item owns each 4 bytes; an item reached twice is
 * claimed once, and items that share a byte are refused. The layout then
 * places every item in file order, each at its run-time size, aligned to a
 * pointer, with bytes no item owns copied between them as they are; at 32 bits
 * every size and place is the file's, so the image is the in-place one. The
 * offset map holds each new place, and a checking pass resolves every
 * reference through it, so the writing pass, which runs the same code, finds
 * nothing to refuse. Compiled only at 64 bits and in the 32-bit oracle build.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "bondconstants.h"
#include "platform_info.h"
#include "port_romconv.h"
#include "port_romlayout.h"
#include "romconv_model_walk.h"

/* 8 at 64 bits. At 32 bits 4, which every item of a file the walk accepts
 * already has, so no padding moves anything from its file place. */
#define MODEL_ALIGN ((u32) sizeof(void *))
#define MODEL_ROUND8(n) (((n) + 7u) & ~(u64) 7u)
#define MODEL_MAX_BYTES 0xFFFFFFF0u

/* ==== the layout and the writing ====================================== */

static void itemAt(const struct ModelWalk *w, u32 slot, struct ModelItem *it)
{
    u8 kind = w->kinds[slot];
    u32 end = slot + 1, op = MODEL_KIND(kind) - MODEL_RECORD;

    it->at = slot * 4;
    it->kind = kind;
    while (kind != MODEL_RAW && end < w->slots && w->kinds[end] == (kind | MODEL_INSIDE))
    {
        end++;
    }
    it->bytes = end * 4 <= w->size ? (end - slot) * 4 : w->size - it->at; /* the file's last partial slot */
    switch (MODEL_KIND(kind))
    {
    case MODEL_SWITCH: it->outBytes = sizeof(ModelNode *); break;
    case MODEL_TEXTURE: it->outBytes = sizeof(ModelFileTextures); break;
    case MODEL_NODE: it->outBytes = sizeof(ModelNode); break;
    case MODEL_LIST: it->outBytes = it->bytes / MODEL_ROM_COMMAND * (u32) sizeof(Gfx); break;
    default: it->outBytes = MODEL_KIND(kind) >= MODEL_RECORD ? portModelRecordOutSize(op) : it->bytes; break;
    }
}

static int isDataBlock(u8 kind)
{
    return kind == MODEL_VERTEX || kind == MODEL_CVERTEX || kind == MODEL_POINTUSAGE || kind == MODEL_POINTUSAGE_ODD
        || kind == MODEL_TEXEL;
}

/* Gives every item its new place, in file order, and every slot of a data
 * block or of raw bytes its own, so a reference into one is remapped. */
static int layout(struct ModelWalk *w)
{
    u64 cursor = 0;
    u32 slot = 0, s;
    struct ModelItem it;

    while (slot < w->slots)
    {
        u32 span;
        u64 to;

        itemAt(w, slot, &it);
        span = (it.bytes + 3) / 4;
        to = it.kind == MODEL_RAW ? cursor : (cursor + MODEL_ALIGN - 1) & ~(u64) (MODEL_ALIGN - 1);
        if (to + it.outBytes > MODEL_MAX_BYTES)
        {
            modelRefuse(w, "the converted file passes 4 GB at the %s at 0x%X", portModelKindName(it.kind), it.at);
            return 0;
        }
        portConvMapPut(&w->map, it.at, (u32) to);
        for (s = 1; s < span && (isDataBlock(it.kind) || it.kind == MODEL_RAW); s++)
        {
            portConvMapPut(&w->map, it.at + s * 4, (u32) to + s * 4);
        }
        cursor = to + it.outBytes;
        slot += span;
    }
    w->total = (u32) MODEL_ROUND8(cursor);
    return 1;
}

/* Checks (W->out NULL) or writes every item at its place. The places come
 * from the map the layout filled, by the same item boundaries. */
static int writeAll(const struct ModelWalk *w)
{
    u32 slot = 0, to;
    struct ModelItem it;
    int ok = 1;

    while (ok && slot < w->slots)
    {
        u32 kind;

        itemAt(w, slot, &it);
        kind = MODEL_KIND(it.kind);
        /* A backstop: the layout gave this item these bytes. */
        if (!portConvMapGet(&w->map, it.at, &to) || (u64) to + it.outBytes > w->total)
        {
            modelRefuse(w, "the %s at 0x%X outgrows the %u bytes measured", portModelKindName(it.kind), it.at, w->total);
            return 0;
        }
        if (it.kind == MODEL_RAW)
        {
            if (w->out != NULL)
            {
                memcpy(w->out + to, w->file + it.at, it.bytes);
            }
        }
        else if (kind == MODEL_SWITCH)
        {
            ok = portModelWriteSwitch(w, &it, to);
        }
        else if (kind == MODEL_TEXTURE)
        {
            ok = portModelWriteTexture(w, &it, to);
        }
        else if (kind == MODEL_NODE)
        {
            ok = portModelWriteNode(w, &it, to);
        }
        else if (kind == MODEL_LIST)
        {
            ok = portModelWriteList(w, &it, to);
        }
        else if (kind >= MODEL_RECORD)
        {
            ok = portModelWriteRecord(w, &it, to);
        }
        else
        {
            ok = portModelWriteBlock(w, &it, to);
        }
        slot += (it.bytes + 3) / 4;
    }
    return ok;
}

/* ==== the interface ==================================================== */

/* Walks, lays out and checks FILE; the map stays live on success. */
static int modelPlan(struct ModelWalk *w, const u8 *file, u32 size, s32 numSwitches, s32 numtextures, const char *name)
{
    int ok;

    memset(w, 0, sizeof(*w));
    w->file = file;
    w->size = size;
    w->name = name;
    if (file == NULL || size < sizeof(RomModelNode) || numSwitches < 0 || numtextures < 0
            || (u64) numSwitches * 4 + (u64) numtextures * sizeof(RomModelTexture) > size - sizeof(RomModelNode))
    {
        modelRefuse(w, "%d switch slots and %d texture entries leave no room for the root node in %u bytes",
                numSwitches, numtextures, size);
        return 0;
    }
    w->numSwitches = (u32) numSwitches;
    w->numTextures = (u32) numtextures;
    w->root = w->numSwitches * 4 + w->numTextures * (u32) sizeof(RomModelTexture);
    if (!portModelKindsInit(w))
    {
        return 0;
    }
    if (!portConvMapInit(&w->map, size))
    {
        modelRefuse(w, "no offset map for the %u-byte file", size);
        return 0;
    }
    ok = portModelClaim(w) && portModelCheckPromoteWalk(w) && layout(w) && writeAll(w);
    if (!ok)
    {
        portConvMapFree(&w->map);
    }
    return ok;
}

u32 portModelConvertedSize(const u8 *file, u32 size, s32 numSwitches, s32 numtextures, const char *name)
{
    struct ModelWalk w;

    if (!modelPlan(&w, file, size, numSwitches, numtextures, name))
    {
        return 0;
    }
    portConvMapFree(&w.map);
    return w.total;
}

s32 portModelConvert(const u8 *file, u32 size, u8 *out, u32 outsize, s32 numSwitches, s32 numtextures,
        const char *name)
{
    struct ModelWalk w;
    int ok;

    if (!modelPlan(&w, file, size, numSwitches, numtextures, name))
    {
        return 0;
    }
    if (w.total > outsize)
    {
        modelRefuse(&w, "the converted file needs %u bytes, the buffer has %u", w.total, outsize);
        ok = 0;
    }
    else if (out == NULL || (uintptr_t) out % 8 != 0)
    {
        modelRefuse(&w, "the output buffer at %p is NULL or not 8-byte aligned", (void *) out);
        ok = 0;
    }
    else if ((uintptr_t) out < (uintptr_t) file + size && (uintptr_t) file < (uintptr_t) out + w.total)
    {
        modelRefuse(&w, "the output buffer overlaps the file");
        ok = 0;
    }
    else
    {
        memset(out, 0, w.total);
        w.out = out;
        ok = writeAll(&w);
    }
    portConvMapFree(&w.map);
    return ok;
}
