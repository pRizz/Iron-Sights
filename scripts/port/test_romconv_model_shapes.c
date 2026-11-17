/* The model file converter on shapes the stock ROM has and the main fixture
 * lacks (LP64-004 Task 10): an odd point-usage count, a display list loading
 * vertices its record does not count (as PexplosionbitZ's), and a segment-5
 * word that is not an address (as G_TRI4's vertex indices). Each converts,
 * with a value checked at 64 bits and the whole image compared with
 * romfile_model.c's at 32. Built and run at each pointer width by
 * test_romconv.sh. */
#include "test_romconv_model_build.h"

static const u32 kEnd[] = {0xB8000000, 0};

/* The half-word at P, native. */
static u32 half(const void *p)
{
    u16 v;

    __builtin_memcpy(&v, p, 2);
    return v;
}

/* A collision record of 3 vertices whose 3 point-usage half-words end the
 * file 6 bytes past their block's start: the slot holding the third is cut
 * short by the file's end. */
static int testOddPointUsageAtTheFileEnd(void)
{
    // Arrange
    u32 node, rec, pu, n;
    ModelRoData_DisplayList_CollisionRecord *col;
    const u8 *p;
    mfBegin(0, 0);
    node = mfNode(MODELNODE_OPCODE_DLCOLLISION);
    rec = mfRecord(node, 32);
    mfPut32(rec, SEG(mfList(kEnd, 1)));
    mfPut32(rec + 8, SEG(mfVertices(3, 0x500)));
    mfPut16(rec + 0xC, 3);
    pu = mfAlloc(6);
    mfPut32(rec + 0x14, SEG(pu));
    mfPut16(pu, 0x2122);
    mfPut16(pu + 2, 0x2324);
    mfPut16(pu + 4, 0x2526);

    // Act
    n = convertFile(0, 0);

    // Assert
    if (expect(n != 0, "a file ending in an odd point-usage block converts"))
    {
        return 1;
    }
    col = (ModelRoData_DisplayList_CollisionRecord *) record_of(g_Out, (ModelNode *) g_Out);
    p = (const u8 *) at(g_Out, (uintptr_t) col->PointUsage);
    return expect(half(p) == 0x2122 && half(p + 2) == 0x2324 && half(p + 4) == 0x2526 && sameAsInPlace(0, 0, n),
            "all three point-usage half-words are swapped, the last in the file's cut-short slot");
}

/* A collision record of 3 vertices with its point usage mid-file: the
 * half-word after the third is the block's padding, copied as it is. */
static int testOddPointUsagePadding(void)
{
    // Arrange
    u32 node, rec, pu, n;
    ModelRoData_DisplayList_CollisionRecord *col;
    const u8 *p;
    mfBegin(0, 0);
    node = mfNode(MODELNODE_OPCODE_DLCOLLISION);
    rec = mfRecord(node, 32);
    pu = mfAlloc(8);
    mfPut32(rec, SEG(mfList(kEnd, 1)));
    mfPut32(rec + 8, SEG(mfVertices(3, 0x500)));
    mfPut16(rec + 0xC, 3);
    mfPut32(rec + 0x14, SEG(pu));
    mfPut16(pu, 0x2122);
    mfPut16(pu + 2, 0x2324);
    mfPut16(pu + 4, 0x2526);
    mfPut16(pu + 6, 0x2728);
    g_Size = (g_Size + 7) & ~7u;

    // Act
    n = convertFile(0, 0);

    // Assert
    if (expect(n != 0, "a file with an odd point-usage block converts"))
    {
        return 1;
    }
    col = (ModelRoData_DisplayList_CollisionRecord *) record_of(g_Out, (ModelNode *) g_Out);
    p = (const u8 *) at(g_Out, (uintptr_t) col->PointUsage);
    return expect(half(p + 4) == 0x2526 && p[6] == 0x27 && p[7] == 0x28 && sameAsInPlace(0, 0, n),
            "an odd block's three half-words are swapped and its padding kept");
}

/* A DL record counting no vertices whose list loads two from its Vertices:
 * the in-place path leaves those bytes as the file has them, and so does the
 * converter, with both words naming their new place. */
static int testListLoadingUncountedVertices(void)
{
    // Arrange
    u32 rec, list, verts, n;
    ModelRoData_DisplayListRecord *dl;
    Gfx *cmd;
    u32 words[] = {0x04100020, 0, 0xB8000000, 0};
    mfBegin(0, 0);
    rec = mfRecord(mfNode(MODELNODE_OPCODE_DL), 20);
    list = mfList(words, 2);
    verts = mfVertices(2, 0x600);
    mfPut32(list + 4, SEG(verts));
    mfPut32(rec, SEG(list));
    mfPut32(rec + 0xC, SEG(verts)); /* numVertices stays 0 */
    g_Size = (g_Size + 7) & ~7u;

    // Act
    n = convertFile(0, 0);

    // Assert
    if (expect(n != 0, "a list loading vertices its record does not count converts"))
    {
        return 1;
    }
    dl = (ModelRoData_DisplayListRecord *) record_of(g_Out, (ModelNode *) g_Out);
    cmd = list_of(g_Out, dl->Primary);
    return expect(dl->numVertices == 0 && cmd[0].words.w1 == (uintptr_t) dl->Vertices
                && sameBytes(at(g_Out, (uintptr_t) dl->Vertices), g_File + verts, 32) && sameAsInPlace(0, 0, n),
            "the uncounted vertices are copied as they are and both words name their new place");
}

/* G_TRI4's w1 holds vertex indices; 0x05 in its top byte is not a segment. */
static int testSegmentFiveIndicesAreCopied(void)
{
    // Arrange
    static const u32 words[] = {0xB1004402, 0x05201310, 0xB8000000, 0};
    u32 rec, n;
    Gfx *cmd;
    mfBegin(0, 0);
    rec = mfRecord(mfNode(MODELNODE_OPCODE_DL), 20);
    mfPut32(rec, SEG(mfList(words, 2)));
    g_Size = (g_Size + 7) & ~7u;

    // Act
    n = convertFile(0, 0);

    // Assert
    if (expect(n != 0, "a G_TRI4 whose indices start with 0x05 converts"))
    {
        return 1;
    }
    cmd = list_of(g_Out, ((ModelRoData_DisplayListRecord *) record_of(g_Out, (ModelNode *) g_Out))->Primary);
    return expect(cmd[0].words.w1 == 0x05201310 && sameAsInPlace(0, 0, n), "a G_TRI4's indices are copied");
}

int main(void)
{
    int failures = testOddPointUsageAtTheFileEnd() + testOddPointUsagePadding() + testListLoadingUncountedVertices()
        + testSegmentFiveIndicesAreCopied();

    if (failures != 0)
    {
        printf("test_romconv_model_shapes: %d failed\n", failures);
        return 1;
    }
    printf("test_romconv_model_shapes: OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
