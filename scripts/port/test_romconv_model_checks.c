/* The model file converter's input checks (LP64-004 Task 10): each malformed
 * file is refused by name with nothing written, and every file either
 * function accepts converts in exactly the bytes measured. Runs
 * test_model_bounds.c's inputs (truncated records, count wraparound, 10,000
 * random files) and 10,000 mutations of the main fixture through both
 * functions, under the sanitizers at 64 bits; at 32 bits each accepted file is
 * also compared with romfile_model.c's in-place result. Built and run at each
 * pointer width by test_romconv.sh. */
#include "test_romconv_model_build.h"


static int refusedMain(void (*change)(const struct MainFile *m), const char *why, const char *what)
{
    struct MainFile m;

    buildMain(&m);
    change(&m);
    return expectRefused(MAIN_SWITCHES, MAIN_TEXTURES, why, what);
}

static void nodeBelowRoot(const struct MainFile *m) { mfPut32(m->group + LINK_NEXT, SEG(8)); }
static void recordPastEnd(const struct MainFile *m) { mfPut32(m->dl + LINK_DATA, SEG(g_Size - 8)); }
static void unownedReference(const struct MainFile *m) { mfPut32(m->gunRec + 0x18, SEG(m->gap)); }
static void nextLoops(const struct MainFile *m) { mfPut32(m->il + LINK_NEXT, SEG(m->dl)); }
static void childLoops(const struct MainFile *m) { mfPut32(m->box2 + LINK_CHILD, SEG(m->bsp)); }
static void recordAsNode(const struct MainFile *m) { mfPut32(m->lodRec + 8, SEG(m->dlRec)); }
static void blocksOverlap(const struct MainFile *m) { mfPut32(m->colRec + 8, SEG(m->dlVerts + 16)); }
static void listCallsItself(const struct MainFile *m)
{
    mfPut32(m->dlList + 8, 0x06000000);
    mfPut32(m->dlList + 12, SEG(m->dlList));
}
static void notSegmentFive(const struct MainFile *m) { mfPut32(m->headerRec + 4, 0x06000010); }
static void texelDepth(const struct MainFile *m) { (void) m; mfPut8(MAIN_SWITCHES * 4 + 2 * 12 + 8, 4); }
static void vertexSpanPastBlock(const struct MainFile *m) { mfPut32(m->dlList, 0x04300060); }
static void recordlessOpcode(const struct MainFile *m) { mfPut16(m->gs, MODELNODE_OPCODE_OP14); }
static void recordMissing(const struct MainFile *m) { mfPut32(m->bsp + LINK_DATA, 0); }

static int testBriefRefusals(void)
{
    struct MainFile m;
    u32 n;
    s32 ok;
    int failures;

    buildMain(&m);
    n = portModelConvertedSize(g_File, g_Size, MAIN_SWITCHES, MAIN_TEXTURES, "Ptest");
    ok = portModelConvert(g_File, g_Size, g_Out, n - 8, MAIN_SWITCHES, MAIN_TEXTURES, "Ptest");
    failures = expect(n != 0 && ok == 0 && allBytes(g_Out, sizeof(g_Out), 0xAA) && contains(g_Log, "the buffer has"),
            "an output 8 bytes short is refused with nothing written");
    failures += refusedMain(nodeBelowRoot, "lies below the root", "a node offset below the root");
    failures += refusedMain(recordPastEnd, "runs past the", "a record that runs past the file");
    return failures + refusedMain(unownedReference, "which nothing owns", "a reference to an offset nothing owns");
}

static int testCollisionLinkToOffsetZero(void)
{
    // Arrange: the root, at offset 0, is a DLCOLLISION whose vertex links to it
    static const u32 end[] = {0xB8000000, 0};
    u32 node, rec, cvert;
    mfBegin(0, 0);
    node = mfNode(MODELNODE_OPCODE_DLCOLLISION);
    rec = mfRecord(node, 32);
    cvert = mfVertices(1, 0x10);
    mfPut32(rec, SEG(mfList(end, 1)));
    mfPut16(rec + 0xE, 1);
    mfPut32(rec + 0x10, SEG(cvert));
    mfPut32(cvert + 8, SEG(node));
    g_Size = (g_Size + 7) & ~7u;

    // Act / Assert
    return expectRefused(0, 0, "links to the node at new offset 0", "a collision link to offset 0");
}

static int testMalformedGraphs(void)
{
    return refusedMain(nextLoops, "would not end", "a Next cycle")
        + refusedMain(childLoops, "would not end", "a Child cycle")
        + refusedMain(recordAsNode, "overlaps a record", "a record reached as a node")
        + refusedMain(blocksOverlap, "overlaps a vertex block", "two vertex blocks sharing bytes")
        + refusedMain(listCallsItself, "calls itself", "a list that calls itself")
        + refusedMain(notSegmentFive, "neither 0 nor a segment-5 offset", "a reference of another segment")
        + refusedMain(texelDepth, "texel depth 4", "an inline texture deeper than 3")
        + refusedMain(vertexSpanPastBlock, "past the vertex block", "a G_VTX loading past its block")
        + refusedMain(recordlessOpcode, "which has no record", "data on an opcode with no record")
        + refusedMain(recordMissing, "which has a record", "an opcode with a record and no data");
}

static int testListsCallingInACircle(void)
{
    u32 rec, a, b;
    u32 words[] = {0x06000000, 0, 0xB8000000, 0};

    mfBegin(0, 0);
    rec = mfRecord(mfNode(MODELNODE_OPCODE_DL), 20);
    a = mfList(words, 2);
    b = mfList(words, 2);
    mfPut32(rec, SEG(a));
    mfPut32(a + 4, SEG(b));
    mfPut32(b + 4, SEG(a));
    g_Size = (g_Size + 7) & ~7u;
    return expectRefused(0, 0, "calls itself", "two lists calling each other");
}

static int testNoGeometry(void)
{
    mfBegin(0, 0);
    mfNode(MODELNODE_OPCODE_NULL);
    g_Size = 24;
    return expectRefused(0, 0, "no vertices and no display lists", "a file with no geometry");
}

static int testChildrenOfOpcode5(void)
{
    static const u32 end[] = {0xB8000000, 0};
    u32 dl = 0, op5;

    mfBegin(0, 0);
    dl = mfNode(MODELNODE_OPCODE_DL);
    mfPut32(mfRecord(dl, 20), SEG(mfList(end, 1)));
    op5 = mfNode(MODELNODE_OPCODE_OP05);
    mfPut32(mfRecord(op5, 424), 1); /* NumChildren 1, Children 0 */
    mfNodeLink(dl, LINK_NEXT, op5);
    g_Size = (g_Size + 7) & ~7u;
    return expectRefused(0, 0, "has children", "an opcode 5 record with children");
}

static int testOutputBuffer(void)
{
    struct MainFile m;
    u32 n;
    s32 misaligned, overlapping;

    buildMain(&m);
    n = portModelConvertedSize(g_File, g_Size, MAIN_SWITCHES, MAIN_TEXTURES, "Ptest");
    if (expect(portModelConvert(g_File, g_Size, NULL, n, MAIN_SWITCHES, MAIN_TEXTURES, "Ptest") == 0
                && contains(g_Log, "not 8-byte aligned"), "a NULL output is refused"))
    {
        return 1;
    }
    misaligned = portModelConvert(g_File, g_Size, g_Out + 4, n, MAIN_SWITCHES, MAIN_TEXTURES, "Ptest");
    if (expect(misaligned == 0 && allBytes(g_Out, sizeof(g_Out), 0xAA) && contains(g_Log, "not 8-byte aligned"),
                "a misaligned output is refused with nothing written"))
    {
        return 1;
    }
    __builtin_memcpy(g_Out, g_File, g_Size);
    overlapping = portModelConvert(g_Out, g_Size, g_Out, n, MAIN_SWITCHES, MAIN_TEXTURES, "Ptest");
    return expect(overlapping == 0 && sameBytes(g_Out, g_File, g_Size) && contains(g_Log, "overlaps the file"),
            "an output over the file is refused with nothing written");
}

/* LP64-007 Task 7: the renderer reads a G_TEXRECT's next two words as its
 * texture coordinates, whatever they hold. A file with one DL node whose
 * list is WORDS (N commands), at the offset returned; a second list, ending
 * at once, after it, at *SECOND. */
static u32 buildTexRectList(const u32 *words, u32 n, u32 *second)
{
    static const u32 end[] = {0xB8000000, 0};
    u32 rec, list;

    mfBegin(0, 0);
    rec = mfRecord(mfNode(MODELNODE_OPCODE_DL), 20);
    list = mfList(words, n);
    mfPut32(rec, SEG(list));
    *second = mfList(end, 1);
    g_Size = (g_Size + 7) & ~7u;
    return list;
}

static int testRefusesATexRectAtTheListsEnd(void)
{
    // Arrange: the end command is the first word of the tail
    static const u32 words[] = {0xE4000000, 0, 0xB8000000, 0};
    u32 second;
    buildTexRectList(words, 2, &second);

    // Act, Assert
    return expectRefused(0, 0, "is not followed by its G_RDPHALF_2 and G_RDPHALF_CONT words",
            "a G_TEXRECT whose tail is the list's end");
}

static int testRefusesATexRectWhoseTailIsACall(void)
{
    // Arrange: the tail's first word reads as a G_DL to the second list
    static const u32 words[] = {0xE5000000, 0, 0x06000000, 0, 0xB2000000, 0, 0xB8000000, 0};
    u32 second, list = buildTexRectList(words, 4, &second);
    mfPut32(list + 8 + 4, SEG(second));

    // Act, Assert
    return expectRefused(0, 0, "is not followed by its G_RDPHALF_2 and G_RDPHALF_CONT words",
            "a G_TEXRECTFLIP whose tail's first word is a list call");
}

static int testAcceptsATexRectWithItsTail(void)
{
    // Arrange
    static const u32 words[] = {0xE4000000, 0, 0xB3000000, 0x00100020, 0xB2000000, 0x04000400, 0xB8000000, 0};
    u32 second;
    buildTexRectList(words, 4, &second);

    // Act
    u32 n = convertFile(0, 0);

    // Assert
    return expect(n != 0 && sameAsInPlace(0, 0, n), "a G_TEXRECT followed by its two words is converted");
}

/* A segment-5 w1 in a command whose w1 is an address the converter does not
 * move (no stock list has one) refuses the file, naming the command. */
static int testOtherAddressCommands(void)
{
    static const u8 ops[] = {0x01, 0x03, 0x09, 0x34, 0xFE, 0xFF};
    struct MainFile m;
    u32 i;
    int failures = 0;

    for (i = 0; i < sizeof(ops); i++)
    {
        buildMain(&m);
        mfPut32(m.dlList + 8, (u32) ops[i] << 24);
        mfPut32(m.dlList + 12, SEG(m.texels));
        failures += expectRefused(MAIN_SWITCHES, MAIN_TEXTURES, "an address the converter does not move",
                "a segment-5 address in another command");
    }
    return failures;
}

/* romfile_model.c skips a block counted above 0x8000; the converter refuses
 * one that is there, and ignores the count of one that is not. */
static void largeCountWithBlock(const struct MainFile *m) { mfPut16(m->dlRec + 0x10, 0x9000); }

static int testLargeCounts(void)
{
    struct MainFile m;
    u32 n;
    int failures = refusedMain(largeCountWithBlock, "counts 36864 elements", "a block counted above 0x8000");

    buildMain(&m);
    mfPut32(m.dlRec + 0xC, 0);
    mfPut16(m.dlRec + 0x10, 0x9000);
    mfPut32(m.dlList, 0xB8000000); /* nothing loads the vertices now unowned */
    mfPut32(m.dlList + 4, 0);
    n = convertFile(MAIN_SWITCHES, MAIN_TEXTURES);
    return failures + expect(n != 0 && sameAsInPlace(MAIN_SWITCHES, MAIN_TEXTURES, n),
            "a count above 0x8000 with no block converts as the in-place path does");
}

/* A raw tail of TAIL bytes after the small file in BIG; its measure. */
static u32 measureWithTail(u8 *big, u32 small, u32 tail)
{
    return portModelConvertedSize(big, small + tail, 0, 0, "Ptest");
}

/* Every reference is 0x05000000 | offset, so no item may reach past
 * 0x01000000: one ending there converts, one 4 bytes longer is refused. */
static int testSixteenMegabytes(void)
{
    static const u32 end[] = {0xB8000000, 0};
    const u32 limit = 0x01000000u;
    u32 rec, small, at4, at8, cursor, tail, n, refused;
    u8 *big;

    mfBegin(0, 0);
    rec = mfRecord(mfNode(MODELNODE_OPCODE_DL), 20); /* the root first: argument order is unspecified */
    mfPut32(rec, SEG(mfList(end, 1)));
    small = g_Size; /* a multiple of 4 */
    big = (u8 *) calloc(limit + 64, 1);
    if (expect(big != NULL, "16 MB for the big file"))
    {
        return 1;
    }
    __builtin_memcpy(big, g_File, small);
    /* The converted items end at CURSOR, which the measure rounds to 8. */
    at4 = measureWithTail(big, small, 4);
    at8 = measureWithTail(big, small, 8);
    cursor = at8 == at4 ? at8 - 8 : at4 - 4;
    tail = limit - cursor;
    n = measureWithTail(big, small, tail);
    g_Log[0] = '\0';
    refused = measureWithTail(big, small, tail + 4);
    free(big);
    return expect(n == limit, "a file whose last item ends at 16 MB converts")
        + expect(refused == 0 && contains(g_Log, "cannot convert models \"Ptest\": the raw bytes at 0x")
                    && contains(g_Log, "past the 16 MB"),
                "a file whose last item ends past 16 MB is refused, naming the item");
}

/* ==== bounds and malformed files ======================================== */

static u32 s_Accepted;

/* FILE (SIZE bytes) is either refused by both functions with nothing
 * written, or converted in exactly the bytes measured; at 32 bits an
 * accepted file is the in-place result. */
static int convertsWithinMeasure(const u8 *file, u32 size, s32 switches, s32 textures)
{
    u32 n = portModelConvertedSize(file, size, switches, textures, "fuzz");
    u8 *exact;
    int ok;

    if (n == 0)
    {
        __builtin_memset(g_Out, 0xAA, sizeof(g_Out));
        return portModelConvert(file, size, g_Out, MF_OUT_CAP, switches, textures, "fuzz") == 0
            && allBytes(g_Out, sizeof(g_Out), 0xAA);
    }
    s_Accepted++;
    exact = (u8 *) malloc(n); /* exactly the size measured: the sanitizer sees any write past it */
    ok = exact != NULL && portModelConvert(file, size, exact, n, switches, textures, "fuzz") == 1;
#if !IS_64_BIT
    if (ok)
    {
        u8 *copy = (u8 *) malloc(size);
        __builtin_memcpy(copy, file, size);
        ok = romfileModelConvert(copy, size, switches, textures, "fuzz", NULL) == 1 && n == ((size + 7) & ~7u)
            && sameBytes(copy, exact, size) && allBytes(exact + size, n - size, 0);
        free(copy);
    }
#endif
    free(exact);
    return ok;
}

static int testBoundsInputs(void)
{
    static const u8 types[] = {MODELNODE_OPCODE_DL, MODELNODE_OPCODE_DLCOLLISION, MODELNODE_OPCODE_DLPRIMARY,
        MODELNODE_OPCODE_LOD, MODELNODE_OPCODE_BSP, MODELNODE_OPCODE_SWITCH};
    u8 data[48] = {0};
    u32 i, failures = 0;

    for (i = 0; i < sizeof(types); i++)
    {
        /* a node pointing at the file's last byte as its record */
        __builtin_memset(data, 0, sizeof(data));
        data[1] = types[i];
        data[4] = 5;
        data[7] = 47;
        failures += portModelConvertedSize(data, sizeof(data), 0, 0, "truncated") != 0;
        failures += portModelConvert(data, sizeof(data), g_Out, MF_OUT_CAP, 0, 0, "truncated") != 0;
    }
    __builtin_memset(data, 0, sizeof(data));
    failures += portModelConvertedSize(data, sizeof(data), 0x40000000, 0, "count") != 0;
    failures += portModelConvertedSize(data, sizeof(data), 0, 0x40000000, "count") != 0;
    failures += portModelConvertedSize(data, sizeof(data), -1, 0, "count") != 0;
    failures += portModelConvertedSize(NULL, sizeof(data), 0, 0, "null") != 0;
    return expect(failures == 0, "truncated records, wrapping counts and a NULL file are refused");
}

static int testRandomFiles(void)
{
    u32 random = 1, round, i, failures = 0;
    static u8 bytes[512];

    for (round = 0; round < 10000; round++)
    {
        const u32 size = 24 + round % 488;
        for (i = 0; i < size; i++)
        {
            random = random * 1664525u + 1013904223u;
            bytes[i] = (u8) (random >> 24);
        }
        failures += !convertsWithinMeasure(bytes, size, (s32) (round % 8), (s32) (round % 8));
    }
    return expect(failures == 0, "10000 random files convert within their measure or are refused");
}

static int testMutatedFiles(void)
{
    struct MainFile m;
    static u8 bytes[MF_CAP];
    u32 random = 7, round, i, size, failures = 0, before = s_Accepted;

    buildMain(&m);
    size = g_Size;
    for (round = 0; round < 10000; round++)
    {
        __builtin_memcpy(bytes, g_File, size);
        for (i = 0; i <= round % 4; i++)
        {
            random = random * 1664525u + 1013904223u;
            bytes[(random >> 8) % size] = (u8) (random >> 24);
        }
        failures += !convertsWithinMeasure(bytes, size, MAIN_SWITCHES, MAIN_TEXTURES);
    }
    printf("test_romconv_model_checks: %u of 10000 mutated files accepted\n", s_Accepted - before);
    return expect(failures == 0 && s_Accepted > before, "10000 mutated files convert within their measure or are refused");
}

int main(void)
{
    int failures = testBriefRefusals() + testCollisionLinkToOffsetZero() + testMalformedGraphs()
        + testListsCallingInACircle() + testNoGeometry() + testChildrenOfOpcode5() + testOutputBuffer()
        + testOtherAddressCommands() + testLargeCounts() + testSixteenMegabytes()
        + testBoundsInputs() + testRandomFiles() + testMutatedFiles() + testRefusesATexRectAtTheListsEnd()
        + testRefusesATexRectWhoseTailIsACall() + testAcceptsATexRectWithItsTail();

    if (failures != 0)
    {
        printf("test_romconv_model_checks: %d failed\n", failures);
        return 1;
    }
    printf("test_romconv_model_checks: OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
