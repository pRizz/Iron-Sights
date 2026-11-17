/**
 * The model file converter's walk (port_romconv.h; LP64-004 Task 10): claims
 * each item of a model file reachable as romfile_model.c's walk reaches it
 * (the switch slots and texture entries; from the root, Next, Child, a LOD's
 * Affects, a switch's Controls and a BSP's two children; each record's lists
 * and blocks; the lists' calls), marking which item owns each 4 bytes. An
 * item reached twice is claimed once; items that share a byte, and every
 * place romfile_model.c counts as a bounds issue, are refused. Reads only the
 * file. Compiled only at 64 bits and in the 32-bit oracle build.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "bondconstants.h"
#include "port_romconv.h"
#include "port_romlayout.h"
#include "romconv_model_walk.h"

#define MODEL_MAX_COUNT 0x8000u /* romfile_model.c's list command cap; it skips larger blocks */
#define MODEL_MAX_QUADS 0x2000u /* its DLPRIMARY cap: 0x8000 vertices */
#define MODEL_NODE_STACK 4096u  /* its MSTACK */

/* ==== which item owns each 4 bytes ===================================== */

/* Grow-only, so that converting a stock file (at most 43,120 bytes; this
 * covers 262,144, portConvScratchReserve's size) allocates nothing. */
static u8 s_KindsStatic[65536];
static u8 *s_Kinds = s_KindsStatic;
static u32 s_KindsCapacity = sizeof(s_KindsStatic);

static const char *const kKindNames[] = {"raw bytes", "switch slot", "texture entry", "node", "vertex block",
    "collision vertex block", "point-usage block", "point-usage block", "inline texture", "display list"};

const char *portModelKindName(u8 kind)
{
    return MODEL_KIND(kind) >= MODEL_RECORD ? "record" : kKindNames[MODEL_KIND(kind)];
}

int portModelKindsInit(struct ModelWalk *w)
{
    u32 slots = w->size / 4 + (w->size % 4 != 0);
    u8 *grown;

    if (slots > s_KindsCapacity)
    {
        grown = (u8 *) malloc(slots);
        if (grown == NULL)
        {
            modelRefuse(w, "no room to check the %u-byte file", w->size);
            return 0;
        }
        if (s_Kinds != s_KindsStatic)
        {
            free(s_Kinds);
        }
        s_Kinds = grown;
        s_KindsCapacity = slots;
    }
    memset(s_Kinds, 0, slots);
    w->kinds = s_Kinds;
    w->slots = slots;
    return 1;
}

/* Claims BYTES at AT for an item of KIND. Returns 1 for a first claim, 2 when
 * the same item (kind, place and length) was claimed before, 0 after refusing. */
static int claim(struct ModelWalk *w, u32 at, u64 bytes, u8 kind, const char *what)
{
    u32 first = at / 4, end, s;

    if (at % 4 != 0)
    {
        modelRefuse(w, "the %s at 0x%X is not at a multiple of 4", what, at);
        return 0;
    }
    if (bytes == 0 || (u64) at + bytes > w->size)
    {
        modelRefuse(w, "the %s at 0x%X (0x%llX bytes) runs past the %u-byte file", what, at,
                (unsigned long long) bytes, w->size);
        return 0;
    }
    end = (u32) (((u64) at + bytes + 3) / 4);
    if (w->kinds[first] == kind)
    {
        for (s = first + 1; s < end && w->kinds[s] == (kind | MODEL_INSIDE); s++)
        {
        }
        if (s == end && (end == w->slots || w->kinds[end] != (kind | MODEL_INSIDE)))
        {
            return 2;
        }
        modelRefuse(w, "the %s at 0x%X is reached again with another length", what, at);
        return 0;
    }
    for (s = first; s < end; s++)
    {
        if (w->kinds[s] != MODEL_RAW)
        {
            modelRefuse(w, "the %s at 0x%X (0x%llX bytes) overlaps a %s at 0x%X", what, at,
                    (unsigned long long) bytes, portModelKindName(w->kinds[s]), s * 4);
            return 0;
        }
    }
    w->kinds[first] = kind;
    for (s = first + 1; s < end; s++)
    {
        w->kinds[s] = (u8) (kind | MODEL_INSIDE);
    }
    return 1;
}

/* The offset a followed reference names: *HAS 0 for 0. Refuses a value that
 * is neither 0 nor a segment-5 offset (romfile_model.c reads it as 0 and
 * leaves the word for the promotion, which keeps it as an address). */
static int follow(const struct ModelWalk *w, u32 value, const char *what, u32 where, u32 *off, int *has)
{
    *has = value != 0;
    *off = value & 0x00FFFFFFu;
    if (value != 0 && (value >> 24) != 0x05)
    {
        modelRefuse(w, "the %s at 0x%X holds 0x%08X, neither 0 nor a segment-5 offset", what, where, value);
        return 0;
    }
    return 1;
}

/* ==== the walk ========================================================= */

/* Claims the display list at AT, to its end command, and the lists it calls. */
static int claimList(struct ModelWalk *w, u32 at)
{
    u32 n = 0, i, o;
    int r;

    for (i = 0; i < w->listDepth; i++)
    {
        if (w->listPath[i] == at)
        {
            modelRefuse(w, "the display list at 0x%X calls itself", at);
            return 0;
        }
    }
    if (w->listDepth > MODEL_LIST_DEPTH)
    {
        modelRefuse(w, "display lists call one another more than %u deep at 0x%X", MODEL_LIST_DEPTH, at);
        return 0;
    }
    for (o = at;; o += MODEL_ROM_COMMAND)
    {
        if ((u64) o + MODEL_ROM_COMMAND > w->size)
        {
            modelRefuse(w, "the display list at 0x%X has no end command before the file's end", at);
            return 0;
        }
        if (++n > MODEL_MAX_COUNT)
        {
            modelRefuse(w, "the display list at 0x%X has no end command in %u commands", at, MODEL_MAX_COUNT);
            return 0;
        }
        if (w->file[o] == 0xB8 || w->file[o] == 0xDF) /* G_ENDDL, and F3DEX2's */
        {
            break;
        }
    }
    r = claim(w, at, (u64) n * MODEL_ROM_COMMAND, MODEL_LIST, "display list");
    if (r != 1)
    {
        return r != 0;
    }
    w->geometry++;
    w->listPath[w->listDepth++] = at;
    for (i = 0, r = 1; r && i < n; i++)
    {
        u32 c = at + i * MODEL_ROM_COMMAND, w1 = modelBe32(w, c + 4);

        if ((w->file[c] == 0x06 || w->file[c] == 0xDE) && (w1 >> 24) == 0x05) /* G_DL, and F3DEX2's */
        {
            r = claimList(w, w1 & 0x00FFFFFFu);
        }
    }
    w->listDepth--;
    return r;
}

static int claimListAt(struct ModelWalk *w, u32 rec, u32 field)
{
    u32 off;
    int has;

    return follow(w, modelBe32(w, rec + field), "record", rec + field, &off, &has) && (!has || claimList(w, off));
}

/* Claims COUNT elements of ELEMENT bytes at the reference in REC + FIELD. */
static int claimBlock(struct ModelWalk *w, u32 rec, u32 field, u32 count, u32 element, u8 kind)
{
    u32 off;
    int has, r;

    if (!follow(w, modelBe32(w, rec + field), "record", rec + field, &off, &has))
    {
        return 0;
    }
    if (!has || count == 0)
    {
        return 1;
    }
    /* romfile_model.c leaves such a block unswapped; the converter refuses it
     * rather than copy a block the game would read as converted. */
    if (count > MODEL_MAX_COUNT)
    {
        modelRefuse(w, "the record at 0x%X counts %u elements, more than the %u converted", rec, count, MODEL_MAX_COUNT);
        return 0;
    }
    r = claim(w, off, (u64) count * element, kind, portModelKindName(kind));
    w->geometry += r == 1 && kind != MODEL_POINTUSAGE && kind != MODEL_POINTUSAGE_ODD;
    return r != 0;
}

/* The lists and blocks the record of OPCODE at REC owns, as convNode reaches them. */
static int claimRecordBlocks(struct ModelWalk *w, u32 opcode, u32 rec)
{
    u32 n, quads;

    switch (opcode)
    {
    case MODELNODE_OPCODE_DL:
        return claimListAt(w, rec, offsetof(RomModelDL, primary)) && claimListAt(w, rec, offsetof(RomModelDL, secondary))
            && claimBlock(w, rec, offsetof(RomModelDL, vertices), modelBe16(w, rec + offsetof(RomModelDL, numvertices)),
                    sizeof(RomModelVertex), MODEL_VERTEX);
    case MODELNODE_OPCODE_DLCOLLISION:
        n = modelBe16(w, rec + offsetof(RomModelDLCollision, numvertices));
        return claimListAt(w, rec, offsetof(RomModelDLCollision, primary))
            && claimListAt(w, rec, offsetof(RomModelDLCollision, secondary))
            && claimBlock(w, rec, offsetof(RomModelDLCollision, vertices), n, sizeof(RomModelVertex), MODEL_VERTEX)
            && claimBlock(w, rec, offsetof(RomModelDLCollision, collisionvertices),
                    modelBe16(w, rec + offsetof(RomModelDLCollision, numcollisionvertices)), sizeof(RomModelVertex),
                    MODEL_CVERTEX)
            && claimBlock(w, rec, offsetof(RomModelDLCollision, pointusage), n, 2,
                    n % 2 != 0 ? MODEL_POINTUSAGE_ODD : MODEL_POINTUSAGE);
    case MODELNODE_OPCODE_DLPRIMARY:
        quads = modelBe32(w, rec + offsetof(RomModelDLPrimary, numvertices));
        if (quads > MODEL_MAX_QUADS)
        {
            modelRefuse(w, "the record at 0x%X counts %u quads, more than %u", rec, quads, MODEL_MAX_QUADS);
            return 0;
        }
        /* numVertices counts quads: model.c reads four vertices for each */
        return claimBlock(w, rec, offsetof(RomModelDLPrimary, vertices), quads * 4, sizeof(RomModelVertex), MODEL_VERTEX)
            && claimListAt(w, rec, offsetof(RomModelDLPrimary, primary));
    default:
        return 1;
    }
}

static int push(struct ModelWalk *w, u32 *stack, u32 *sp, u32 off, int has)
{
    if (!has)
    {
        return 1;
    }
    if (*sp == MODEL_NODE_STACK)
    {
        modelRefuse(w, "the node walk passes %u pending nodes", MODEL_NODE_STACK);
        return 0;
    }
    stack[(*sp)++] = off;
    return 1;
}

/* Claims the node at OFF, its record and the record's blocks, and pushes the
 * nodes it leads to: Next, and Child or, for a LOD, Affects and, for a switch,
 * Controls; a BSP's two children too. */
static int walkNode(struct ModelWalk *w, u32 off, u32 *stack, u32 *sp)
{
    u32 opcode, rsize, data, next, child, left = 0, right = 0;
    int r, hasData, hasNext, hasChild, hasLeft = 0, hasRight = 0;

    if (off < w->root)
    {
        modelRefuse(w, "a node at 0x%X lies below the root at 0x%X", off, w->root);
        return 0;
    }
    r = claim(w, off, sizeof(RomModelNode), MODEL_NODE, "node");
    if (r != 1)
    {
        return r != 0;
    }
    w->nodes++;
    opcode = modelBe16(w, off + offsetof(RomModelNode, opcode)) & 0xFFu;
    rsize = portModelRecordFileSize(opcode);
    if (!follow(w, modelBe32(w, off + offsetof(RomModelNode, data)), "node", off, &data, &hasData)
            || !follow(w, modelBe32(w, off + offsetof(RomModelNode, next)), "node", off, &next, &hasNext)
            || !follow(w, modelBe32(w, off + offsetof(RomModelNode, child)), "node", off, &child, &hasChild))
    {
        return 0;
    }
    if (hasData != (rsize != 0))
    {
        modelRefuse(w, "the node at 0x%X has opcode %u, which %s, and %s", off, opcode,
                rsize != 0 ? "has a record" : "has no record", hasData ? "names one" : "names none");
        return 0;
    }
    if (hasData)
    {
        r = claim(w, data, rsize, (u8) (MODEL_RECORD + opcode), "record");
        if (r == 0 || (r == 1 && !claimRecordBlocks(w, opcode, data)))
        {
            return 0;
        }
        if (opcode == MODELNODE_OPCODE_LOD)
        {
            r = follow(w, modelBe32(w, data + offsetof(RomModelLOD, affects)), "record", data, &child, &hasChild);
        }
        else if (opcode == MODELNODE_OPCODE_SWITCH)
        {
            r = follow(w, modelBe32(w, data + offsetof(RomModelSwitch, controls)), "record", data, &child, &hasChild);
        }
        else if (opcode == MODELNODE_OPCODE_BSP)
        {
            r = follow(w, modelBe32(w, data + offsetof(RomModelBSP, leftchild)), "record", data, &left, &hasLeft)
                && follow(w, modelBe32(w, data + offsetof(RomModelBSP, rightchild)), "record", data, &right, &hasRight);
        }
        if (!r)
        {
            return 0;
        }
    }
    return push(w, stack, sp, left, hasLeft) && push(w, stack, sp, right, hasRight) && push(w, stack, sp, next, hasNext)
        && push(w, stack, sp, child, hasChild);
}

/* The switch slots, the texture entries and their inline texels. */
static int claimHead(struct ModelWalk *w)
{
    u32 i;

    for (i = 0; i < w->numSwitches; i++)
    {
        if (!claim(w, i * 4, 4, MODEL_SWITCH, "switch slot"))
        {
            return 0;
        }
    }
    for (i = 0; i < w->numTextures; i++)
    {
        u32 at = w->numSwitches * 4 + i * (u32) sizeof(RomModelTexture), id = modelBe32(w, at);
        u32 depth = w->file[at + offsetof(RomModelTexture, renderdepth)], texels;

        if (!claim(w, at, sizeof(RomModelTexture), MODEL_TEXTURE, "texture entry"))
        {
            return 0;
        }
        if ((id >> 24) != 0x05)
        {
            continue;
        }
        if (depth > 3)
        {
            modelRefuse(w, "texture entry %u has texel depth %u, more than 3", i, depth);
            return 0;
        }
        texels = (((u32) w->file[at + offsetof(RomModelTexture, width)] * w->file[at + offsetof(RomModelTexture, height)])
                         << depth)
            >> 1;
        if (texels == 0)
        {
            modelRefuse(w, "texture entry %u names inline texels but has none", i);
            return 0;
        }
        if (!claim(w, id & 0x00FFFFFFu, (texels + 3) & ~3u, MODEL_TEXEL, "inline texture"))
        {
            return 0;
        }
    }
    return 1;
}

static int walkNodes(struct ModelWalk *w)
{
    u32 stack[MODEL_NODE_STACK], sp = 0;

    stack[sp++] = w->root;
    while (sp > 0)
    {
        if (!walkNode(w, stack[--sp], stack, &sp))
        {
            return 0;
        }
    }
    if (w->geometry == 0)
    {
        modelRefuse(w, "it has no vertices and no display lists, which romfile_model.c refuses too");
        return 0;
    }
    return 1;
}

int portModelClaim(struct ModelWalk *w)
{
    return claimHead(w) && walkNodes(w);
}
