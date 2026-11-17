/* The setup file converter's intro, one record of every kind (LP64-004 Task
 * 14): each record's words land in its struct's fields in native order, at
 * 64 bits the pad words fall where the camera and the credits record need
 * them, and the intro walks of bondview_r.c, prop.c and bondview2.c, each
 * reproduced here by its own stepping rule, go where the game's would. At 32
 * bits the image is romfile_setup.c's in-place result. A prop converter
 * double (linked instead of romconv_setup_props.c) shows that a prop list the
 * write pass would refuse is refused by the checks, with nothing written. Run
 * at both widths by test_romconv.sh. */
#include "test_romconv_setup_build.h"

/* bondview.h's PC INTRO_TYPE (that header does not build under these warnings). */
#define INTRO_TYPE(rec) ((s32) (((u32) (rec)->type) >> 24))

#if !IS_64_BIT
s32 romfileSetupConvert(u8 *data, u32 size, const char *name, const u8 *compiled);
#endif

/* ==== the prop converter double ========================================= */

/* Accepts only the end record alone, as the stubs do; with s_PropsWriteRefuses
 * set, its conversion refuses what its size accepted. */
static int s_PropsWriteRefuses;

u32 portSetupPropsSize(const u8 *props, u32 size)
{
    return props != NULL && size == 4 && props[3] == PROPDEF_END ? (u32) sizeof(void *) : 0;
}

s32 portSetupPropsConvert(const u8 *props, u32 size, u8 *out, u32 outsize)
{
    if (s_PropsWriteRefuses || portSetupPropsSize(props, size) == 0 || outsize < sizeof(void *))
    {
        return 0;
    }
    __builtin_memset(out, 0, sizeof(void *));
    out[0] = props[1];
    out[1] = props[0];
    out[2] = props[2];
    out[3] = props[3];
    return 1;
}

/* ==== the intro of every kind =========================================== */

/* Each record's words as the file holds them (big-endian values), every value
 * different and changed by a byte swap. A type word is a u16, a byte and the
 * type (KSK) but the spawn's (K32, so its first byte is 0); WATCH's hours are
 * small, as the game's are, so bondview2.c's misstep there is the game's. */
#define MAX_WORDS 10
static const struct
{
    u32 type, words, w[MAX_WORDS];
} kRecords[] = {
    {INTROTYPE_SPAWN, 3, {0x00123400u, 0x0000A1B2u, 0x0000C3D4u}},
    {INTROTYPE_ITEM, 4, {0x21430001u, 0x00000D17u, 0x00000E28u, 0x00000F39u}},
    {INTROTYPE_AMMO, 4, {0x65870002u, 0x0000011Au, 0x0000022Bu, 0x0000033Cu}},
    {INTROTYPE_SWIRL, 8,
        {0xA9CB0003u, 0x0000044Du, 0x00015E6Fu, 0x00026F70u, 0xFFFE7081u, 0x00018192u, 0x000292A3u, 0x0000A3B4u}},
    {INTROTYPE_ANIM, 2, {0xEDCF0004u, 0x000000B5u}},
    {INTROTYPE_CUFF, 2, {0x13570005u, 0x000000C6u}},
    {INTROTYPE_CAMERA, 10,
        {0x24680006u, 0x0007D1E2u, 0x0008E2F3u, 0xFFF9F304u, 0x00010415u, 0x00021526u, 0x00000637u, 0x0000AB12u,
            0x0000CD34u, 0}},
    {INTROTYPE_WATCH, 3, {0x35790007u, 0x0000000Bu, 0x0000002Du}},
    {INTROTYPE_CREDITS, 2, {0x468A0008u, 0 /* the credits table's offset, filled in */}},
    {INTROTYPE_END, 1, {0x579B0009u}},
};
#define RECORDS (sizeof(kRecords) / sizeof(kRecords[0]))
#define CAMERA 6
#define WATCH 7
#define CREDITS 8
#define END 9

/* Where each record starts in the image, from the intro's start. At 64 bits
 * a pad word goes at 92 (after the 8-byte CUFF, before the camera) and at 164
 * (after the 12-byte WATCH, before the credits record). */
#if IS_64_BIT
static const u32 kAt[RECORDS] = {0, 12, 28, 44, 76, 84, 96, 152, 168, 184};
static const u32 kPads[] = {92, 164};
#define PADS 2u
#else
static const u32 kAt[RECORDS] = {0, 12, 28, 44, 76, 84, 92, 132, 144, 152};
static const u32 kPads[] = {0};
#define PADS 0u
#endif
/* PADS as an object, so that a 32-bit comparison with it is not always false. */
static const u32 kPadCount = PADS;

/* The main fixture (no nested lists) with its intro replaced by the records
 * of kRecords that ORDER names, appended after it; the old intro's bytes
 * stay, unowned and unread. */
static void sfBuildIntro(struct SetupFixture *f, const u32 *order, u32 count)
{
    u32 i, k;

    sfBuildFixture(f, 0);
    f->intro = g_Size;
    for (i = 0; i < count; i++)
    {
        for (k = 0; k < kRecords[order[i]].words; k++)
        {
            sfWords(1, order[i] == CREDITS && k == 1 ? f->credits : kRecords[order[i]].w[k]);
        }
    }
    sfHeader(2, f->intro);
}

/* One record of every kind, in kRecords' order. */
static void sfBuildEveryKind(struct SetupFixture *f)
{
    static const u32 order[RECORDS] = {0, 1, 2, 3, 4, 5, CAMERA, WATCH, CREDITS, END};

    sfBuildIntro(f, order, RECORDS);
}

/* A type word swapped as romfile_setup.c's KSK: the u16's two bytes reversed,
 * then the byte and the type, read as a native word. */
static u32 nativeKsk(u32 w)
{
    return ((w >> 16) & 0xFFu) | (((w >> 24) & 0xFFu) << 8) | (((w >> 8) & 0xFFu) << 16) | ((w & 0xFFu) << 24);
}

static u32 word(const u8 *p)
{
    u32 v;

    __builtin_memcpy(&v, p, 4);
    return v;
}

static u8 *convertedIntro(void)
{
    return g_Out + (uintptr_t) ((struct stagesetup *) (void *) g_Out)->intro;
}

/* ==== the walks the game has ============================================ */

enum Walker
{
    BONDVIEW_R,
    PROP_C,
    BONDVIEW2
};

/* The bytes WALKER steps from a record of TYPE (0 at the end). bondview_r.c
 * and prop.c (portMeasureIntro, in words) have an arm for every type;
 * bondview2.c has arms for 0 to 6 and steps 4 for WATCH, CREDITS and any
 * other type, as its default arm does. */
static u32 step(enum Walker walker, s32 type)
{
    switch (type)
    {
    case INTROTYPE_SPAWN: return sizeof(SetupIntroSpawn);
    case INTROTYPE_ITEM: return sizeof(struct SetupIntroItem);
    case INTROTYPE_AMMO: return sizeof(struct SetupIntroAmmo);
    case INTROTYPE_SWIRL: return sizeof(struct SetupIntroSwirl);
    case INTROTYPE_ANIM: return sizeof(struct SetupIntroAnim);
    case INTROTYPE_CUFF: return sizeof(struct SetupIntroCuff);
    case INTROTYPE_CAMERA: return sizeof(SetupIntroCamera);
    case INTROTYPE_WATCH: return walker == BONDVIEW2 ? sizeof(SetupIntroEmpty) : sizeof(struct SetupIntroWatch);
    case INTROTYPE_CREDITS:
        return walker == BONDVIEW2 ? sizeof(SetupIntroEmpty) : sizeof(struct SetupIntroCredits);
    case INTROTYPE_END: return 0;
    default: return sizeof(SetupIntroEmpty);
    }
}

/* Walks the converted intro as WALKER does, recording each offset it reads a
 * type at into SEEN (at most MAX); stops at END or past the intro. Returns
 * the number of offsets seen. */
static u32 walk(enum Walker walker, u32 *seen, u32 max)
{
    const u8 *intro = convertedIntro();
    u32 at = 0, n = 0, by;

    while (n < max && at <= kAt[END])
    {
        seen[n++] = at;
        by = step(walker, INTRO_TYPE((const SetupIntroEmpty *) (const void *) (intro + at)));
        if (by == 0)
        {
            break;
        }
        at += by;
    }
    return n;
}

static int sameOffsets(const u32 *seen, u32 n, const u32 *want, u32 count)
{
    u32 i;

    for (i = 0; i < count && i < n && seen[i] == want[i]; i++)
    {
    }
    return i == count;
}

/* ==== tests ============================================================= */

static int testEveryRecordsWords(void)
{
    // Arrange
    struct SetupFixture f;
    const u8 *rec;
    const SetupIntroCamera *camera;
    u32 i, k, failures = 0;

    sfBuildEveryKind(&f);

    // Act
    failures += sfConvert() == 0;

    // Assert: the type word by KSK (the spawn's K32), every other word in native order
    for (i = 0; failures == 0 && i < RECORDS; i++)
    {
        rec = convertedIntro() + kAt[i];
        if (word(rec) != (i == 0 ? kRecords[i].w[0] : nativeKsk(kRecords[i].w[0]))
            || INTRO_TYPE((const SetupIntroEmpty *) (const void *) rec) != (s32) kRecords[i].type)
        {
            printf("test_romconv_setup_intro: record %u's type word is 0x%08X\n", i, word(rec));
            failures++;
        }
        for (k = 1; i != CAMERA && i != CREDITS && k < kRecords[i].words; k++)
        {
            failures += word(rec + k * 4) != kRecords[i].w[k];
        }
    }
    camera = (const SetupIntroCamera *) (const void *) (convertedIntro() + kAt[CAMERA]);
    for (k = 1; failures == 0 && k < 7; k++)
    {
        failures += word((const u8 *) camera + k * 4) != kRecords[CAMERA].w[k];
    }
    failures += failures == 0
        && ((uintptr_t) camera->lang1c.lang_ptr != kRecords[CAMERA].w[7]
            || (uintptr_t) camera->lang20.lang_ptr != kRecords[CAMERA].w[8] || camera->prev != NULL);
    return expect(failures == 0, "every intro record's words, each in its own field in native order");
}

static int testCreditsRecordFindsItsTable(void)
{
    // Arrange
    struct SetupFixture f;
    const struct SetupIntroCredits *rec;
    const CreditsEntry *table;
    int ok;

    sfBuildEveryKind(&f);

    // Act
    ok = sfConvert() != 0;
    rec = (const struct SetupIntroCredits *) (const void *) (convertedIntro() + kAt[CREDITS]);
#if IS_64_BIT
    table = (const CreditsEntry *) (g_Out + rec->unk04);
#else
    __builtin_memcpy(&table, &rec->unk04, sizeof(table));
#endif

    // Assert: the table read only once it is known to lie inside OUT
    ok = ok && (uintptr_t) rec % _Alignof(struct SetupIntroCredits) == 0 && (uintptr_t) table % 2 == 0
        && (const u8 *) table >= g_Out && (const u8 *) (table + 3) <= g_Out + sizeof(g_Out) && table[0].TextId1 == 0x0101
        && table[1].TextId2 == 0x0104 && table[2].TextId1 == 0;
    return expect(ok, "the credits record after a WATCH finds its table");
}

static int testPadWordsAt64Bits(void)
{
    // Arrange
    struct SetupFixture f;
    const u8 *intro;
    u32 i, place, p, pads = 0;
    int ok;

    sfBuildEveryKind(&f);

    // Act
    ok = sfConvert() != 0;
    intro = convertedIntro();

    // Assert: each record where expected, a pad word in each gap and nowhere else
    for (i = 0, place = 0; ok && i < RECORDS; i++)
    {
        for (p = place; ok && p < kAt[i]; p += 4, pads++)
        {
            ok = pads < kPadCount && p == kPads[pads] && word(intro + p) == (u32) INTROTYPE_MAX << 24;
        }
        ok = ok && (kAt[i] % (i == CAMERA || i == CREDITS ? sizeof(void *) : 4)) == 0;
        place = kAt[i] + (i == CAMERA ? sizeof(SetupIntroCamera)
                          : i == CREDITS ? sizeof(struct SetupIntroCredits) : kRecords[i].words * 4);
    }
    ok = ok && pads == kPadCount;
    return expect(ok, "pad words before the camera (after a CUFF) and the credits (after a WATCH), and only there");
}

static int testBondviewRAndPropWalksReachTheEnd(void)
{
    // Arrange
    struct SetupFixture f;
    u32 want[RECORDS + PADS], seen[32], i, n = 0, pads = 0, nr, np;
    int ok;

    sfBuildEveryKind(&f);
    for (i = 0; i < RECORDS; i++)
    {
        while (pads < kPadCount && kPads[pads] < kAt[i])
        {
            want[n++] = kPads[pads++];
        }
        want[n++] = kAt[i];
    }

    // Act
    ok = sfConvert() != 0;
    nr = ok ? walk(BONDVIEW_R, seen, 32) : 0;
    ok = ok && nr == n && sameOffsets(seen, nr, want, n);
    np = ok ? walk(PROP_C, seen, 32) : 0;

    // Assert: every record and pad word in turn, then the end
    ok = ok && np == n && sameOffsets(seen, np, want, n);
    return expect(ok, "bondview_r.c's and prop.c's walks visit every record and pad word and reach the end");
}

/* bondview2.c steps 4 into a WATCH and reads its hours (11, so type 0) as a
 * SPAWN, stepping 12: it reads its next type 4 bytes past the WATCH's end. At
 * 64 bits that is the credits record, past the pad word, and it then steps 4
 * into the credits record and reads its zero padding as a SPAWN, landing on
 * the end. At 32 bits it is the credits record's table address, whose top
 * byte, an address's, decides the rest; the test follows it that far. */
static int testBondview2Walk(void)
{
    // Arrange
    struct SetupFixture f;
#if IS_64_BIT
    static const u32 want[] = {0, 12, 28, 44, 76, 84, 92, 96, 152, 156, 168, 172, 184};
#else
    static const u32 want[] = {0, 12, 28, 44, 76, 84, 92, 132, 136, 148};
#endif
    const u32 count = sizeof(want) / sizeof(want[0]);
    u32 seen[32], n;
    int ok;

    sfBuildEveryKind(&f);

    // Act
    ok = sfConvert() != 0;
    n = ok ? walk(BONDVIEW2, seen, 32) : 0;

    // Assert
    ok = ok && sameOffsets(seen, n, want, count)
        && (IS_64_BIT ? n == count && seen[n - 1] == kAt[END] : want[count - 1] == kAt[CREDITS] + 4);
    return expect(ok, "bondview2.c's walk, with its inherited WATCH misstep, goes where the game's does");
}

/* Stock Dam's order: a WATCH followed by a SPAWN, which needs no pad, then an
 * ITEM. bondview2.c reads the WATCH's hours as a SPAWN and the next types 4
 * bytes past each record's start, at both widths alike: it skips the SPAWN
 * and the ITEM (whose item_right word it reads as a SPAWN) and lands on the
 * end. bondview_r.c visits each record. */
static int testWatchBeforeSpawnOvershootsAtBothWidths(void)
{
    // Arrange
    static const u32 order[] = {WATCH, 0, 1, END};
    static const u32 records[] = {0, 12, 24, 40}, bondview2[] = {0, 4, 16, 28, 40};
    struct SetupFixture f;
    u32 seen[32], n, m;
    int ok;

    sfBuildIntro(&f, order, sizeof(order) / sizeof(order[0]));

    // Act
    ok = sfConvert() != 0;
    n = ok ? walk(BONDVIEW_R, seen, 32) : 0;
    ok = ok && n == 4 && sameOffsets(seen, n, records, 4);
    m = ok ? walk(BONDVIEW2, seen, 32) : 0;

    // Assert
    ok = ok && m == 5 && sameOffsets(seen, m, bondview2, 5);
    return expect(ok, "a WATCH before a SPAWN: bondview2.c overshoots by 4 at both widths");
}

static int testMatchesInPlaceAt32Bits(void)
{
#if IS_64_BIT
    return 0; /* the in-place pass exists only at 32 bits */
#else
    // Arrange
    struct SetupFixture f;
    static u8 copy[SF_CAP] __attribute__((aligned(16)));
    u32 n, table, i;
    int ok;

    sfBuildEveryKind(&f);
    __builtin_memcpy(copy, g_File, g_Size);
    table = f.intro + kAt[CREDITS] + (u32) offsetof(struct SetupIntroCredits, unk04);

    // Act
    n = sfConvert();
    ok = romfileSetupConvert(copy, g_Size, "UsetuptestZ", NULL) == 1;

    // Assert: every byte equal but the credits table's address, each its own buffer's
    ok = ok && n == ((g_Size + 7) & ~7u) && word(copy + table) == (u32) (uintptr_t) (copy + f.credits)
        && word(g_Out + table) == (u32) (uintptr_t) (g_Out + f.credits);
    for (i = 0; ok && i < g_Size; i++)
    {
        ok = (i >= table && i < table + 4) || copy[i] == g_Out[i];
    }
    return expect(ok, "at 32 bits the intro of every kind is the in-place result byte for byte");
#endif
}

static int testPropsRefusalIsTheChecks(void)
{
    // Arrange
    struct SetupFixture f;
    int ok;

    sfBuildEveryKind(&f);
    s_PropsWriteRefuses = 1;

    // Act and Assert: refused by the measure too, and OUT left as it was
    ok = sfRefused("the prop converter refused");
    s_PropsWriteRefuses = 0;
    return expect(ok, "a prop list the prop converter would refuse is refused before OUT is written");
}

int main(void)
{
    int failures = testEveryRecordsWords() + testCreditsRecordFindsItsTable() + testPadWordsAt64Bits()
        + testBondviewRAndPropWalksReachTheEnd() + testBondview2Walk() + testWatchBeforeSpawnOvershootsAtBothWidths()
        + testMatchesInPlaceAt32Bits()
        + testPropsRefusalIsTheChecks();

    if (failures != 0)
    {
        printf("test_romconv_setup_intro: %d failed\n", failures);
        return 1;
    }
    printf("test_romconv_setup_intro: OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
