/* The models' converted display lists at 64 bits: which block of the high
 * arena belongs to which node record, and when it may be used again. The rule
 * and its reasons are in port_gdl_store.h. CMake compiles this file only at
 * 64 bits; scripts/port/test_gdl_store.c compiles it at both widths. */
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "port_gdl_store.h"
#include "port_poison.h"

extern void *portAllocHigh(u32 size);                 /* pcsched.c */
extern void portHighArenaReport(void);                /* pcsched.c: the arena's use, for its peak line */
extern u32 portGfxFramesSubmitted(void);              /* pcsched.c */
extern u32 portGfxFramesFinished(void);               /* pcsched.c */
extern void portPartCacheVerticesFreed(const void *vtx); /* roc_hooks.c */
extern s32 texLoadFromGdl(Gfx *src, s32 srcsize, Gfx *dst, void *texpool); /* tex.c */
extern void portConLogf(const char *fmt, ...);

/* ==== the store ========================================================= */

typedef struct GdlBlock
{
    u8 *block;        /* in the high arena */
    u32 bytes;        /* its size: the worst case of the list it was first taken for */
    uintptr_t holder; /* the node record that points at it; unused while the block is spare */
    u32 frames;       /* a spare block: the frames submitted when it was given back */
    s32 next;         /* a spare block: the next larger or equal one, -1 at the end */
    s32 spare;
} GdlBlock;

static GdlBlock *s_Blocks;
static u32 s_BlockCount, s_BlockRoom;
static s32 s_SpareFirst = -1; /* the spare blocks, smallest first */
static u32 s_Held, s_Spare, s_Bytes;

void portGdlStoreGiveBack(const void *start, u32 bytes)
{
    const uintptr_t first = (uintptr_t) start;
    u32 i;

    for (i = 0; i < s_BlockCount; i++)
    {
        GdlBlock *block = &s_Blocks[i];
        s32 *at = &s_SpareFirst;

        if (block->spare || block->holder - first >= (uintptr_t) bytes)
        {
            continue; /* held by a record this image did not write over (unsigned: below START wraps) */
        }
        while (*at >= 0 && s_Blocks[*at].bytes < block->bytes)
        {
            at = &s_Blocks[*at].next;
        }
        block->spare = 1;
        block->frames = portGfxFramesSubmitted();
        block->next = *at;
        *at = (s32) i;
        s_Held--;
        s_Spare++;
        PORT_POISON(block->block, block->bytes);
    }
}

/* The spare block to take for a list whose worst case is BYTES: the smallest
 * that holds it with every frame submitted before its give-back finished.
 * *LINK is the link that names it. NULL: none. */
static GdlBlock *gdlFindSpare(u32 bytes, s32 **link)
{
    const u32 finished = portGfxFramesFinished();
    s32 *at = &s_SpareFirst;

    while (*at >= 0)
    {
        GdlBlock *block = &s_Blocks[*at];

        if (block->bytes >= bytes && (s32) (finished - block->frames) >= 0)
        {
            *link = at;
            return block;
        }
        at = &block->next;
    }
    return NULL;
}

/* A new entry for a block the arena just gave; NULL when the table cannot
 * grow, and the block is then one the store never gives back. */
static GdlBlock *gdlNewEntry(void)
{
    if (s_BlockCount == s_BlockRoom)
    {
        const u32 room = s_BlockRoom ? s_BlockRoom * 2 : 1024;
        GdlBlock *blocks = realloc(s_Blocks, (size_t) room * sizeof(GdlBlock));

        if (blocks == NULL)
        {
            static s32 said;
            if (!said)
            {
                said = 1;
                portConLogf("ge: gdl store: no memory for %u entries; new blocks are not reused\n", (unsigned) room);
            }
            return NULL;
        }
        s_Blocks = blocks;
        s_BlockRoom = room;
    }
    return &s_Blocks[s_BlockCount++];
}

void *portGdlStoreTake(const void *holder, u32 bytes, s32 *again)
{
    s32 *link = NULL;
    GdlBlock *block = gdlFindSpare(bytes, &link);

    *again = block != NULL;
    if (block != NULL)
    {
        *link = block->next;
        s_Spare--;
        PORT_UNPOISON(block->block, block->bytes);
        memset(block->block, 0, block->bytes); /* a new block of the arena is zero */
    }
    else
    {
        u8 *fresh = portAllocHigh(bytes);

        if (fresh == NULL)
        {
            return NULL;
        }
        block = gdlNewEntry();
        if (block == NULL)
        {
            return fresh;
        }
        block->block = fresh;
        block->bytes = bytes;
        s_Bytes += bytes;
    }
    block->holder = (uintptr_t) holder;
    block->spare = 0;
    block->next = -1;
    s_Held++;
    return block->block;
}

void portGdlStoreCounts(u32 *held, u32 *spare, u32 *bytes)
{
    *held = s_Held;
    *spare = s_Spare;
    *bytes = s_Bytes;
}

/* ==== the models' side =================================================== */

static s32 s_CallerRoom; /* the image being converted stands in a caller's buffer */

void portModelGdlImageLoaded(const void *start, u32 bytes, s32 callerRoom)
{
    s_CallerRoom = callerRoom;
    portGdlStoreGiveBack(start, bytes);
    portHighArenaReport();
}

/* The part cache keys a retained part on its list's address and its vertex
 * base (roc_hooks.c). A list that takes a block again in a buffer the stage
 * loads more than once could meet the key of the list that had the block
 * before, so the cache forgets the base this node's lists are drawn with:
 * a gun-list node's is its model's base, a part node's its vertices
 * (model.c: modelRenderNodeGundl, modelRenderNodeDl). */
static void gdlForgetParts(const struct ModelNode *node)
{
    switch (node->Opcode & 0xff)
    {
        case MODELNODE_OPCODE_DL:
            portPartCacheVerticesFreed(node->Data->DisplayList.BaseAddr);
            break;
        case MODELNODE_OPCODE_DLCOLLISION:
            portPartCacheVerticesFreed(node->Data->DisplayListCollisions.Vertices);
            break;
        default:
            break;
    }
}

Gfx *portModelGdlConvert(struct ModelNode *node, Gfx *src, struct texpool *pool)
{
    s32 n = 0, again = 0;
    Gfx *dst;

    while ((u8) (src[n].words.w0 >> 24) != (u8) G_ENDDL)
    {
        n++;
        if (n > 0x4000)
        {
            return NULL; /* no end: the node keeps its list */
        }
    }
    n++; /* the end too */

    dst = portGdlStoreTake(node->Data, (u32) (n * 4 + 96) * (u32) sizeof(Gfx), &again);
    if (dst == NULL)
    {
        return NULL;
    }
    if (again && s_CallerRoom)
    {
        gdlForgetParts(node);
    }
    texLoadFromGdl(src, n * (s32) sizeof(Gfx), dst, pool);
    return dst;
}
