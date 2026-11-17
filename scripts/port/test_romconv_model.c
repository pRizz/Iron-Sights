/* The model file converter (LP64-004 Task 10): portModelConvert writes the
 * run-time image of a big-endian model file, every item at its C size and
 * every reference remapped; portModelConvertedSize measures it first. Built
 * and run at each pointer width by test_romconv.sh; at 32 bits every image is
 * also compared with romfile_model.c's in-place result. Every input is built
 * here (test_romconv_model_build.h). */
#include "test_romconv_model_build.h"

/* The bits of the f32 at P. */
static u32 bits(const void *p)
{
    u32 v;

    __builtin_memcpy(&v, p, 4);
    return v;
}

static int testBrief(void)
{
    // Arrange
    struct MainFile m;
    u8 *out = g_Out;
    u32 n;
    uintptr_t *sw;
    ModelFileTextures *tex;
    ModelNode *root, *node_dl;
    ModelRoData_DisplayListRecord *dl;
    Gfx *cmd;
    buildMain(&m);

    // Act
    n = portModelConvertedSize(g_File, g_Size, 2, 3, "Ptest");
    if (expect(n % 8 == 0 && n > 0 && n <= MF_OUT_CAP && portModelConvert(g_File, g_Size, out, n, 2, 3, "Ptest") == 1,
                "the main file converts at its measured size"))
    {
        return 1;
    }
    sw = (uintptr_t *) out;
    tex = (ModelFileTextures *) (out + 2 * sizeof(void *));
    root = (ModelNode *) (tex + 3);
    node_dl = nth_child(out, child_of(out, root), 0);
    dl = record_of(out, node_dl);
    cmd = list_of(out, dl->Primary);

    // Assert
    return expect(sw[0] == (0x05000000u | off_of(out, node_dl)) && sw[1] == 0
            && tex[1].TextureID == 0x123 && tex[1].Width == 32 && tex[1].Height == 64 && (tex[2].TextureID >> 24) == 0x05
            && (root->Opcode & 0xff) == MODELNODE_OPCODE_HEADER && dl->numVertices == 4 && GFX_OP(&cmd[0]) == 0x04
            && cmd[0].words.w1 == (0x05000000u | (off_of(out, vertices_of(out, dl)) + 2 * 16))
            && GFX_OP(&cmd[3]) == 0xB8 && cmd[3].words.w1 == 0,
            "the brief's assertions");
}

static int testWritesExactlyTheMeasuredBytes(void)
{
    // Arrange
    struct MainFile m;
    u32 n;
    buildMain(&m);

    // Act
    n = convertFile(MAIN_SWITCHES, MAIN_TEXTURES);

    // Assert
    return expect(n != 0 && allBytes(g_Out + n, sizeof(g_Out) - n, 0xAA)
            && (u8 *) rootOf(g_Out, MAIN_SWITCHES, MAIN_TEXTURES) - g_Out
                   == MAIN_SWITCHES * (long) sizeof(void *) + MAIN_TEXTURES * (long) sizeof(ModelFileTextures),
            "nothing past the measured bytes is written, and the root follows the head");
}

static int testMatchesInPlaceAt32Bits(void)
{
    struct MainFile m;
    u32 n;

    buildMain(&m);
    n = convertFile(MAIN_SWITCHES, MAIN_TEXTURES);
    return expect(n != 0 && sameAsInPlace(MAIN_SWITCHES, MAIN_TEXTURES, n), "at 32 bits the image is the in-place one");
}

/* The converted main file's nodes, by what they are. */
struct MainOut
{
    ModelNode *header, *group, *dl, *col, *prim, *lod, *sw, *head, *bsp, *box1, *box2, *gun, *shadow, *gs, *il;
};

static int convertMain(struct MainFile *m, struct MainOut *o)
{
    buildMain(m);
    if (convertFile(MAIN_SWITCHES, MAIN_TEXTURES) == 0)
    {
        return 0;
    }
    o->header = rootOf(g_Out, MAIN_SWITCHES, MAIN_TEXTURES);
    o->group = child_of(g_Out, o->header);
    o->dl = nth_child(g_Out, o->group, 0);
    o->col = nth_child(g_Out, o->group, 1);
    o->prim = nth_child(g_Out, o->group, 2);
    o->lod = nth_child(g_Out, o->group, 3);
    o->bsp = nth_child(g_Out, o->group, 4);
    o->gun = nth_child(g_Out, o->group, 5);
    o->shadow = nth_child(g_Out, o->group, 6);
    o->gs = nth_child(g_Out, o->group, 7);
    o->il = nth_child(g_Out, o->group, 8);
    o->sw = child_of(g_Out, o->lod);
    o->head = child_of(g_Out, o->sw);
    o->box1 = nth_child(g_Out, o->bsp, 0);
    o->box2 = nth_child(g_Out, o->bsp, 1);
    return 1;
}

#define NODE_REF(node) ((uintptr_t) (0x05000000u | off_of(g_Out, (node))))

static int testNodes(void)
{
    struct MainFile m;
    struct MainOut o;

    return expect(convertMain(&m, &o) && (o.col->Opcode & 0xff) == MODELNODE_OPCODE_DLCOLLISION
            && (uintptr_t) o.col->Parent == NODE_REF(o.group) && (uintptr_t) o.col->Prev == NODE_REF(o.dl)
            && (uintptr_t) o.col->Next == NODE_REF(o.prim) && o.dl->Prev == NULL && o.il->Next == NULL
            && o.header->Parent == NULL && (o.head->Opcode & 0xff) == MODELNODE_OPCODE_HEAD,
            "every node link is the new offset of its node, and 0 stays 0");
}

static int testHeaderAndGroup(void)
{
    struct MainFile m;
    struct MainOut o;
    ModelRoData_HeaderRecord *h;
    ModelRoData_GroupRecord *g;

    if (!convertMain(&m, &o))
    {
        return expect(0, "the main file converts");
    }
    h = record_of(g_Out, o.header);
    g = record_of(g_Out, o.group);
    return expect(h->AnimPart == 0x1011 && h->RwDataIndex == 0x1C1D && (uintptr_t) h->FirstGroup == NODE_REF(o.group)
            && bits(&g->Origin.x) == 0x20212223 && g->JointID == 0x2C2D && g->MatrixID2 == 0x3233
            && bits(&g->BoundingVolumeRadius) == 0x38393A3B && (uintptr_t) g->ChildGroup == NODE_REF(o.dl),
            "the header's and the group's scalars and references");
}

static int testDisplayListRecords(void)
{
    struct MainFile m;
    struct MainOut o;
    ModelRoData_DisplayListRecord *dl;
    ModelRoData_DisplayList_CollisionRecord *col;
    ModelRoData_DisplayListPrimaryRecord *prim;
    Vertex *v;

    if (!convertMain(&m, &o))
    {
        return expect(0, "the main file converts");
    }
    dl = record_of(g_Out, o.dl);
    col = record_of(g_Out, o.col);
    prim = record_of(g_Out, o.prim);
    v = vertices_of(g_Out, dl);
    return expect(dl->ModelType == 3 && (uintptr_t) dl->BaseAddr == 0x1234 && dl->Secondary == NULL
            && v[2].coord.x == 0x120 && v[2].s == 0x124 && v[2].t == 0x125 && v[2].r == 0xC2 && v[2].a == 3
            && col->numVertices == 2 && col->numCollisionVertices == 2 && col->ModelType == 0x0405
            && col->RwDataIndex == 0x0607 && (uintptr_t) col->BaseAddr == 0x2345
            && ((Vertex *) at(g_Out, (uintptr_t) col->Vertices))[1].coord.z == 0x212
            && ((Vertex *) at(g_Out, (uintptr_t) col->CollisionVertices))[0].coord.x == 0x300
            && ((s16 *) at(g_Out, (uintptr_t) col->PointUsage))[0] == 0x1112
            && ((s16 *) at(g_Out, (uintptr_t) col->PointUsage))[1] == 0x1314
            && list_of(g_Out, col->Primary)[0].words.w1 == 0x04000000 && col->Secondary == NULL
            && prim->numVertices == 1 && (uintptr_t) prim->BaseAddr == 0x3456
            && ((Vertex *) at(g_Out, (uintptr_t) prim->Vertices))[3].coord.x == 0x430
            && GFX_OP(list_of(g_Out, prim->Primary)) == 0xB8,
            "the three display-list records' scalars, blocks and lists");
}

static int testCollisionLink(void)
{
    struct MainFile m;
    struct MainOut o;
    Vertex *cv;

    if (!convertMain(&m, &o))
    {
        return expect(0, "the main file converts");
    }
    cv = at(g_Out, (uintptr_t) ((ModelRoData_DisplayList_CollisionRecord *) record_of(g_Out, o.col))->CollisionVertices);
#if IS_64_BIT
    return expect(cv[1].LinkOffset == off_of(g_Out, o.col) && cv[1].CollisionRelatedIndex == 0x0102
            && cv[0].LinkOffset == 0 && cv[1].coord.x == 0x310,
            "a linked collision vertex holds its node's new offset and its swapped index");
#else
    /* The in-place path's bytes: s and t swapped as half-words, the index as stored. */
    return expect(cv[1].s == 0x0500 && (u16) cv[1].t == off_of(g_Out, o.col) && cv[1].r == 0x01 && cv[1].g == 0x02,
            "a linked collision vertex keeps the in-place path's bytes");
#endif
}

static int testTreeRecords(void)
{
    struct MainFile m;
    struct MainOut o;
    ModelRoData_LODRecord *lod;
    ModelRoData_SwitchRecord *sw;
    ModelRoData_BSPRecord *bsp;
    ModelRoData_BoundingBoxRecord *box;

    if (!convertMain(&m, &o))
    {
        return expect(0, "the main file converts");
    }
    lod = record_of(g_Out, o.lod);
    sw = record_of(g_Out, o.sw);
    bsp = record_of(g_Out, o.bsp);
    box = record_of(g_Out, o.box1);
    return expect(bits(&lod->MinDistance) == 0x30313233 && lod->RwDataIndex == 0x3C3D
            && (uintptr_t) lod->Affects == NODE_REF(o.sw) && sw->RwDataIndex == 0x3C3D && sw->reserved == 0x3E3F
            && (uintptr_t) sw->Controls == NODE_REF(o.head)
            && ((ModelRoData_HeadPlaceholderRecord *) record_of(g_Out, o.head))->RwDataIndex == 0x3C3D
            && bits(&bsp->Point.x) == 0x40414243 && bits(&bsp->Vector.z) == 0x54555657 && bsp->RwDataIndex == 0x6263
            && (uintptr_t) bsp->leftChild == NODE_REF(o.box1) && (uintptr_t) bsp->rightChild == NODE_REF(o.box2)
            && box->ModelNumber == 0x50515253 && bits(&box->Bounds.xmin) == 0x54555657
            && ((ModelRoData_BoundingBoxRecord *) record_of(g_Out, o.box2))->ModelNumber == 0x58595A5B,
            "the LOD's, switch's, head's, BSP's and boxes' scalars and references");
}

static int testImageRecords(void)
{
    struct MainFile m;
    struct MainOut o;
    ModelRoData_GunfireRecord *gun;
    ModelRoData_ShadowRecord *sh;
    ModelRoData_GroupSimpleRecord *gs;
    ModelRoData_InterlinkageRecord *il;
    uintptr_t texture1 = 0x05000000u | (2 * sizeof(void *) + 1 * sizeof(ModelFileTextures));

    if (!convertMain(&m, &o))
    {
        return expect(0, "the main file converts");
    }
    gun = record_of(g_Out, o.gun);
    sh = record_of(g_Out, o.shadow);
    gs = record_of(g_Out, o.gs);
    il = record_of(g_Out, o.il);
    return expect(bits(&gun->Offset.x) == 0x60616263 && bits(&gun->Scale) == 0x7C7D7E7F && gun->RwDataIndex == 0x8081
            && (uintptr_t) gun->BaseAddr == 0x84858687 && (uintptr_t) gun->Image == texture1
            && bits(&sh->pos.x) == 0x70717273 && bits(&sh->Scale) == 0x88898A8B && (uintptr_t) sh->BaseAddr == 0x8C8D8E8F
            && (uintptr_t) sh->image == texture1 && (uintptr_t) sh->Header == NODE_REF(o.header)
            && bits(&gs->Origin.x) == 0x90919293 && gs->Group1 == (s16) 0x9C9D && gs->Group2 == 0x9E9F
            && bits(&gs->BoundingVolumeRadius) == 0xA0A1A2A3 && bits(&il->pos.x) == 0xA0A1A2A3
            && bits(&il->Scale) == 0xB8B9BABB,
            "the gunfire's, shadow's, simple group's and interlink's scalars and references");
}

static int testTexturesAndRawBytes(void)
{
    struct MainFile m;
    struct MainOut o;
    ModelFileTextures *tex = (ModelFileTextures *) (g_Out + MAIN_SWITCHES * sizeof(void *));
    u8 *texels, *il;
    u32 i, run = 0, best = 0;

    if (!convertMain(&m, &o))
    {
        return expect(0, "the main file converts");
    }
    texels = at(g_Out, tex[2].TextureID);
    il = (u8 *) record_of(g_Out, o.il) + sizeof(ModelRoData_InterlinkageRecord);
    for (i = 0; il + i < texels; i++)
    {
        run = il[i] == 0x5A ? run + 1 : 0;
        best = run > best ? run : best;
    }
    return expect(tex[0].TextureID == 0x55 && tex[0].MipMapTiles == 0x61 && tex[0].RenderDepth == 2
            && tex[0].tflags == 0x64 && bits(texels) == 0x80818283 && bits(texels + 12) == 0x8C8D8E8F && best == 8,
            "texture entries, inline texels as words, and the bytes nothing owns copied");
}

/* ==== a file of the opcodes the ROM lacks =============================== */

struct RareFile
{
    u32 root, rootRec, op3, op3Rec, op5, op5Rec, op6, op6Rec, op7, op7Rec, op11, op11Rec, op17, op17Rec, dl, dlRec;
};

static void buildRare(struct RareFile *f)
{
    static const u32 end[] = {0xB8000000, 0};
    u32 i;

    mfBegin(0, 0);
    f->root = mfNode(MODELNODE_OPCODE_OP20);
    f->rootRec = mfRecord(f->root, 16);
    f->op3 = mfNode(MODELNODE_OPCODE_OP03);
    f->op3Rec = mfRecord(f->op3, 28);
    f->op5 = mfNode(MODELNODE_OPCODE_OP05);
    f->op5Rec = mfRecord(f->op5, 424);
    f->op6 = mfNode(MODELNODE_OPCODE_OP06);
    f->op6Rec = mfRecord(f->op6, 24);
    f->op7 = mfNode(MODELNODE_OPCODE_OP07);
    f->op7Rec = mfRecord(f->op7, 432);
    f->op11 = mfNode(MODELNODE_OPCODE_OP11);
    f->op11Rec = mfRecord(f->op11, 76);
    f->op17 = mfNode(MODELNODE_OPCODE_OP17);
    f->op17Rec = mfRecord(f->op17, 28);
    f->dl = mfNode(MODELNODE_OPCODE_DL);
    f->dlRec = mfRecord(f->dl, 20);
    mfPut32(f->dlRec, SEG(mfList(end, 1)));
    mfPattern(f->rootRec, 0, 16, 0x11);
    mfPattern(f->op3Rec, 0, 28, 0x21);
    mfPattern(f->op5Rec, 0, 424, 0x31);
    mfPattern(f->op6Rec, 0, 24, 0x41);
    mfPattern(f->op7Rec, 0, 432, 0x51);
    mfPattern(f->op11Rec, 0, 76, 0x61);
    mfPattern(f->op17Rec, 0, 28, 0x71);
    /* references: valid nodes or 0; opcodes 5 and 7 have no children */
    mfPut32(f->rootRec + 4, SEG(f->op3));
    mfPut32(f->op3Rec + 0x14, 0);
    for (i = 0; i < 16; i++)
    {
        mfPut8(f->op5Rec + i, 0);
        mfPut8(f->op7Rec + 8 + i, 0);
    }
    mfPut32(f->op7Rec + 0, SEG(f->op3));
    mfPut32(f->op7Rec + 4, SEG(f->op5));
    mfPut32(f->op11Rec + 0x3C, SEG(f->op6));
    mfPut32(f->op17Rec + 0x14, SEG(f->op3));
    mfAdopt(f->root, 0, f->op3);
    mfAdopt(f->root, f->op3, f->op5);
    mfAdopt(f->root, f->op5, f->op6);
    mfAdopt(f->root, f->op6, f->op7);
    mfAdopt(f->root, f->op7, f->op11);
    mfAdopt(f->root, f->op11, f->op17);
    mfAdopt(f->root, f->op17, f->dl);
    g_Size = (g_Size + 7) & ~7u;
}

/* Whether each of the N native words at P is the big-endian word of the file at FROM. */
static int wordsMatch(const u8 *p, u32 from, u32 n)
{
    u32 i;

    for (i = 0; i < n && bits(p + 4 * i) == mfGet32(from + 4 * i); i++)
    {
    }
    return i == n;
}

static int testOpcodesTheRomLacks(void)
{
    struct RareFile f;
    ModelNode *root, *n3, *n5, *n6, *n7, *n11, *n17;
    ModelRoData_HeaderRecord *r20;
    ModelRoData_GroupRecord *r3, *r17;
    ModelRoData_Op05Record *r5;
    ModelRoData_Op06Record *r6;
    ModelRoData_Op07Record *r7;
    ModelRoData_Op11Record *r11;
    u32 n, k, wide = 1;

    buildRare(&f);
    n = convertFile(0, 0);
    if (expect(n != 0 && sameAsInPlace(0, 0, n), "the file of the opcodes the ROM lacks converts"))
    {
        return 1;
    }
    root = rootOf(g_Out, 0, 0);
    n3 = nth_child(g_Out, root, 0), n5 = nth_child(g_Out, root, 1), n6 = nth_child(g_Out, root, 2);
    n7 = nth_child(g_Out, root, 3), n11 = nth_child(g_Out, root, 4), n17 = nth_child(g_Out, root, 5);
    r20 = record_of(g_Out, root), r3 = record_of(g_Out, n3), r5 = record_of(g_Out, n5), r6 = record_of(g_Out, n6);
    r7 = record_of(g_Out, n7), r11 = record_of(g_Out, n11), r17 = record_of(g_Out, n17);
    for (k = 3; k < 15; k++)
    {
        wide &= r11->unk0c[k] == mfGet32(f.op11Rec + 4 * k);
    }
    return expect(r20->AnimPart == mfGet16(f.rootRec) && r20->Group2 == mfGet16(f.rootRec + 0xA)
            && r20->reserved == mfGet16(f.rootRec + 0xE) && (uintptr_t) r20->FirstGroup == NODE_REF(n3)
            && wordsMatch((u8 *) &r3->Origin, f.op3Rec, 3) && r3->JointID == mfGet16(f.op3Rec + 0xC)
            && r3->MatrixID2 == (s16) mfGet16(f.op3Rec + 0x12) && r3->ChildGroup == NULL
            && bits(&r3->BoundingVolumeRadius) == mfGet32(f.op3Rec + 0x18)
            && r5->NumChildren == 0 && r5->Children == NULL && r5->Images == NULL
            && wordsMatch(r5->Data, f.op5Rec + 0x10, 100) && r5->unk1A0 == mfGet32(f.op5Rec + 0x1A0)
            && (uintptr_t) r5->BaseAddr == mfGet32(f.op5Rec + 0x1A4)
            && wordsMatch((u8 *) &r6->unk00, f.op6Rec, 5) && (uintptr_t) r6->BaseAddr == mfGet32(f.op6Rec + 0x14)
            && (uintptr_t) r7->unk00 == NODE_REF(n3) && (uintptr_t) r7->unk04 == NODE_REF(n5) && r7->Vertices == NULL
            && wordsMatch(r7->Data, f.op7Rec + 0x18, 100) && r7->unk1A8 == mfGet16(f.op7Rec + 0x1A8)
            && r7->RwDataIndex == mfGet16(f.op7Rec + 0x1AA) && (uintptr_t) r7->BaseAddr == mfGet32(f.op7Rec + 0x1AC)
            && wordsMatch((u8 *) &r11->pos, f.op11Rec, 3) && wide && r11->unk0c[15] == NODE_REF(n6)
            && bits(&r11->BoundingVolumeRadius) == mfGet32(f.op11Rec + 0x40)
            /* swapped as one word at 0x44, as romfile_model.c does */
            && r11->RwDataIndex == mfGet16(f.op11Rec + 0x46) && r11->unk46 == mfGet16(f.op11Rec + 0x44)
            && (uintptr_t) r11->BaseAddr == mfGet32(f.op11Rec + 0x48)
            /* opcode 17 is read with the group record's map, as romfile_model.c does */
            && wordsMatch((u8 *) &r17->Origin, f.op17Rec, 3) && r17->JointID == mfGet16(f.op17Rec + 0xC)
            && (uintptr_t) ((ModelRoData_Op17Record *) r17)->othernode == NODE_REF(n3)
            && bits(&((ModelRoData_Op17Record *) r17)->scale1) == mfGet32(f.op17Rec + 0x18),
            "every scalar of opcodes 3, 5, 6, 7, 11, 17 and 20 in the field its pattern predicts");
}

static int testListCallsAnother(void)
{
    static const u32 end[] = {0xB8000000, 0};
    u32 rec, called, i, ok = 1;
    static const u32 calls[] = {0x06000000, 0xDE000000};

    for (i = 0; i < 2; i++)
    {
        // Arrange: a DL node whose list calls a second list
        ModelNode *node;
        Gfx *list;
        u32 caller[] = {calls[i], 0, 0xB8000000, 0}, n;
        mfBegin(0, 0);
        rec = mfRecord(mfNode(MODELNODE_OPCODE_DL), 20);
        mfPut32(rec, SEG(mfList(caller, 2)));
        called = mfList(end, 1);
        mfPut32(mfGet32(rec) - 0x05000000u + 4, SEG(called));
        g_Size = (g_Size + 7) & ~7u;

        // Act
        n = convertFile(0, 0);
        ok &= n != 0;
        node = rootOf(g_Out, 0, 0);
        list = list_of(g_Out, ((ModelRoData_DisplayListRecord *) record_of(g_Out, node))->Primary);

        // Assert
        ok &= GFX_OP(&list[0]) == calls[i] >> 24
            && list[0].words.w1 == (0x05000000u | (off_of(g_Out, list) + 2 * (u32) sizeof(Gfx)))
            && GFX_OP(list_of(g_Out, (void *) list[0].words.w1)) == 0xB8 && sameAsInPlace(0, 0, n);
    }
    return expect(ok, "a list call's word is the called list's new offset");
}

int main(void)
{
    int failures = testBrief() + testWritesExactlyTheMeasuredBytes() + testMatchesInPlaceAt32Bits() + testNodes()
        + testHeaderAndGroup() + testDisplayListRecords() + testCollisionLink() + testTreeRecords() + testImageRecords()
        + testTexturesAndRawBytes() + testOpcodesTheRomLacks() + testListCallsAnother();

    if (failures != 0)
    {
        printf("test_romconv_model: %d failed\n", failures);
        return 1;
    }
    printf("test_romconv_model: OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
