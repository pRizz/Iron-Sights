/**
 * The model file converter's item writers (port_romconv.h; LP64-004 Task 10):
 * one field map for each record kind, and the writers of switch slots,
 * texture entries, nodes, records and the vertex, point-usage and texel
 * blocks. romconv_model.c lays the items out and writes the display lists.
 *
 * A field map is romfile_model.c's (:47-65 and its OP05, OP07 and OP11
 * cases), with each field's place in the C record beside its place in the
 * file: at 32 bits the two are equal, so the output is the in-place result
 * byte for byte, including that path's handling of the opcodes the ROM lacks
 * (opcode 17 read as a group record, OP11's last two half-words and the
 * OP05 and OP07 data bytes swapped as words); at 64 bits opcode 17 has its own
 * map instead (LP64-006 Task 16). Compiled only at 64 bits and in
 * the 32-bit oracle build.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "bondconstants.h"
#include "platform_info.h"
#include "port_romconv.h"
#include "port_romlayout.h"
#include "romconv_model_walk.h"

enum FieldKind
{
    FIELD_SWAP, /* a big-endian scalar, written native and zero-extended to the C field's width */
    FIELD_RAW,  /* bytes copied as they are */
    FIELD_REF   /* FIELD_REF + enum ModelRef: a reference, written 0x05000000 plus the new offset */
};

/* COUNT elements of WIDTH bytes at FROM in the file, each written at TO +
 * i * TOWIDTH in the C record. */
struct ModelField
{
    u16 from;
    u16 to;
    u8 count;
    u8 width;
    u8 towidth;
    u8 kind;
};

#define FSIZE(T, f) sizeof(((T *) 0)->f)
#define SWAP(R, rf, C, cf) {offsetof(R, rf), offsetof(C, cf), 1, FSIZE(R, rf), FSIZE(C, cf), FIELD_SWAP}
#define RUN(R, rf, C, cf, n, w) {offsetof(R, rf), offsetof(C, cf), n, w, w, FIELD_SWAP}
#define REF(R, rf, C, cf, ref) {offsetof(R, rf), offsetof(C, cf), 1, 4, FSIZE(C, cf), FIELD_REF + (ref)}
#define RAW(R, rf, to, n) {offsetof(R, rf), to, 1, n, n, FIELD_RAW}

typedef ModelRoData_HeaderRecord CHeader;
typedef ModelRoData_GroupRecord CGroup;
typedef ModelRoData_DisplayListRecord CDL;
typedef ModelRoData_DisplayList_CollisionRecord CDLCol;
typedef ModelRoData_DisplayListPrimaryRecord CDLPrim;
typedef ModelRoData_LODRecord CLOD;
typedef ModelRoData_BSPRecord CBSP;
typedef ModelRoData_BoundingBoxRecord CBBox;
typedef ModelRoData_GunfireRecord CGunfire;
typedef ModelRoData_ShadowRecord CShadow;
typedef ModelRoData_SwitchRecord CSwitch;
typedef ModelRoData_Op06Record COp06;
typedef ModelRoData_GroupSimpleRecord CGroupSimple;
typedef ModelRoData_HeadPlaceholderRecord CHead;
typedef ModelRoData_InterlinkageRecord CInterlink;
typedef ModelRoData_Op11Record COp11;
typedef ModelRoData_Op05Record COp05;
typedef ModelRoData_Op07Record COp07;

static const struct ModelField kHeader[] = {
    SWAP(RomModelHeader, animpart, CHeader, AnimPart), SWAP(RomModelHeader, matrixindex, CHeader, MatrixIndex),
    REF(RomModelHeader, firstgroup, CHeader, FirstGroup, REF_NODE_OR_RECORD),
    SWAP(RomModelHeader, group1, CHeader, Group1), SWAP(RomModelHeader, group2, CHeader, Group2),
    SWAP(RomModelHeader, rwdataindex, CHeader, RwDataIndex), SWAP(RomModelHeader, reserved, CHeader, reserved)};
static const struct ModelField kGroup[] = {
    RUN(RomModelGroup, origin, CGroup, Origin, 3, 4), SWAP(RomModelGroup, jointid, CGroup, JointID),
    RUN(RomModelGroup, matrixids, CGroup, MatrixIDs, 3, 2),
    REF(RomModelGroup, childgroup, CGroup, ChildGroup, REF_NODE_OR_RECORD),
    SWAP(RomModelGroup, radius, CGroup, BoundingVolumeRadius)};
static const struct ModelField kDL[] = {
    REF(RomModelDL, primary, CDL, Primary, REF_LIST), REF(RomModelDL, secondary, CDL, Secondary, REF_LIST),
    SWAP(RomModelDL, baseaddr, CDL, BaseAddr), REF(RomModelDL, vertices, CDL, Vertices, REF_VERTEX),
    SWAP(RomModelDL, numvertices, CDL, numVertices), RAW(RomModelDL, modeltype, offsetof(CDL, ModelType), 1),
    RAW(RomModelDL, pad, offsetof(CDL, ModelType) + 1, 1)};
static const struct ModelField kDLCol[] = {
    REF(RomModelDLCollision, primary, CDLCol, Primary, REF_LIST),
    REF(RomModelDLCollision, secondary, CDLCol, Secondary, REF_LIST),
    REF(RomModelDLCollision, vertices, CDLCol, Vertices, REF_VERTEX),
    SWAP(RomModelDLCollision, numvertices, CDLCol, numVertices),
    SWAP(RomModelDLCollision, numcollisionvertices, CDLCol, numCollisionVertices),
    REF(RomModelDLCollision, collisionvertices, CDLCol, CollisionVertices, REF_CVERTEX),
    REF(RomModelDLCollision, pointusage, CDLCol, PointUsage, REF_POINTUSAGE),
    SWAP(RomModelDLCollision, modeltype, CDLCol, ModelType), SWAP(RomModelDLCollision, rwdataindex, CDLCol, RwDataIndex),
    SWAP(RomModelDLCollision, baseaddr, CDLCol, BaseAddr)};
static const struct ModelField kDLPrim[] = {
    SWAP(RomModelDLPrimary, numvertices, CDLPrim, numVertices),
    REF(RomModelDLPrimary, vertices, CDLPrim, Vertices, REF_VERTEX),
    REF(RomModelDLPrimary, primary, CDLPrim, Primary, REF_LIST), SWAP(RomModelDLPrimary, baseaddr, CDLPrim, BaseAddr)};
static const struct ModelField kLOD[] = {
    SWAP(RomModelLOD, mindistance, CLOD, MinDistance), SWAP(RomModelLOD, maxdistance, CLOD, MaxDistance),
    REF(RomModelLOD, affects, CLOD, Affects, REF_NODE), SWAP(RomModelLOD, rwdataindex, CLOD, RwDataIndex),
    SWAP(RomModelLOD, reserved, CLOD, reserved)};
static const struct ModelField kBSP[] = {
    RUN(RomModelBSP, point, CBSP, Point, 3, 4), RUN(RomModelBSP, vector, CBSP, Vector, 3, 4),
    REF(RomModelBSP, leftchild, CBSP, leftChild, REF_NODE), REF(RomModelBSP, rightchild, CBSP, rightChild, REF_NODE),
    SWAP(RomModelBSP, reserved, CBSP, reserved), SWAP(RomModelBSP, rwdataindex, CBSP, RwDataIndex)};
static const struct ModelField kBBox[] = {
    SWAP(RomModelBBox, modelnumber, CBBox, ModelNumber), RUN(RomModelBBox, bounds, CBBox, Bounds, 6, 4)};
static const struct ModelField kGunfire[] = {
    RUN(RomModelGunfire, offset, CGunfire, Offset, 3, 4), RUN(RomModelGunfire, size, CGunfire, Size, 3, 4),
    REF(RomModelGunfire, image, CGunfire, Image, REF_TEXTURE), SWAP(RomModelGunfire, scale, CGunfire, Scale),
    SWAP(RomModelGunfire, rwdataindex, CGunfire, RwDataIndex), SWAP(RomModelGunfire, reserved, CGunfire, reserved),
    SWAP(RomModelGunfire, baseaddr, CGunfire, BaseAddr)};
static const struct ModelField kShadow[] = {
    RUN(RomModelShadow, pos, CShadow, pos, 2, 4), RUN(RomModelShadow, size, CShadow, size, 2, 4),
    REF(RomModelShadow, image, CShadow, image, REF_TEXTURE),
    REF(RomModelShadow, header, CShadow, Header, REF_NODE_OR_RECORD), SWAP(RomModelShadow, scale, CShadow, Scale),
    SWAP(RomModelShadow, baseaddr, CShadow, BaseAddr)};
static const struct ModelField kSwitch[] = {
    REF(RomModelSwitch, controls, CSwitch, Controls, REF_NODE), SWAP(RomModelSwitch, rwdataindex, CSwitch, RwDataIndex),
    SWAP(RomModelSwitch, reserved, CSwitch, reserved)};
static const struct ModelField kOp06[] = {
    RUN(RomModelOp06, unk, COp06, unk00, 5, 4), SWAP(RomModelOp06, baseaddr, COp06, BaseAddr)};
static const struct ModelField kGroupSimple[] = {
    RUN(RomModelGroupSimple, origin, CGroupSimple, Origin, 3, 4), SWAP(RomModelGroupSimple, group1, CGroupSimple, Group1),
    SWAP(RomModelGroupSimple, group2, CGroupSimple, Group2),
    SWAP(RomModelGroupSimple, radius, CGroupSimple, BoundingVolumeRadius)};
/* The C record has the index alone; the file's other half-word is kept. */
static const struct ModelField kHead[] = {
    SWAP(RomModelHead, rwdataindex, CHead, RwDataIndex), RAW(RomModelHead, pad, sizeof(u16), 2)};
static const struct ModelField kInterlink[] = {
    RUN(RomModelInterlink, pos, CInterlink, pos, 3, 4), RUN(RomModelInterlink, pos2, CInterlink, pos2, 3, 4),
    SWAP(RomModelInterlink, scale, CInterlink, Scale)};
/* Words 0 to 2 are the position the game reads; 3 to 14 widen into their
 * pointer-sized slots. As romfile_model.c swaps the record as 19 words, the
 * two half-words at 0x44 are swapped as one word. */
static const struct ModelField kOp11[] = {
    {offsetof(RomModelOp11, unk0c), offsetof(COp11, pos), 3, 4, 4, FIELD_SWAP},
    {offsetof(RomModelOp11, unk0c[3]), offsetof(COp11, unk0c[3]), 12, 4, sizeof(uintptr_t), FIELD_SWAP},
    REF(RomModelOp11, unk0c[15], COp11, unk0c[15], REF_ANY), SWAP(RomModelOp11, radius, COp11, BoundingVolumeRadius),
    {offsetof(RomModelOp11, rwdataindex), offsetof(COp11, RwDataIndex), 1, 4, 4, FIELD_SWAP},
    SWAP(RomModelOp11, baseaddr, COp11, BaseAddr)};
/* The children, vertex and image arrays of opcodes 5 and 7 are not converted
 * (romfile_model.c leaves them as the file has them), so their references
 * must be 0; the data bytes are swapped as words, as there. */
static const struct ModelField kOp05[] = {
    SWAP(RomModelOp05, numchildren, COp05, NumChildren), REF(RomModelOp05, children, COp05, Children, REF_UNCONVERTED),
    REF(RomModelOp05, vertices, COp05, Vertices, REF_UNCONVERTED),
    REF(RomModelOp05, images, COp05, Images, REF_UNCONVERTED), RUN(RomModelOp05, data, COp05, Data, 100, 4),
    SWAP(RomModelOp05, unk1a0, COp05, unk1A0), SWAP(RomModelOp05, baseaddr, COp05, BaseAddr)};
static const struct ModelField kOp07[] = {
    REF(RomModelOp07, unk00, COp07, unk00, REF_NODE), REF(RomModelOp07, unk04, COp07, unk04, REF_NODE),
    SWAP(RomModelOp07, numchildren, COp07, NumChildren), REF(RomModelOp07, children, COp07, Children, REF_UNCONVERTED),
    REF(RomModelOp07, vertices, COp07, Vertices, REF_UNCONVERTED),
    REF(RomModelOp07, images, COp07, Images, REF_UNCONVERTED), RUN(RomModelOp07, data, COp07, Data, 100, 4),
    SWAP(RomModelOp07, unk1a8, COp07, unk1A8), SWAP(RomModelOp07, rwdataindex, COp07, RwDataIndex),
    SWAP(RomModelOp07, baseaddr, COp07, BaseAddr)};
#if IS_64_BIT
/* model.c reads opcode 17 as ModelRoData_Op17Record; the group map would swap
 * pos.y and pos.z as half-words and leave scale2 as the file has it. 32 bits
 * keeps the in-place pass's reading (decision 10; LP64-006 Task 16). */
typedef ModelRoData_Op17Record COp17;
static const struct ModelField kOp17[] = {
    SWAP(RomModelOp17, hitpart, COp17, hitpart), SWAP(RomModelOp17, radiussq, COp17, radiusSq),
    RUN(RomModelOp17, pos, COp17, pos, 3, 4), REF(RomModelOp17, othernode, COp17, othernode, REF_NODE),
    SWAP(RomModelOp17, scale1, COp17, scale1), SWAP(RomModelOp17, scale2, COp17, scale2)};
#endif

struct ModelRecordMap
{
    const struct ModelField *fields;
    u32 count;
    u32 fileBytes;
    u32 cBytes;
};

#define RECORD_MAP(f, R, C) {f, sizeof(f) / sizeof((f)[0]), sizeof(R), sizeof(C)}

/* One entry an opcode; an opcode with no record has none. */
static const struct ModelRecordMap kRecordMaps[MODELNODE_OPCODE_MAX] = {
    [MODELNODE_OPCODE_HEADER] = RECORD_MAP(kHeader, RomModelHeader, CHeader),
    [MODELNODE_OPCODE_GROUP] = RECORD_MAP(kGroup, RomModelGroup, CGroup),
    [MODELNODE_OPCODE_OP03] = RECORD_MAP(kGroup, RomModelGroup, CGroup),
    [MODELNODE_OPCODE_DL] = RECORD_MAP(kDL, RomModelDL, CDL),
    [MODELNODE_OPCODE_OP05] = RECORD_MAP(kOp05, RomModelOp05, COp05),
    [MODELNODE_OPCODE_OP06] = RECORD_MAP(kOp06, RomModelOp06, COp06),
    [MODELNODE_OPCODE_OP07] = RECORD_MAP(kOp07, RomModelOp07, COp07),
    [MODELNODE_OPCODE_LOD] = RECORD_MAP(kLOD, RomModelLOD, CLOD),
    [MODELNODE_OPCODE_BSP] = RECORD_MAP(kBSP, RomModelBSP, CBSP),
    [MODELNODE_OPCODE_BBOX] = RECORD_MAP(kBBox, RomModelBBox, CBBox),
    [MODELNODE_OPCODE_OP11] = RECORD_MAP(kOp11, RomModelOp11, COp11),
    [MODELNODE_OPCODE_GUNFIRE] = RECORD_MAP(kGunfire, RomModelGunfire, CGunfire),
    [MODELNODE_OPCODE_SHADOW] = RECORD_MAP(kShadow, RomModelShadow, CShadow),
    [MODELNODE_OPCODE_INTERLINK] = RECORD_MAP(kInterlink, RomModelInterlink, CInterlink),
#if IS_64_BIT
    [MODELNODE_OPCODE_OP17] = RECORD_MAP(kOp17, RomModelOp17, COp17),
#else
    [MODELNODE_OPCODE_OP17] = RECORD_MAP(kGroup, RomModelGroup, CGroup),
#endif
    [MODELNODE_OPCODE_SWITCH] = RECORD_MAP(kSwitch, RomModelSwitch, CSwitch),
    [MODELNODE_OPCODE_OP20] = RECORD_MAP(kHeader, RomModelHeader, CHeader),
    [MODELNODE_OPCODE_GROUPSIMPLE] = RECORD_MAP(kGroupSimple, RomModelGroupSimple, CGroupSimple),
    [MODELNODE_OPCODE_DLPRIMARY] = RECORD_MAP(kDLPrim, RomModelDLPrimary, CDLPrim),
    [MODELNODE_OPCODE_HEAD] = RECORD_MAP(kHead, RomModelHead, CHead),
    [MODELNODE_OPCODE_DLCOLLISION] = RECORD_MAP(kDLCol, RomModelDLCollision, CDLCol),
};

u32 portModelRecordFileSize(u32 opcode)
{
    return opcode < MODELNODE_OPCODE_MAX ? kRecordMaps[opcode].fileBytes : 0;
}

u32 portModelRecordOutSize(u32 opcode)
{
    const struct ModelRecordMap *m;

    if (opcode >= MODELNODE_OPCODE_MAX)
    {
        return 0;
    }
    m = &kRecordMaps[opcode];
    return m->cBytes > m->fileBytes ? m->cBytes : m->fileBytes;
}

/* Writes VALUE in native order in the WIDTH bytes at P (8: zero-extended). */
static void putNative(u8 *p, u32 value, u32 width)
{
    u8 b = (u8) value;
    u16 h = (u16) value;
    u64 d = value;

    if (width == 1)
    {
        memcpy(p, &b, 1);
    }
    else if (width == 2)
    {
        memcpy(p, &h, 2);
    }
    else if (width == 4)
    {
        memcpy(p, &value, 4);
    }
    else
    {
        memcpy(p, &d, 8);
    }
}

/* Applies the N fields of F to the file bytes at AT, writing at DST (NULL:
 * resolving references only). OPCODE names a node's record. */
static int applyFields(const struct ModelWalk *w, u32 at, u8 *dst, const struct ModelField *f, u32 n, u32 opcode,
        const char *what)
{
    u32 i, e;

    for (i = 0; i < n; i++)
    {
        for (e = 0; e < f[i].count; e++)
        {
            u32 from = at + f[i].from + e * f[i].width;
            u8 *to = dst != NULL ? dst + f[i].to + e * f[i].towidth : NULL;
            u32 value = f[i].width == 4 ? modelBe32(w, from) : f[i].width == 2 ? modelBe16(w, from) : w->file[from];

            if (f[i].kind >= FIELD_REF)
            {
                u32 moved;

                if (!portModelResolve(w, value, (enum ModelRef) (f[i].kind - FIELD_REF), opcode, what, from, &moved))
                {
                    return 0;
                }
                value = value != 0 ? MODEL_SEGMENT | moved : 0; /* 0 stays 0 */
            }
            if (to == NULL)
            {
                continue;
            }
            if (f[i].kind == FIELD_RAW)
            {
                memcpy(to, w->file + from, f[i].width);
            }
            else
            {
                putNative(to, value, f[i].towidth);
            }
        }
    }
    return 1;
}

static u8 *outAt(const struct ModelWalk *w, u32 to)
{
    return w->out != NULL ? w->out + to : NULL;
}

int portModelWriteSwitch(const struct ModelWalk *w, const struct ModelItem *item, u32 to)
{
    static const struct ModelField slot = {0, 0, 1, 4, sizeof(ModelNode *), FIELD_REF + REF_NODE};

    return applyFields(w, item->at, outAt(w, to), &slot, 1, 0, "switch slot");
}

int portModelWriteTexture(const struct ModelWalk *w, const struct ModelItem *item, u32 to)
{
    /* A texture number is copied; a segment-5 ID is an inline texture. */
    static const struct ModelField number[] = {
        SWAP(RomModelTexture, textureid, ModelFileTextures, TextureID),
        RAW(RomModelTexture, width, offsetof(ModelFileTextures, Width), 7),
        RAW(RomModelTexture, pad, offsetof(ModelFileTextures, tflags) + 1, 1)};
    static const struct ModelField inline_[] = {
        REF(RomModelTexture, textureid, ModelFileTextures, TextureID, REF_TEXEL),
        RAW(RomModelTexture, width, offsetof(ModelFileTextures, Width), 7),
        RAW(RomModelTexture, pad, offsetof(ModelFileTextures, tflags) + 1, 1)};
    int inl = (modelBe32(w, item->at) >> 24) == 0x05;

    return applyFields(w, item->at, outAt(w, to), inl ? inline_ : number, 3, 0, "texture entry");
}

int portModelWriteNode(const struct ModelWalk *w, const struct ModelItem *item, u32 to)
{
    static const struct ModelField fields[] = {
        SWAP(RomModelNode, opcode, ModelNode, Opcode),
        {offsetof(RomModelNode, pad), offsetof(ModelNode, Opcode) + 2, 1, 2, 2, FIELD_SWAP},
        REF(RomModelNode, data, ModelNode, Data, REF_RECORD), REF(RomModelNode, parent, ModelNode, Parent, REF_NODE),
        REF(RomModelNode, next, ModelNode, Next, REF_NODE), REF(RomModelNode, prev, ModelNode, Prev, REF_NODE),
        REF(RomModelNode, child, ModelNode, Child, REF_NODE)};

    return applyFields(w, item->at, outAt(w, to), fields, sizeof(fields) / sizeof(fields[0]),
            modelBe16(w, item->at) & 0xFFu, "node");
}

int portModelWriteRecord(const struct ModelWalk *w, const struct ModelItem *item, u32 to)
{
    u32 opcode = MODEL_KIND(item->kind) - MODEL_RECORD;
    const struct ModelRecordMap *m = &kRecordMaps[opcode];

    /* The game's promotion steps through the children array of opcodes 5
     * and 7 for NumChildren entries; that array is not converted. */
    if ((opcode == MODELNODE_OPCODE_OP05 || opcode == MODELNODE_OPCODE_OP07)
            && (s32) modelBe32(w, item->at + (opcode == MODELNODE_OPCODE_OP05 ? offsetof(RomModelOp05, numchildren)
                                                                              : offsetof(RomModelOp07, numchildren)))
                       > 0)
    {
        modelRefuse(w, "the opcode %u record at 0x%X has children, which are not converted", opcode, item->at);
        return 0;
    }
    return applyFields(w, item->at, outAt(w, to), m->fields, m->count, opcode, "record");
}

/* The six half-words of each vertex swapped and its last four bytes kept. At
 * 64 bits a collision vertex's link becomes its target node's new offset and
 * the related index is swapped; at 32 bits both stay as romfile_model.c leaves
 * them, which the game's promotion then fixes. Links are checked at both widths. */
static int writeVertices(const struct ModelWalk *w, const struct ModelItem *item, u8 *dst, int collision)
{
    u32 v, h;

    for (v = 0; v < item->bytes / sizeof(RomModelVertex); v++)
    {
        u32 at = item->at + v * (u32) sizeof(RomModelVertex);
        u32 link = modelBe32(w, at + offsetof(RomModelVertex, link)), moved = 0;
        u8 *p = dst != NULL ? dst + v * sizeof(Vertex) : NULL;

        if (collision && link != 0)
        {
            if (!portModelResolve(w, link, REF_NODE, 0, "collision vertex", at, &moved))
            {
                return 0;
            }
            if (moved == 0)
            {
                modelRefuse(w, "the collision vertex at 0x%X links to the node at new offset 0, which reads as no link",
                        at);
                return 0;
            }
        }
        if (p == NULL)
        {
            continue;
        }
        for (h = 0; h < offsetof(RomModelVertex, tail); h += 2)
        {
            putNative(p + h, modelBe16(w, at + h), 2);
        }
        memcpy(p + offsetof(RomModelVertex, tail), w->file + at + offsetof(RomModelVertex, tail), 4);
#if IS_64_BIT
        if (collision && link != 0)
        {
            putNative(p + offsetof(Vertex, LinkOffset), moved, sizeof(u32));
            putNative(p + offsetof(Vertex, CollisionRelatedIndex),
                    modelBe16(w, at + offsetof(RomModelVertex, tail)), sizeof(s16));
        }
#endif
    }
    return 1;
}

/* Swaps the block's first UNITS half-words or words (WIDTH 2 or 4), keeping
 * the rest. */
static void writeSwapped(const struct ModelWalk *w, const struct ModelItem *item, u8 *dst, u32 units, u32 width)
{
    u32 i;

    for (i = 0; i < units; i++)
    {
        u32 at = item->at + i * width;

        putNative(dst + i * width, width == 4 ? modelBe32(w, at) : modelBe16(w, at), width);
    }
    memcpy(dst + units * width, w->file + item->at + units * width, item->bytes - units * width);
}

int portModelWriteBlock(const struct ModelWalk *w, const struct ModelItem *item, u32 to)
{
    u8 *dst = outAt(w, to);
    u32 kind = MODEL_KIND(item->kind);

    if (kind == MODEL_VERTEX || kind == MODEL_CVERTEX)
    {
        return writeVertices(w, item, dst, kind == MODEL_CVERTEX);
    }
    if (dst == NULL)
    {
        return 1; /* point usage and texels hold no reference */
    }
    if (kind == MODEL_TEXEL)
    {
        writeSwapped(w, item, dst, item->bytes / 4, 4);
    }
    else
    {
        /* An odd count's block is claimed to the next slot, which the file's
         * end may cut short. */
        writeSwapped(w, item, dst, (item->bytes + 3) / 4 * 2 - (kind == MODEL_POINTUSAGE_ODD), 2);
    }
    return 1;
}
