/* The setup file converter's tables and intro (LP64-004 Task 14), on a file
 * built in the test: every table and every kind of reference lands where the
 * game's readers look for it, the intro walks by sizeof to its end, and at
 * 32 bits the image is romfile_setup.c's in-place result. Run at both widths
 * by test_romconv.sh. */
#include "test_romconv_setup_build.h"

/* bondview.h's PC INTRO_TYPE, as the game's intro walks read a type (that
 * header does not build under these warnings). */
#define INTRO_TYPE(rec) ((s32) (((u32) (rec)->type) >> 24))

#if !IS_64_BIT
/* romfile_setup.c, linked into the 32-bit half: the in-place pass. */
s32 romfileSetupConvert(u8 *data, u32 size, const char *name, const u8 *compiled);
#endif

static struct stagesetup *header(void)
{
    return (struct stagesetup *) (void *) g_Out;
}

static int testBrief(void)
{
    // Arrange
    struct SetupFixture f;
    struct stagesetup *s;
    PadRecord *pads;
    AIListRecord *ai;
    u32 n;

    sfBuildMain(&f);

    // Act
    n = portSetupConvertedSize(g_File, g_Size, "UsetuptestZ");
    s = header();

    // Assert
    if (n == 0 || n > SF_OUT_CAP || portSetupConvert(g_File, g_Size, g_Out, n, "UsetuptestZ") != 1)
    {
        return expect(0, "the brief's fixture converts");
    }
    pads = AT(PadRecord, g_Out, s->pads);
    ai = AT(AIListRecord, g_Out, s->ailists);
    return expect(pads[1].pos.z == -40.0f && (uintptr_t) pads[1].plink != 0 && pads[2].plink == NULL
            && sameText((char *) g_Out + (uintptr_t) pads[1].plink, "p1") && ai[1].ID == 0x0401
            && g_Out[(uintptr_t) ai[1].ailist] == 0xAD,
        "the brief's assertions: pad 1's place and name, the terminator's NULL, AI list 1's code");
}

static int testWritesExactlyTheMeasuredBytes(void)
{
    // Arrange
    struct SetupFixture f;
    u8 *exact;
    u32 n;
    int ok;

    sfBuildMain(&f);
    n = portSetupConvertedSize(g_File, g_Size, "UsetuptestZ");
    exact = (u8 *) malloc(n); /* exactly the size measured: the sanitizer sees any write past it */

    // Act
    ok = n != 0 && n % 8 == 0 && exact != NULL && portSetupConvert(g_File, g_Size, exact, n, "UsetuptestZ") == 1
        && portSetupConvert(g_File, g_Size, g_Out, n, "UsetuptestZ") == 1;

    // Assert: at 32 bits the credits record holds its table's address in each buffer
#if !IS_64_BIT
    __builtin_memcpy(exact + f.intro + 12 + 40 + 4, g_Out + f.intro + 12 + 40 + 4, 4);
#endif
    ok = ok && sameBytes(exact, g_Out, n) && allBytes(g_Out + n, SF_OUT_CAP - n, 0xAA);
    free(exact);
    return expect(ok, "the image is exactly the bytes measured, a multiple of 8, the same in any buffer");
}

static int testHeaderNamesEveryTable(void)
{
    // Arrange
    struct SetupFixture f;
    struct stagesetup *s = header();
    const u32 align = (u32) sizeof(void *);
    int ok;

    sfBuildMain(&f);

    // Act
    ok = sfConvert() != 0;

    // Assert: each table starts on its record's alignment, after the header
    ok = ok && (uintptr_t) s->pads % align == 0 && (uintptr_t) s->boundpads % align == 0
        && (uintptr_t) s->pathwaypoints % align == 0 && (uintptr_t) s->waypointgroups % align == 0
        && (uintptr_t) s->patrolpaths % align == 0 && (uintptr_t) s->ailists % align == 0
        && (uintptr_t) s->padnames % align == 0 && (uintptr_t) s->boundpadnames % align == 0
        && (uintptr_t) s->intro % align == 0 && (uintptr_t) s->propDefs % align == 0
        && (uintptr_t) s->pads >= sizeof(struct stagesetup) + 8 /* the names come first */
        && AT(PropDefHeaderRecord, g_Out, s->propDefs)->type == PROPDEF_END
        && AT(SetupIntroEmpty, g_Out, s->intro)->type == 0 /* a spawn first */;
    return expect(ok, "every header field is its table's offset in the image, aligned for its records");
}

static int testPads(void)
{
    // Arrange
    struct SetupFixture f;
    PadRecord *pads;
    BoundPadRecord *bound;
    int ok;

    sfBuildMain(&f);

    // Act
    ok = sfConvert() != 0;
    pads = AT(PadRecord, g_Out, header()->pads);
    bound = AT(BoundPadRecord, g_Out, header()->boundpads);

    // Assert: names at odd offsets and a name that is another's tail resolve
    ok = ok && pads[0].pos.x == 10.0f && pads[0].pos.y == 20.0f && pads[0].look.z == 1.0f
        && sameText((char *) g_Out + (uintptr_t) pads[0].plink, "p0") && (uintptr_t) pads[0].plink % 2 == 1
        && (uintptr_t) pads[0].stan == 0x51 && pads[1].plink == pads[0].plink + 3
        && bound[0].pos.x == 5.0f && bound[0].bbox.xmin == -7.0f && bound[0].bbox.zmax == -2.0f
        && sameText((char *) g_Out + (uintptr_t) bound[0].plink, "1") && bound[0].plink == pads[1].plink + 1
        && bound[1].plink == NULL;
    return expect(ok, "pads and bound pads: floats, names (odd, shared tails), the stan word, the box");
}

static int testWaypointsAndLists(void)
{
    // Arrange
    struct SetupFixture f;
    waypoint *wp;
    s32 *l0, *l1, *l2;
    int ok;

    sfBuildMain(&f);

    // Act
    ok = sfConvert() != 0;
    wp = AT(waypoint, g_Out, header()->pathwaypoints);
    l0 = AT(s32, g_Out, wp[0].neighbours);
    l1 = AT(s32, g_Out, wp[1].neighbours);
    l2 = AT(s32, g_Out, wp[2].neighbours);

    // Assert: waypoint 2's list starts inside waypoint 0's
    ok = ok && wp[0].padID == 0 && wp[2].padID == 2 && wp[3].padID == -1 && l0[0] == 1 && l0[1] == 2 && l0[2] == -1
        && l1[0] == 0 && l1[1] == -1 && l2 == l0 + 1 && (uintptr_t) wp[0].neighbours % 4 == 0;
    return expect(ok, "waypoints: pads, neighbour lists, a list named from inside another, the terminator");
}

static int testWaygroupsAndPaths(void)
{
    // Arrange
    struct SetupFixture f;
    waygroup *wg;
    PathRecord *path;
    s32 *points, *pathPoints;
    int ok;

    sfBuildMain(&f);

    // Act
    ok = sfConvert() != 0;
    wg = AT(waygroup, g_Out, header()->waypointgroups);
    path = AT(PathRecord, g_Out, header()->patrolpaths);
    points = AT(s32, g_Out, wg[0].waypoints);
    pathPoints = AT(s32, g_Out, path[0].waypoints);

    // Assert
    ok = ok && AT(s32, g_Out, wg[0].neighbours)[0] == -1 && points[0] == 0 && points[2] == 2 && points[3] == -1
        && wg[1].neighbours == NULL && pathPoints[0] == 0 && pathPoints[1] == 1 && pathPoints[2] == -1
        && path[0].ID == 3 && path[0].isLoop == 1 && path[0].len == 5 && path[1].waypoints == NULL;
    return expect(ok, "waygroups and paths: both lists, ID, loop flag and length, the terminators");
}

static int testAiListsAndNameTables(void)
{
    // Arrange
    struct SetupFixture f;
    AIListRecord *ai;
    pname *names, *boundNames;
    int ok;

    sfBuildMain(&f);

    // Act
    ok = sfConvert() != 0;
    ai = AT(AIListRecord, g_Out, header()->ailists);
    names = AT(pname, g_Out, header()->padnames);
    boundNames = AT(pname, g_Out, header()->boundpadnames);

    // Assert: list 0 at an odd offset, list 1 starting inside its code
    ok = ok && ai[0].ID == AI0_ID && (uintptr_t) ai[0].ailist % 2 == 1 && g_Out[(uintptr_t) ai[0].ailist] == 0x2A
        && ai[1].ailist == (AIRecord *) ((uintptr_t) ai[0].ailist + 1)
        && g_Out[(uintptr_t) ai[0].ailist + 3] == (u8) AI_EndList && ai[2].ailist == NULL
        && sameText((char *) g_Out + (uintptr_t) names[0].p, "p0")
        && sameText((char *) g_Out + (uintptr_t) names[1].p, "p1") && names[2].p == NULL
        && sameText((char *) g_Out + (uintptr_t) boundNames[0].p, "1") && boundNames[1].p == NULL;
    return expect(ok, "AI lists (odd, nested) with their code, and both name tables");
}

/* The intro walk of bondview_r.c: by each record's sizeof, 4 for an unknown type. */
static u32 walkIntro(u8 *intro, u32 *types, u32 max)
{
    SetupIntroEmpty *rec = (SetupIntroEmpty *) (void *) intro;
    u32 n = 0, step;

    while (n < max && INTRO_TYPE(rec) != INTROTYPE_END)
    {
        types[n++] = (u32) INTRO_TYPE(rec);
        switch (INTRO_TYPE(rec))
        {
        case INTROTYPE_SPAWN: step = sizeof(SetupIntroSpawn); break;
        case INTROTYPE_CAMERA: step = sizeof(SetupIntroCamera); break;
        case INTROTYPE_CREDITS: step = sizeof(struct SetupIntroCredits); break;
        default: step = sizeof(SetupIntroEmpty); break;
        }
        rec = (SetupIntroEmpty *) (void *) ((u8 *) rec + step);
    }
    types[n] = (u32) INTRO_TYPE(rec);
    return n;
}

static int testIntroWalksBySizeof(void)
{
    // Arrange
    struct SetupFixture f;
    u32 types[8], n;
    int ok;

    sfBuildMain(&f);

    // Act
    ok = sfConvert() != 0;
    n = ok ? walkIntro(g_Out + (uintptr_t) header()->intro, types, 7) : 0;

    // Assert: at 64 bits one pad word of type INTROTYPE_MAX keeps the camera aligned
#if IS_64_BIT
    ok = ok && n == 4 && types[0] == INTROTYPE_SPAWN && types[1] == INTROTYPE_MAX && types[2] == INTROTYPE_CAMERA
        && types[3] == INTROTYPE_CREDITS && types[4] == INTROTYPE_END;
#else
    ok = ok && n == 3 && types[0] == INTROTYPE_SPAWN && types[1] == INTROTYPE_CAMERA
        && types[2] == INTROTYPE_CREDITS && types[3] == INTROTYPE_END;
#endif
    return expect(ok, "the intro walked by sizeof visits the spawn, the camera and the credits, then the end");
}

static int testCameraAndCredits(void)
{
    // Arrange
    struct SetupFixture f;
    u8 *intro;
    SetupIntroCamera *camera;
    struct SetupIntroCredits *rec;
    CreditsEntry *table;
    int ok;

    sfBuildMain(&f);

    // Act
    ok = sfConvert() != 0;
    intro = g_Out + (uintptr_t) header()->intro;
    camera = (SetupIntroCamera *) (void *) (intro + (IS_64_BIT ? 16 : 12));
    rec = (struct SetupIntroCredits *) (void *) ((u8 *) camera + sizeof(SetupIntroCamera));
#if IS_64_BIT
    table = (CreditsEntry *) ((uintptr_t) g_Out + rec->unk04); /* bondview_r.c's 64-bit read */
#else
    __builtin_memcpy(&table, &rec->unk04, sizeof(table));
#endif

    // Assert: the text IDs in the low words of their unions, the table found
    ok = ok && INTRO_TYPE(camera) == INTROTYPE_CAMERA && (uintptr_t) camera % _Alignof(SetupIntroCamera) == 0
        && camera->unk04.ival == 100 && camera->unk0C.ival == 300 && camera->unk14.ival == 500 && camera->unk18 == 6
        && camera->lang1c.lang_index[0] == CAM_TEXT1 && camera->lang20.lang_index == (s32) CAM_TEXT2
        && (uintptr_t) camera->lang1c.lang_ptr == CAM_TEXT1 && (uintptr_t) camera->lang20.lang_ptr == CAM_TEXT2
        && camera->prev == NULL && INTRO_TYPE(rec) == INTROTYPE_CREDITS
        && (uintptr_t) rec % _Alignof(struct SetupIntroCredits) == 0 && table[0].TextId1 == 0x0101
        && table[0].TextId2 == 0x0102 && table[0].Position1 == 10 && table[0].Alignment2 == 2
        && table[1].TextId1 == 0x0103 && table[1].Position2 == 40 && table[2].TextId1 == 0 && table[2].TextId2 == 0;
    return expect(ok, "the camera's scalars and text IDs, and the credits record's table");
}

/* bondview2.c's intro walk steps 4 bytes over a credits record. At 64 bits it
 * then reads the record's padding word, which must read as a SPAWN (12 bytes)
 * so that the walk lands on the next record. */
static int testBondview2StepsOverCredits(void)
{
    // Arrange
    struct SetupFixture f;
    u8 *rec;
    s32 padding;
    int ok;

    sfBuildMain(&f);

    // Act
    ok = sfConvert() != 0;
    rec = g_Out + (uintptr_t) header()->intro + (IS_64_BIT ? 16 : 12) + sizeof(SetupIntroCamera);
    __builtin_memcpy(&padding, rec + 4, sizeof(padding));

    // Assert
    ok = ok && (!IS_64_BIT
        || (padding == 0 && 4 + sizeof(struct SetupIntroSpawn) == sizeof(struct SetupIntroCredits)
            && INTRO_TYPE((SetupIntroEmpty *) (void *) (rec + 16)) == INTROTYPE_END));
    return expect(ok, "at 64 bits bondview2.c's walk over the credits record lands on the next record");
}

static int testMatchesInPlaceAt32Bits(void)
{
#if IS_64_BIT
    return 0; /* the in-place pass exists only at 32 bits */
#else
    // Arrange
    struct SetupFixture f;
    static u8 copy[SF_CAP] __attribute__((aligned(16)));
    u32 n, word, i;
    u8 *inPlace, *converted;
    int ok;

    /* Waypoint 2 gets a list of its own: the in-place pass swaps a list word
     * once for each list that covers it, so a list named from inside another
     * is left half big-endian there (the converter swaps each word once). */
    sfBuildFixture(&f, 0);
    __builtin_memcpy(copy, g_File, g_Size);
    /* the credits record's table word, after the spawn and the camera (32-bit records are the file's) */
    word = f.intro
        + (u32) (sizeof(SetupIntroSpawn) + sizeof(SetupIntroCamera) + offsetof(struct SetupIntroCredits, unk04));

    // Act
    n = sfConvert();
    ok = romfileSetupConvert(copy, g_Size, "UsetuptestZ", NULL) == 1;

    // Assert: every byte equal but the table's address, each its own buffer's
    __builtin_memcpy(&inPlace, copy + word, 4);
    __builtin_memcpy(&converted, g_Out + word, 4);
    ok = ok && n == ((g_Size + 7) & ~7u) && inPlace == copy + f.credits && converted == g_Out + f.credits
        && allBytes(g_Out + g_Size, n - g_Size, 0);
    for (i = 0; ok && i < g_Size; i++)
    {
        ok = (i >= word && i < word + 4) || copy[i] == g_Out[i];
    }
    return expect(ok, "at 32 bits the image is the in-place result byte for byte");
#endif
}

int main(void)
{
    int failures = testBrief() + testWritesExactlyTheMeasuredBytes() + testHeaderNamesEveryTable() + testPads()
        + testWaypointsAndLists() + testWaygroupsAndPaths() + testAiListsAndNameTables() + testIntroWalksBySizeof()
        + testCameraAndCredits() + testBondview2StepsOverCredits() + testMatchesInPlaceAt32Bits();

    if (failures != 0)
    {
        printf("test_romconv_setup: %d failed\n", failures);
        return 1;
    }
    printf("test_romconv_setup: OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
