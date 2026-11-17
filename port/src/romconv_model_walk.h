/**
 * The model file converter's shared state (LP64-004 Task 10), used by
 * romconv_model.c (the walk and the layout), romconv_model_refs.c (the
 * reference lookup, the display lists' words and the promotion's walk) and
 * romconv_model_records.c (one writer for each kind of item). Not part of
 * port_romconv.h's interface; compiled only where the converters are (64 bits
 * and the 32-bit oracle build).
 */
#ifndef ROMCONV_MODEL_WALK_H
#define ROMCONV_MODEL_WALK_H

#include <PR/ultratypes.h>
#include "port_romconv.h"

/*
 * What owns each 4 bytes of the file. The first slot of an item holds its
 * kind; the item's other slots hold the kind with MODEL_INSIDE. A record's
 * kind is MODEL_RECORD plus its node's opcode. Unclaimed slots (MODEL_RAW)
 * are copied as the file has them, as the 32-bit path leaves them.
 */
enum ModelKind
{
    MODEL_RAW = 0,
    MODEL_SWITCH,         /* one switch slot */
    MODEL_TEXTURE,        /* one texture entry */
    MODEL_NODE,
    MODEL_VERTEX,         /* a render vertex block */
    MODEL_CVERTEX,        /* a collision vertex block */
    MODEL_POINTUSAGE,     /* an even number of half-words */
    MODEL_POINTUSAGE_ODD, /* an odd number: the last slot's second half is not one */
    MODEL_TEXEL,          /* an inline texture's texels */
    MODEL_LIST,           /* a display list, to its end command */
    MODEL_RECORD = 32
};
#define MODEL_INSIDE 0x80u
#define MODEL_KIND(k) ((k) & 0x7Fu)

/** What a reference must name. */
enum ModelRef
{
    REF_NODE,
    REF_RECORD,           /* the record of the opcode given */
    REF_NODE_OR_RECORD,
    REF_ANY,              /* the start of any item */
    REF_LIST,
    REF_VERTEX,           /* a vertex block, or raw bytes (a block of no vertices) */
    REF_CVERTEX,
    REF_POINTUSAGE,
    REF_TEXEL,            /* anywhere in an inline texture's texels */
    REF_TEXTURE,          /* a texture entry */
    REF_UNCONVERTED,      /* must be 0: nothing converts what it names */
    REF_VERTEX_LOAD       /* a G_VTX source: anywhere in a vertex block */
};

/** The list calls a walk follows inside one another, as romfile_model.c's swapDL. */
#define MODEL_LIST_DEPTH 32

struct ModelWalk
{
    const u8 *file;
    u32 size;
    u32 numSwitches;
    u32 numTextures;
    u32 root;         /* the root node's offset: the switch slots and texture entries come first */
    const char *name; /* the file, for refusals */
    u8 *kinds;        /* one per 4 bytes of the file */
    u32 slots;
    struct PortConvMap map; /* each item's new offset; every slot of a data block and of raw bytes */
    u32 nodes;              /* nodes claimed */
    u32 geometry;           /* vertex blocks and lists claimed */
    u32 vertices;           /* render and collision vertices claimed, as romfile_model.c counts them */
    u32 lists;              /* display lists claimed, the lists they call included */
    u32 opcodes[256];       /* nodes claimed, by opcode (the census) */
    u32 total;              /* the output's bytes, once laid out */
    u8 *out;                /* NULL while checking */
    u32 listPath[MODEL_LIST_DEPTH + 1]; /* the lists being claimed, outermost first */
    u32 listDepth;
};

/** One item of the file as the layout sees it. */
struct ModelItem
{
    u32 at;       /* file offset */
    u32 bytes;    /* its bytes in the file (a point-usage block's last slot whole) */
    u32 outBytes; /* its bytes in the output */
    u8 kind;
};

#define modelRefuse(w, ...) portConvRefuse(PORT_LP64_MODELS, (w)->name, __VA_ARGS__)
#define modelBe32(w, at) portConvBe32((w)->file + (at))
#define modelBe16(w, at) portConvBe16((w)->file + (at))
#define MODEL_SEGMENT 0x05000000u
#define MODEL_ROM_COMMAND 8u /* a ROM display-list command's bytes */

/** The name of an item of KIND, for refusals. */
const char *portModelKindName(u8 kind);

/** Gives W its owner slots, all unclaimed; refuses when they cannot be had. */
int portModelKindsInit(struct ModelWalk *w);

/** Claims every item of W's file (romconv_model_walk.c); reads only the file. */
int portModelClaim(struct ModelWalk *w);

/** The bytes of opcode OPCODE's record in the file (romfile_model.c's
 * modelRecordSize), or 0 for an opcode that has none. */
u32 portModelRecordFileSize(u32 opcode);

/** The bytes of opcode OPCODE's record in the output: its C record's size,
 * or the file's when that is larger (the head placeholder). */
u32 portModelRecordOutSize(u32 opcode);

/**
 * Looks the reference VALUE up as REF (OPCODE for REF_RECORD) and gives its
 * new offset in *TO (0 for a VALUE of 0). Refuses, naming WHAT at file offset
 * WHERE, a value that is neither 0 nor a segment-5 offset and one that names
 * no item of that kind.
 */
int portModelResolve(const struct ModelWalk *w, u32 value, enum ModelRef ref, u32 opcode, const char *what, u32 where,
        u32 *to);

/**
 * Follows the nodes as the game's promotion does (model.c's
 * modelPromoteNodeOffsetsToPointers: Child, with a LOD's Affects, then Next,
 * then up through Parent) and refuses links that name no node or that would
 * keep that walk from ending: more than two steps a node.
 */
int portModelCheckPromoteWalk(const struct ModelWalk *w);

/**
 * Checks (W->out NULL) or writes the item ITEM at TO in W->out. Each reads the
 * file only; while checking, each refuses what writing would find wrong, so
 * writing refuses nothing.
 */
int portModelWriteSwitch(const struct ModelWalk *w, const struct ModelItem *item, u32 to);
int portModelWriteTexture(const struct ModelWalk *w, const struct ModelItem *item, u32 to);
int portModelWriteNode(const struct ModelWalk *w, const struct ModelItem *item, u32 to);
int portModelWriteRecord(const struct ModelWalk *w, const struct ModelItem *item, u32 to);
int portModelWriteBlock(const struct ModelWalk *w, const struct ModelItem *item, u32 to);
int portModelWriteList(const struct ModelWalk *w, const struct ModelItem *item, u32 to);

#endif
