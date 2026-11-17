/**
 * The model file converter's references (port_romconv.h; LP64-004 Task 10):
 * the one lookup every reference goes through, the display lists' address
 * words, and a check that the nodes' links let the game's promotion end.
 * Compiled only at 64 bits and in the 32-bit oracle build.
 */
#include <stddef.h>
#include <stdint.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "bondconstants.h"
#include "port_romconv.h"
#include "port_romlayout.h"
#include "romconv_model_walk.h"

static const char *const kRefNames[] = {"a node", "its node's record", "a node or a record", "an item",
    "a display list", "a vertex block", "a collision vertex block", "a point-usage block", "inline texels",
    "a texture entry", "anything converted", "a place in a vertex block"};

/* Whether the slot kind K is something REF may name. */
static int refAccepts(enum ModelRef ref, u8 k, u32 opcode)
{
    int start = (k & MODEL_INSIDE) == 0;

    switch (ref)
    {
    case REF_NODE: return k == MODEL_NODE;
    case REF_RECORD: return k == MODEL_RECORD + opcode;
    case REF_NODE_OR_RECORD: return k == MODEL_NODE || (start && k >= MODEL_RECORD);
    case REF_ANY: return start && k != MODEL_RAW;
    case REF_LIST: return k == MODEL_LIST;
    /* A block of no elements is not claimed; romfile_model.c leaves its
     * bytes as the file has them, and so does the layout. */
    case REF_VERTEX: return k == MODEL_VERTEX || k == MODEL_RAW;
    case REF_CVERTEX: return k == MODEL_CVERTEX || k == MODEL_RAW;
    case REF_POINTUSAGE: return k == MODEL_POINTUSAGE || k == MODEL_POINTUSAGE_ODD || k == MODEL_RAW;
    case REF_TEXEL: return MODEL_KIND(k) == MODEL_TEXEL;
    case REF_TEXTURE: return k == MODEL_TEXTURE;
    /* A vertex keeps its 16 bytes, so a place inside a block moves with it. */
    case REF_VERTEX_LOAD: return MODEL_KIND(k) == MODEL_VERTEX || k == MODEL_RAW;
    default: return 0;
    }
}

static void refuseTarget(const struct ModelWalk *w, const char *what, u32 where, u32 off, const char *wanted)
{
    u8 k = off % 4 == 0 && off < w->size ? w->kinds[off / 4] : MODEL_RAW;

    if (k == MODEL_RAW)
    {
        modelRefuse(w, "the %s at 0x%X names 0x%X, which nothing owns, not %s", what, where, off, wanted);
    }
    else
    {
        modelRefuse(w, "the %s at 0x%X names 0x%X, %s a %s, not %s", what, where, off,
                (k & MODEL_INSIDE) != 0 ? "inside" : "the start of", portModelKindName(k), wanted);
    }
}

int portModelResolve(const struct ModelWalk *w, u32 value, enum ModelRef ref, u32 opcode, const char *what, u32 where,
        u32 *to)
{
    u32 off = value & 0x00FFFFFFu;

    *to = 0;
    if (value == 0)
    {
        return 1;
    }
    if ((value >> 24) != 0x05)
    {
        modelRefuse(w, "the %s at 0x%X holds 0x%08X, neither 0 nor a segment-5 offset", what, where, value);
        return 0;
    }
    if (off % 4 != 0 || off >= w->size || !refAccepts(ref, w->kinds[off / 4], opcode)
            || !portConvMapGet(&w->map, off, to))
    {
        refuseTarget(w, what, where, off, kRefNames[ref]);
        return 0;
    }
    return 1;
}

/* A G_VTX word: the LEN bytes of vertices it loads from VALUE lie in one
 * vertex block, or in raw bytes no item interrupts (laid out unbroken). */
static int resolveVertexSpan(const struct ModelWalk *w, u32 value, u32 len, u32 where, u32 *to)
{
    u32 off = value & 0x00FFFFFFu, s, end;
    u8 first;

    if (!portModelResolve(w, value, REF_VERTEX_LOAD, 0, "vertex command", where, to))
    {
        return 0;
    }
    first = w->kinds[off / 4];
    end = (u32) (((u64) off + len + 3) / 4);
    for (s = off / 4 + 1; s < end; s++)
    {
        u8 k = s < w->slots ? w->kinds[s] : (u8) 0xFF;

        if (first == MODEL_RAW ? k != MODEL_RAW : k != (MODEL_VERTEX | MODEL_INSIDE))
        {
            modelRefuse(w, "the vertex command at 0x%X loads 0x%X bytes from 0x%X, past the %s there", where, len, off,
                    first == MODEL_RAW ? "raw bytes" : "vertex block");
            return 0;
        }
    }
    return 1;
}

int portModelWriteList(const struct ModelWalk *w, const struct ModelItem *item, u32 to)
{
    u32 n = item->bytes / MODEL_ROM_COMMAND, i;
    Gfx *dst = w->out != NULL ? (Gfx *) (w->out + to) : NULL;

    if (dst != NULL)
    {
        portGfxFromRom(dst, w->file + item->at, n);
    }
    for (i = 0; i < n; i++)
    {
        u32 c = item->at + i * MODEL_ROM_COMMAND, w0 = modelBe32(w, c), w1 = modelBe32(w, c + 4), moved;
        u32 op = w0 >> 24;
        int ok;

        /* Segment 3 and 4 words, and every other command, are copied. */
        if ((w1 >> 24) != 0x05)
        {
            continue;
        }
        if (op == 0x04) /* G_VTX: w0's low half-word is the bytes loaded */
        {
            ok = resolveVertexSpan(w, w1, w0 & 0xFFFFu, c, &moved);
        }
        else if (op == 0xFD) /* G_SETTIMG */
        {
            ok = portModelResolve(w, w1, REF_TEXEL, 0, "texture image command", c, &moved);
        }
        else if (op == 0x06 || op == 0xDE) /* G_DL, and F3DEX2's */
        {
            ok = portModelResolve(w, w1, REF_LIST, 0, "display-list call", c, &moved);
        }
        else
        {
            continue;
        }
        if (!ok)
        {
            return 0;
        }
        if (dst != NULL)
        {
            dst[i].words.w1 = (uintptr_t) (MODEL_SEGMENT | moved);
        }
    }
    return 1;
}

/* The node a link at FIELD of the node at NODE names (*HAS 0 for none). */
static int nodeLink(const struct ModelWalk *w, u32 node, u32 field, const char *what, u32 *to, int *has)
{
    u32 value = modelBe32(w, node + field), off = value & 0x00FFFFFFu;

    *has = value != 0;
    *to = off;
    if (value != 0 && ((value >> 24) != 0x05 || off % 4 != 0 || off >= w->size || w->kinds[off / 4] != MODEL_NODE))
    {
        modelRefuse(w, "the %s of the node at 0x%X, 0x%08X, is not a node", what, node, value);
        return 0;
    }
    return 1;
}

int portModelCheckPromoteWalk(const struct ModelWalk *w)
{
    /* Down once and up once a node, and the last step up to none. */
    u64 steps = 0, limit = 2 * (u64) w->nodes + 2;
    u32 node = w->root, next;
    int has = 1, hasNext;

    while (has)
    {
        u32 child, data;
        int hasChild;

        if (++steps > limit)
        {
            modelRefuse(w, "the nodes' Child, Next and Parent links loop: the game's walk of them would not end");
            return 0;
        }
        if (!nodeLink(w, node, offsetof(RomModelNode, child), "Child", &child, &hasChild))
        {
            return 0;
        }
        /* The promotion sets a LOD's Child to its Affects before it steps. */
        data = modelBe32(w, node + offsetof(RomModelNode, data)) & 0x00FFFFFFu;
        if ((modelBe16(w, node) & 0xFFu) == MODELNODE_OPCODE_LOD
                && !nodeLink(w, data, offsetof(RomModelLOD, affects), "LOD's Affects", &child, &hasChild))
        {
            return 0;
        }
        if (hasChild)
        {
            node = child;
            continue;
        }
        while (has)
        {
            if (!nodeLink(w, node, offsetof(RomModelNode, next), "Next", &next, &hasNext))
            {
                return 0;
            }
            if (hasNext)
            {
                node = next;
                break;
            }
            if (!nodeLink(w, node, offsetof(RomModelNode, parent), "Parent", &node, &has))
            {
                return 0;
            }
            if (has && ++steps > limit)
            {
                modelRefuse(w, "the nodes' Child, Next and Parent links loop: the game's walk of them would not end");
                return 0;
            }
        }
    }
    return 1;
}
