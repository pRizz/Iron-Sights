/* The setup file converter's input checks (LP64-004 Task 14): every table,
 * list, name, AI list, intro, credits table and prop list walk is bounded by
 * the file and refused by name when it finds no terminator; references that
 * name the wrong kind of item, overlapping items and bad buffers are refused
 * with nothing written; and random and mutated files convert in exactly the
 * bytes measured or are refused. Run at both widths by test_romconv.sh. */
#include "test_romconv_setup_build.h"

/* Appends N words of 4 (nonzero, positive, aligned: no table's terminator)
 * and returns their offset. */
static u32 sfFours(u32 n)
{
    const u32 at = g_Size;
    u32 i;

    for (i = 0; i < n; i++)
    {
        sfWords(1, 4u);
    }
    return at;
}

static int testBriefRefusals(void)
{
    struct SetupFixture f;
    u32 n, i, failures = 0;

    // A header offset outside the file
    sfBuildMain(&f);
    sfHeader(6, g_Size + 4);
    failures += !sfRefused("header offset 6");

    // A pad table with no terminator: two pads named at offset 4, then the file's end
    sfBegin();
    sfPad(1.0f, 2.0f, 3.0f, 4);
    sfPad(1.0f, 2.0f, 3.0f, 4);
    for (i = 0; i < 8; i++)
    {
        sfHeader(i, 40);
    }
    failures += !sfRefused("the pad table at 0x28 has no terminator");

    // An output buffer 8 bytes short
    sfBuildMain(&f);
    n = portSetupConvertedSize(g_File, g_Size, "UsetuptestZ");
    failures += n == 0 || portSetupConvert(g_File, g_Size, g_Out, n - 8, "UsetuptestZ") != 0
        || !contains(g_Log, "needs") || !allBytes(g_Out, sizeof(g_Out), 0xAA);
    return expect(failures == 0, "the brief's refusals: an outside header offset, no pad terminator, 8 bytes short");
}

static int testEveryTableWalkIsBounded(void)
{
    static const struct
    {
        u32 field;
        const char *message;
    } tables[] = {
        {7, "the bound pad table at"}, {0, "the waypoint table at"}, {1, "the waygroup table at"},
        {4, "the path table at"}, {5, "the AI list table at"}, {8, "the pad name table at"},
        {9, "the bound pad name table at"},
    };
    struct SetupFixture f;
    u32 i, failures = 0;

    for (i = 0; i < sizeof(tables) / sizeof(tables[0]); i++)
    {
        // Arrange: the table moved to the file's end with no terminator
        sfBuildMain(&f);
        sfHeader(tables[i].field, sfFours(20));

        // Act and Assert
        if (!sfRefused(tables[i].message) || !contains(g_Log, "has no terminator"))
        {
            printf("test_romconv_setup_checks: %s not refused\n", tables[i].message);
            failures++;
        }
    }
    return expect(failures == 0, "every table with no terminator inside the file is refused");
}

static int testEveryOtherWalkIsBounded(void)
{
    struct SetupFixture f;
    u32 at, failures = 0;

    // A neighbour list with no negative word
    sfBuildMain(&f);
    sfPut32(f.waypoints + 16 + 4, sfWords(2, 1u, 2u));
    failures += !sfRefused("has no negative word");

    // A name with no NUL before its unowned bytes end
    sfBuildMain(&f);
    sfPut8(f.names + 3, 'z');
    sfPut8(f.names + 6, 'z');
    sfPut8(f.names + 7, 'z');
    failures += !sfRefused("whose NUL is not before");

    // AI code with no AI_EndList byte before its unowned bytes end
    sfBuildMain(&f);
    sfPut8(f.code + 4, 0x05);
    failures += !sfRefused("whose AI_EndList byte is not before");

    // An intro with no end record
    sfBuildMain(&f);
    sfHeader(2, sfWords(3, INTRO_WORD(INTROTYPE_SPAWN), 0u, 0u));
    failures += !sfRefused("the intro at");

    // A credits table with no terminator
    sfBuildMain(&f);
    at = sfWords(2, INTRO_WORD(INTROTYPE_CREDITS), 0u);
    sfWords(1, INTRO_WORD(INTROTYPE_END));
    sfPut32(at + 4, sfCredit(1, 2, 3, 4, 5, 6));
    sfHeader(2, at);
    failures += !sfRefused("with no terminator");

    // A prop list with no end record
    sfBuildMain(&f);
    sfHeader(3, sfWords(1, 0u)); /* PROPDEF_NOTHING, then the file's end */
    failures += !sfRefused("the prop list at");
    return expect(failures == 0, "lists, names, AI code, the intro, credits and props each end inside the file");
}

static int testWrongKindsAndOverlaps(void)
{
    struct SetupFixture f;
    u32 failures = 0;

    // A pad's name inside a table
    sfBuildMain(&f);
    sfPut32(f.pads + 36, f.waypoints);
    failures += !sfRefused("inside a table");

    // An AI list inside an s32 list
    sfBuildMain(&f);
    sfPut32(f.ailists, f.wp1List + 1);
    failures += !sfRefused("inside a table");

    // A name past the file's end
    sfBuildMain(&f);
    sfPut32(f.padnames, g_Size + 1);
    failures += !sfRefused("outside the file");

    // A neighbour list running into the pad table
    sfBuildMain(&f);
    sfPut32(f.waypoints + 16 + 4, f.pads);
    failures += !sfRefused("runs into the pad table");

    // A list off the 4-byte grid
    sfBuildMain(&f);
    sfPut32(f.paths, f.path0List + 2);
    failures += !sfRefused("not a multiple of 4");

    // Two tables sharing bytes: the path table at the waypoint table
    sfBuildMain(&f);
    sfHeader(4, f.waypoints);
    failures += !sfRefused("overlaps the waypoint table");

    // A header offset off the grid, and one of 0
    sfBuildMain(&f);
    sfHeader(6, f.pads + 2);
    failures += !sfRefused("not a multiple of 4");
    sfBuildMain(&f);
    sfHeader(4, 0);
    failures += !sfRefused("0 names the header itself");
    return expect(failures == 0, "references to the wrong kind of item and items sharing bytes are refused");
}

static int testRecordTypes(void)
{
    struct SetupFixture f;
    u32 failures = 0;

    // An intro record of an unknown type
    sfBuildMain(&f);
    sfPut32(f.intro + 12, 11);
    failures += !sfRefused("has type 11");

    // A prop record of an unknown type
    sfBuildMain(&f);
    sfPut32(f.props, 60);
    failures += !sfRefused("has type 60");

    // A prop record before the end record converts (Task 15), each at its converted size
    sfBuildMain(&f);
    sfHeader(3, sfWords(2, 0u, (u32) PROPDEF_END));
    failures += sfConvert() == 0
        || AT(PropDefHeaderRecord, g_Out, AT(struct stagesetup, g_Out, 0)->propDefs)->type != PROPDEF_NOTHING
        || AT(PropDefHeaderRecord, g_Out, (uintptr_t) AT(struct stagesetup, g_Out, 0)->propDefs
               + portSetupWide(PROPDEF_NOTHING)->widewords * 4u)->type != PROPDEF_END;
    return expect(failures == 0, "unknown intro and prop types refused, a known prop record converted");
}

static int testBuffersAndShortFiles(void)
{
    struct SetupFixture f;
    u8 *inside = g_File + (8 - (uintptr_t) g_File % 8) % 8 + 8; /* 8-byte aligned, inside the file */
    u32 failures = 0;

    sfBuildMain(&f);
    failures += portSetupConvert(g_File, g_Size, NULL, SF_OUT_CAP, "UsetuptestZ") != 0
        || !contains(g_Log, "not 8-byte aligned");
    failures += portSetupConvert(g_File, g_Size, g_Out + 4, SF_OUT_CAP - 4, "UsetuptestZ") != 0
        || !contains(g_Log, "not 8-byte aligned") || !allBytes(g_Out, sizeof(g_Out), 0xAA);
    failures += portSetupConvert(g_File, g_Size, inside, SF_CAP - 16, "UsetuptestZ") != 0
        || !contains(g_Log, "overlaps the file");
    sfBuildMain(&f);
    g_Size = 36;
    failures += !sfRefused("cannot hold the 40-byte header");
    failures += portSetupConvertedSize(NULL, 64, "UsetuptestZ") != 0 || !contains(g_Log, "no file");
    // After every refusal the offset map is free again
    sfBuildMain(&f);
    failures += sfConvert() == 0;
    return expect(failures == 0, "NULL, misaligned and overlapping buffers, short and NULL files");
}

/* Whether FILE converts in exactly the bytes measured (the sanitizer sees a
 * write past them) or is refused by both entry points with nothing written. */
static u32 s_Accepted;

static int convertsWithinMeasure(const u8 *file, u32 size)
{
    const u32 n = portSetupConvertedSize(file, size, "fuzz");
    u8 *exact;
    int ok;

    if (n == 0)
    {
        __builtin_memset(g_Out, 0xAA, sizeof(g_Out));
        return portSetupConvert(file, size, g_Out, SF_OUT_CAP, "fuzz") == 0 && allBytes(g_Out, sizeof(g_Out), 0xAA);
    }
    s_Accepted++;
    exact = (u8 *) malloc(n);
    ok = exact != NULL && n % 8 == 0 && portSetupConvert(file, size, exact, n, "fuzz") == 1;
    free(exact);
    return ok;
}

static int testRandomFiles(void)
{
    static u8 bytes[512];
    u32 random = 1, round, i, failures = 0;

    for (round = 0; round < 10000; round++)
    {
        const u32 size = 40 + round % 472;
        for (i = 0; i < size; i++)
        {
            random = random * 1664525u + 1013904223u;
            bytes[i] = (u8) (random >> 24);
        }
        /* header offsets inside the file, so some walks start */
        for (i = 0; i < 10 && round % 2 == 0; i++)
        {
            random = random * 1664525u + 1013904223u;
            bytes[i * 4] = 0;
            bytes[i * 4 + 1] = 0;
            bytes[i * 4 + 2] = (u8) (((random >> 8) % size) >> 8);
            bytes[i * 4 + 3] = (u8) (((random >> 8) % size) & 0xFC);
        }
        failures += !convertsWithinMeasure(bytes, size);
    }
    return expect(failures == 0, "10000 random files convert within their measure or are refused");
}

static int testMutatedFiles(void)
{
    static u8 bytes[SF_CAP];
    struct SetupFixture f;
    u32 random = 7, round, i, size, failures = 0, before = s_Accepted;

    sfBuildMain(&f);
    size = g_Size;
    for (round = 0; round < 10000; round++)
    {
        __builtin_memcpy(bytes, g_File, size);
        for (i = 0; i <= round % 4; i++)
        {
            random = random * 1664525u + 1013904223u;
            bytes[(random >> 8) % size] = (u8) (random >> 24);
        }
        failures += !convertsWithinMeasure(bytes, size);
    }
    printf("test_romconv_setup_checks: %u of 10000 mutated files accepted\n", s_Accepted - before);
    return expect(failures == 0 && s_Accepted > before, "10000 mutated files convert within their measure or are refused");
}

int main(void)
{
    int failures = testBriefRefusals() + testEveryTableWalkIsBounded() + testEveryOtherWalkIsBounded()
        + testWrongKindsAndOverlaps() + testRecordTypes() + testBuffersAndShortFiles() + testRandomFiles()
        + testMutatedFiles();

    if (failures != 0)
    {
        printf("test_romconv_setup_checks: %d failed\n", failures);
        return 1;
    }
    printf("test_romconv_setup_checks: OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
