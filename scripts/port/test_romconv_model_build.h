/* The model file converter's fixture builder (LP64-004 Task 10): builds a
 * big-endian model file from calls that return each item's offset, so the
 * tests name things by what they are. Every byte is written here; nothing is
 * read from a ROM. Shared by test_romconv_model.c and
 * test_romconv_model_checks.c. */
#ifndef TEST_ROMCONV_MODEL_BUILD_H
#define TEST_ROMCONV_MODEL_BUILD_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "bondconstants.h"
#include "platform_info.h"
#include "port_gfxwords.h"
#include "port_romconv.h"

#define MF_CAP 0x1000u
#define MF_OUT_CAP (3u * MF_CAP)
#define SEG(off) (0x05000000u | (u32) (off))

/* The game's <string.h>, first on the include path, declares no memcmp; the
 * compiler's own are called by name. */
#define sameBytes(a, b, size) (__builtin_memcmp((a), (b), (size)) == 0)
#define contains(text, part) (__builtin_strstr((text), (part)) != NULL)

static u8 g_File[MF_CAP];
static u32 g_Size;
static u8 g_Out[MF_OUT_CAP] __attribute__((aligned(16)));
static char g_Log[512];

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(g_Log, sizeof(g_Log), fmt, ap);
    va_end(ap);
}

static inline int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_model: FAIL: %s (log: %s)\n", what, g_Log);
        fflush(stdout); /* before a sanitizer stops a later test */
        return 1;
    }
    return 0;
}

static inline int allBytes(const u8 *p, size_t size, u8 value)
{
    size_t i;

    for (i = 0; i < size && p[i] == value; i++)
    {
    }
    return i == size;
}

static inline void mfPut8(u32 at, u32 v) { g_File[at] = (u8) v; }
static inline void mfPut16(u32 at, u32 v) { g_File[at] = (u8) (v >> 8); g_File[at + 1] = (u8) v; }
static inline void mfPut32(u32 at, u32 v) { mfPut16(at, v >> 16); mfPut16(at + 2, v & 0xFFFF); }
static inline u32 mfGet32(u32 at)
{
    return ((u32) g_File[at] << 24) | ((u32) g_File[at + 1] << 16) | ((u32) g_File[at + 2] << 8) | g_File[at + 3];
}
static inline u32 mfGet16(u32 at) { return ((u32) g_File[at] << 8) | g_File[at + 1]; }

/** An empty file with SWITCHES slots and TEXTURES entries, all zero; g_Out 0xAA. */
static inline void mfBegin(u32 switches, u32 textures)
{
    __builtin_memset(g_File, 0, sizeof(g_File));
    __builtin_memset(g_Out, 0xAA, sizeof(g_Out));
    g_Log[0] = '\0';
    g_Size = switches * 4 + textures * 12;
}

/** BYTES of zeros at the end of the file, at a multiple of 4. */
static inline u32 mfAlloc(u32 bytes)
{
    u32 at = (g_Size + 3) & ~3u;

    g_Size = at + bytes;
    return at;
}

/** A node of OPCODE; its links are set with mfNodeLink. */
static inline u32 mfNode(u32 opcode)
{
    u32 at = mfAlloc(24);

    mfPut16(at, opcode);
    return at;
}

enum { LINK_DATA = 4, LINK_PARENT = 8, LINK_NEXT = 12, LINK_PREV = 16, LINK_CHILD = 20 };
static inline void mfNodeLink(u32 node, u32 field, u32 target) { mfPut32(node + field, SEG(target)); }

/** NODE's record of BYTES, zero; returns its offset. */
static inline u32 mfRecord(u32 node, u32 bytes)
{
    u32 at = mfAlloc(bytes);

    mfNodeLink(node, LINK_DATA, at);
    return at;
}

/** CHILD under PARENT after PREV (0 for the first child). */
static inline void mfAdopt(u32 parent, u32 prev, u32 child)
{
    mfNodeLink(child, LINK_PARENT, parent);
    if (prev == 0)
    {
        mfNodeLink(parent, LINK_CHILD, child);
        return;
    }
    mfNodeLink(prev, LINK_NEXT, child);
    mfNodeLink(child, LINK_PREV, prev);
}

/** N vertices, vertex i's six half-words FIRST + 16i + 0..5 and its colour
 * bytes 0xC0 + i, 1, 2, 3. */
static inline u32 mfVertices(u32 n, u32 first)
{
    u32 at = mfAlloc(n * 16), i, h;

    for (i = 0; i < n; i++)
    {
        for (h = 0; h < 6; h++)
        {
            mfPut16(at + i * 16 + h * 2, first + i * 16 + h);
        }
        mfPut8(at + i * 16 + 12, 0xC0 + i);
        mfPut8(at + i * 16 + 13, 1);
        mfPut8(at + i * 16 + 14, 2);
        mfPut8(at + i * 16 + 15, 3);
    }
    return at;
}

/** A display list of N commands, WORDS[2i] and WORDS[2i + 1] each. */
static inline u32 mfList(const u32 *words, u32 n)
{
    u32 at = mfAlloc(n * 8), i;

    for (i = 0; i < 2 * n; i++)
    {
        mfPut32(at + i * 4, words[i]);
    }
    return at;
}

/** Texture entry I of a file with SWITCHES slots. */
static inline void mfTexture(u32 switches, u32 i, u32 id, u32 width, u32 height, u32 depth)
{
    u32 at = switches * 4 + i * 12;

    mfPut32(at, id);
    mfPut8(at + 4, width);
    mfPut8(at + 5, height);
    mfPut8(at + 6, 0x61);
    mfPut8(at + 7, 0x62);
    mfPut8(at + 8, depth);
    mfPut8(at + 9, 0x63);
    mfPut8(at + 10, 0x64);
    mfPut8(at + 11, 0x65);
}

/* ==== the main fixture ================================================== */

/* Every item of the main file, by what it is. */
struct MainFile
{
    u32 header, headerRec, group, groupRec, dl, dlRec, dlVerts, dlList;
    u32 col, colRec, colVerts, colCVerts, colPU, colList, prim, primRec, primVerts, primList;
    u32 lod, lodRec, sw, swRec, head, headRec, bsp, bspRec, box1, box1Rec, box2, box2Rec;
    u32 gun, gunRec, shadow, shadowRec, gs, gsRec, il, ilRec, gap, texels;
};

#define MAIN_SWITCHES 2
#define MAIN_TEXTURES 3

/** Fills record bytes [FROM, TO) of REC with BASE + i. */
static inline void mfPattern(u32 rec, u32 from, u32 to, u32 base)
{
    for (; from < to; from++)
    {
        mfPut8(rec + from, base + from);
    }
}

/** Two switch slots, three textures (the third inline), a header whose first
 * group is a group; under the group a DL, a DLCOLLISION, a DLPRIMARY, a LOD
 * whose Affects is a switch (controlling a head placeholder), a BSP with two
 * bounding boxes, a gunfire and a shadow naming texture 1, a simple group and
 * an interlink; 8 bytes nothing owns; the inline texels last. */
static inline void buildMain(struct MainFile *m)
{
    mfBegin(MAIN_SWITCHES, MAIN_TEXTURES);
    m->header = mfNode(MODELNODE_OPCODE_HEADER);
    m->headerRec = mfRecord(m->header, 16);
    m->group = mfNode(MODELNODE_OPCODE_GROUP);
    m->groupRec = mfRecord(m->group, 28);
    m->dl = mfNode(MODELNODE_OPCODE_DL);
    m->dlRec = mfRecord(m->dl, 20);
    m->dlVerts = mfVertices(4, 0x100);
    m->dlList = mfAlloc(32);
    m->col = mfNode(MODELNODE_OPCODE_DLCOLLISION);
    m->colRec = mfRecord(m->col, 32);
    m->colVerts = mfVertices(2, 0x200);
    m->colCVerts = mfVertices(2, 0x300);
    m->colPU = mfAlloc(4);
    {
        static const u32 words[] = {0x04100020, 0x04000000, 0xB8000000, 0};
        m->colList = mfList(words, 2); /* a segment-4 G_VTX, copied */
    }
    m->prim = mfNode(MODELNODE_OPCODE_DLPRIMARY);
    m->primRec = mfRecord(m->prim, 16);
    m->primVerts = mfVertices(4, 0x400);
    {
        static const u32 words[] = {0xB8000000, 0};
        m->primList = mfList(words, 1);
    }
    m->lod = mfNode(MODELNODE_OPCODE_LOD);
    m->lodRec = mfRecord(m->lod, 16);
    m->sw = mfNode(MODELNODE_OPCODE_SWITCH);
    m->swRec = mfRecord(m->sw, 8);
    m->head = mfNode(MODELNODE_OPCODE_HEAD);
    m->headRec = mfRecord(m->head, 4);
    m->bsp = mfNode(MODELNODE_OPCODE_BSP);
    m->bspRec = mfRecord(m->bsp, 36);
    m->box1 = mfNode(MODELNODE_OPCODE_BBOX);
    m->box1Rec = mfRecord(m->box1, 28);
    m->box2 = mfNode(MODELNODE_OPCODE_BBOX);
    m->box2Rec = mfRecord(m->box2, 28);
    m->gun = mfNode(MODELNODE_OPCODE_GUNFIRE);
    m->gunRec = mfRecord(m->gun, 40);
    m->shadow = mfNode(MODELNODE_OPCODE_SHADOW);
    m->shadowRec = mfRecord(m->shadow, 32);
    m->gs = mfNode(MODELNODE_OPCODE_GROUPSIMPLE);
    m->gsRec = mfRecord(m->gs, 20);
    m->il = mfNode(MODELNODE_OPCODE_INTERLINK);
    m->ilRec = mfRecord(m->il, 28);
    m->gap = mfAlloc(8);
    __builtin_memset(g_File + m->gap, 0x5A, 8);
    m->texels = mfAlloc(16);
    mfPattern(m->texels, 0, 16, 0x80);

    /* the tree */
    mfAdopt(m->header, 0, m->group);
    mfAdopt(m->group, 0, m->dl);
    mfAdopt(m->group, m->dl, m->col);
    mfAdopt(m->group, m->col, m->prim);
    mfAdopt(m->group, m->prim, m->lod);
    mfAdopt(m->group, m->lod, m->bsp);
    mfAdopt(m->group, m->bsp, m->gun);
    mfAdopt(m->group, m->gun, m->shadow);
    mfAdopt(m->group, m->shadow, m->gs);
    mfAdopt(m->group, m->gs, m->il);
    mfAdopt(m->lod, 0, m->sw);
    mfAdopt(m->sw, 0, m->head);
    mfAdopt(m->bsp, 0, m->box1);
    mfAdopt(m->bsp, m->box1, m->box2);

    /* the head: switch 0 names the DL node; textures 0x55, 0x123 and inline */
    mfPut32(0, SEG(m->dl));
    mfTexture(MAIN_SWITCHES, 0, 0x55, 16, 16, 2);
    mfTexture(MAIN_SWITCHES, 1, 0x123, 32, 64, 2);
    mfTexture(MAIN_SWITCHES, 2, SEG(m->texels), 4, 4, 1);

    /* the records: scalars from a counting pattern, references set after */
    mfPattern(m->headerRec, 0, 16, 0x10);
    mfPut32(m->headerRec + 4, SEG(m->group));
    mfPattern(m->groupRec, 0, 28, 0x20);
    mfPut32(m->groupRec + 0x14, SEG(m->dl));
    mfPut32(m->dlRec + 0, SEG(m->dlList));
    mfPut32(m->dlRec + 8, 0x00001234);
    mfPut32(m->dlRec + 0xC, SEG(m->dlVerts));
    mfPut16(m->dlRec + 0x10, 4);
    mfPut8(m->dlRec + 0x12, 3);
    mfPut8(m->dlRec + 0x13, 0x77);
    {
        u32 words[] = {0x04100020, SEG(m->dlVerts + 2 * 16), 0xC0000000, 0x00550000, 0xFD100000, SEG(m->texels),
            0xB8000000, 0};
        u32 i;
        for (i = 0; i < 8; i++)
        {
            mfPut32(m->dlList + i * 4, words[i]);
        }
    }
    mfPut32(m->colRec + 0, SEG(m->colList));
    mfPut32(m->colRec + 8, SEG(m->colVerts));
    mfPut16(m->colRec + 0xC, 2);
    mfPut16(m->colRec + 0xE, 2);
    mfPut32(m->colRec + 0x10, SEG(m->colCVerts));
    mfPut32(m->colRec + 0x14, SEG(m->colPU));
    mfPut16(m->colRec + 0x18, 0x0405);
    mfPut16(m->colRec + 0x1A, 0x0607);
    mfPut32(m->colRec + 0x1C, 0x00002345);
    mfPut16(m->colPU, 0x1112);
    mfPut16(m->colPU + 2, 0x1314);
    mfPut32(m->colCVerts + 8, 0); /* a collision vertex's s and t are its link: the first has none */
    mfPut32(m->colCVerts + 16 + 8, SEG(m->col)); /* the second links to its node */
    mfPut16(m->colCVerts + 16 + 12, 0x0102);
    mfPut32(m->primRec + 0, 1); /* one quad */
    mfPut32(m->primRec + 4, SEG(m->primVerts));
    mfPut32(m->primRec + 8, SEG(m->primList));
    mfPut32(m->primRec + 0xC, 0x00003456);
    mfPattern(m->lodRec, 0, 16, 0x30);
    mfPut32(m->lodRec + 8, SEG(m->sw));
    mfPut32(m->swRec, SEG(m->head));
    mfPattern(m->swRec, 4, 8, 0x38);
    mfPattern(m->headRec, 0, 4, 0x3C);
    mfPattern(m->bspRec, 0, 36, 0x40);
    mfPut32(m->bspRec + 0x18, SEG(m->box1));
    mfPut32(m->bspRec + 0x1C, SEG(m->box2));
    mfPattern(m->box1Rec, 0, 28, 0x50);
    mfPattern(m->box2Rec, 0, 28, 0x58);
    mfPattern(m->gunRec, 0, 40, 0x60);
    mfPut32(m->gunRec + 0x18, SEG(MAIN_SWITCHES * 4 + 12)); /* texture 1 */
    mfPattern(m->shadowRec, 0, 32, 0x70);
    mfPut32(m->shadowRec + 0x10, SEG(MAIN_SWITCHES * 4 + 12));
    mfPut32(m->shadowRec + 0x14, SEG(m->header));
    mfPattern(m->gsRec, 0, 20, 0x90);
    mfPattern(m->ilRec, 0, 28, 0xA0);
    g_Size = (g_Size + 7) & ~7u;
}

/* ==== reading the output ================================================ */

/** The offset in OUT that a converted reference WORD names. */
static inline u32 refOffset(uintptr_t word) { return (u32) (word & 0x00FFFFFFu); }
static inline void *at(u8 *out, uintptr_t word) { return out + refOffset(word); }
static inline u32 off_of(u8 *out, const void *p) { return (u32) ((const u8 *) p - out); }
static inline ModelNode *rootOf(u8 *out, u32 switches, u32 textures)
{
    return (ModelNode *) (out + switches * sizeof(ModelNode *) + textures * sizeof(ModelFileTextures));
}
static inline void *record_of(u8 *out, const ModelNode *node) { return at(out, (uintptr_t) node->Data); }
static inline ModelNode *child_of(u8 *out, const ModelNode *node) { return (ModelNode *) at(out, (uintptr_t) node->Child); }
static inline ModelNode *next_of(u8 *out, const ModelNode *node) { return (ModelNode *) at(out, (uintptr_t) node->Next); }
static inline Gfx *list_of(u8 *out, const void *word) { return (Gfx *) at(out, (uintptr_t) word); }
static inline Vertex *vertices_of(u8 *out, const ModelRoData_DisplayListRecord *dl)
{
    return (Vertex *) at(out, (uintptr_t) dl->Vertices);
}
/** Child number I (0 the first) of NODE. */
static inline ModelNode *nth_child(u8 *out, const ModelNode *node, u32 i)
{
    ModelNode *n = child_of(out, node);

    while (i-- > 0)
    {
        n = next_of(out, n);
    }
    return n;
}

/** The size measured for g_File, then converted into g_Out at exactly that size. */
static inline u32 convertFile(u32 switches, u32 textures)
{
    u32 n = portModelConvertedSize(g_File, g_Size, (s32) switches, (s32) textures, "Ptest");

    if (n == 0 || n > MF_OUT_CAP || portModelConvert(g_File, g_Size, g_Out, n, (s32) switches, (s32) textures, "Ptest") != 1)
    {
        return 0;
    }
    return n;
}

/** g_File as changed must be refused by both functions, nothing written to
 * g_Out, and the line must name WHY. */
static inline int expectRefused(u32 switches, u32 textures, const char *why, const char *what)
{
    u32 n;
    s32 ok;

    __builtin_memset(g_Out, 0xAA, sizeof(g_Out));
    n = portModelConvertedSize(g_File, g_Size, (s32) switches, (s32) textures, "Ptest");
    ok = portModelConvert(g_File, g_Size, g_Out, MF_OUT_CAP, (s32) switches, (s32) textures, "Ptest");
    return expect(n == 0 && ok == 0 && allBytes(g_Out, sizeof(g_Out), 0xAA)
            && contains(g_Log, "ge: lp64: cannot convert models \"Ptest\": ") && contains(g_Log, why), what);
}

#endif
