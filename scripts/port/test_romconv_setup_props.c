/* The setup file converter's prop records (LP64-004 Task 15): portSetupWide's
 * table against the compiler, for every type with a C record (each pointer
 * word's place is the offsetof of the member it becomes, each record's size
 * the record's sizeof, padded to a pointer), and a prop list built here from
 * invented numbers, converted and walked as the game walks it. Run at both
 * widths by test_romconv.sh; nothing is read from a ROM. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "bondconstants.h"
#include "bondview.h" /* textoverride, the record RENAME is linked as */
#include "platform_info.h"
#include "port_romconv_stage.h"
#include "../../port/generated/setup_kinds.h"

#define PTR ((size_t) sizeof(void *))
#define ROUND_PTR(n) (((size_t) (n) + PTR - 1) & ~(PTR - 1))

static int s_Failures;

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

static void check(int ok, const char *what, int line)
{
    if (!ok)
    {
        printf("test_romconv_setup_props: FAIL line %d: %s\n", line, what);
        fflush(stdout);
        s_Failures++;
    }
}
#define CHECK(cond) check((cond), #cond, __LINE__)

/* ==== the rule, as the test states it ================================== */

/* Where the rule puts file word WORD of a record (WORD == filewords: its end
 * before padding): a pointer word at the next multiple of the pointer size
 * and pointer-sized, every other word 4 bytes. */
static size_t ruleOffset(const struct PortSetupWide *w, u32 word)
{
    size_t place = 0;
    u32 i, p = 0;

    for (i = 0; i <= word && i < w->filewords; i++)
    {
        const int pointer = p < w->npointers && w->pointerword[p] == i;

        if (pointer)
        {
            place = ROUND_PTR(place);
            p++;
        }
        if (i == word)
        {
            return place;
        }
        place += pointer ? PTR : 4;
    }
    return place;
}

static size_t layout_from_pointer_words(const struct PortSetupWide *w)
{
    return ROUND_PTR(ruleOffset(w, w->filewords));
}

/* The record's size by the rule is its WIDEWORDS, and its C record's sizeof,
 * padded to a pointer. */
#define CHECK_WIDE(type, T) do { \
        const struct PortSetupWide *w = portSetupWide(type); \
        CHECK(layout_from_pointer_words(w) == (size_t) w->widewords * 4); \
        CHECK((size_t) w->widewords * 4 == ROUND_PTR(sizeof(T))); \
    } while (0)
/* For a type whose C record is not the file's record: the rule's size only;
 * its member places are the check. */
#define CHECK_RULE_ONLY(type) do { \
        const struct PortSetupWide *w = portSetupWide(type); \
        CHECK(layout_from_pointer_words(w) == (size_t) w->widewords * 4); \
    } while (0)
/* File word WORD of TYPE lands on MEMBER of T. */
#define CHECK_AT(type, word, T, member) CHECK(ruleOffset(portSetupWide(type), (word)) == offsetof(T, member))
#define CHECK_OBJECT(type, T) do { \
        CHECK_AT(type, 4, T, prop); CHECK_AT(type, 5, T, model); \
        CHECK_AT(type, 26, T, ptr_allocated_collisiondata_block); CHECK_AT(type, 27, T, projectile); \
        CHECK_AT(type, 28, T, maxdamage); CHECK_AT(type, 29, T, damage); \
    } while (0)

static void testObjectTypes(void)
{
    static const u32 plain[] = {PROPDEF_PROP, PROPDEF_ALARM, PROPDEF_RACK, PROPDEF_HAT, PROPDEF_GLASS, PROPDEF_SAFE,
        PROPDEF_GAS_RELEASING};
    u32 i, m;

    // Arrange, Act, Assert: each check reads the table and the compiler
    for (i = 0; i < sizeof(plain) / sizeof(plain[0]); i++)
    {
        CHECK_WIDE(plain[i], ObjectRecord); /* 144 at 64 bits, 128 at 32 */
        CHECK_OBJECT(plain[i], ObjectRecord);
    }
    CHECK(sizeof(void *) == 4 || (offsetof(ObjectRecord, prop) == 16 && offsetof(ObjectRecord, model) == 24));
    CHECK_WIDE(PROPDEF_HAT, HatRecord);
    CHECK_WIDE(PROPDEF_DOOR, DoorRecord);
    CHECK_OBJECT(PROPDEF_DOOR, DoorRecord);
    CHECK_AT(PROPDEF_DOOR, 32, DoorRecord, linkedDoorOffset);
    CHECK_AT(PROPDEF_DOOR, 50, DoorRecord, linkedDoor);
    CHECK_AT(PROPDEF_DOOR, 51, DoorRecord, unkcc);
    CHECK_AT(PROPDEF_DOOR, 52, DoorRecord, bbox);
    CHECK_AT(PROPDEF_DOOR, 61, DoorRecord, openSoundState);
    CHECK_AT(PROPDEF_DOOR, 62, DoorRecord, closeSoundState);
    CHECK_AT(PROPDEF_DOOR, 63, DoorRecord, lastcalc60i);
    CHECK_WIDE(PROPDEF_KEY, KeyRecord);
    CHECK_OBJECT(PROPDEF_KEY, KeyRecord);
    CHECK_WIDE(PROPDEF_MAGAZINE, AmmoCrateRecord);
    CHECK_AT(PROPDEF_MAGAZINE, 32, AmmoCrateRecord, ammoType);
    CHECK_WIDE(PROPDEF_COLLECTABLE, WeaponObjRecord);
    CHECK_AT(PROPDEF_COLLECTABLE, 32, WeaponObjRecord, weaponnum);
    CHECK_AT(PROPDEF_COLLECTABLE, 33, WeaponObjRecord, dualweapon);
    CHECK_WIDE(PROPDEF_MONITOR, MonitorObjRecord);
    CHECK_AT(PROPDEF_MONITOR, 32, MonitorObjRecord, Monitor.cmdlist);
    CHECK_AT(PROPDEF_MONITOR, 34, MonitorObjRecord, Monitor.tconfig);
    CHECK_AT(PROPDEF_MONITOR, 61, MonitorObjRecord, OwnerOffset);
    CHECK_WIDE(PROPDEF_MULTI_MONITOR, MultiMonitorObjRecord);
    for (m = 0; m < 4; m++)
    {
        const size_t base = offsetof(MultiMonitorObjRecord, Monitor) + m * sizeof(MonitorRecord);

        CHECK(ruleOffset(portSetupWide(PROPDEF_MULTI_MONITOR), 32 + 29 * m) == base + offsetof(MonitorRecord, cmdlist));
        CHECK(ruleOffset(portSetupWide(PROPDEF_MULTI_MONITOR), 34 + 29 * m) == base + offsetof(MonitorRecord, tconfig));
        CHECK(ruleOffset(portSetupWide(PROPDEF_MULTI_MONITOR), 60 + 29 * m) == base + offsetof(MonitorRecord, colinc));
    }
    CHECK_AT(PROPDEF_MULTI_MONITOR, 148, MultiMonitorObjRecord, ImageNums);
    CHECK_WIDE(PROPDEF_AUTOGUN, AutogunRecord);
    CHECK_AT(PROPDEF_AUTOGUN, 49, AutogunRecord, unkC4);
    CHECK_AT(PROPDEF_AUTOGUN, 50, AutogunRecord, unkC8);
    CHECK_AT(PROPDEF_AUTOGUN, 51, AutogunRecord, beam);
    CHECK_AT(PROPDEF_AUTOGUN, 52, AutogunRecord, is_active);
    CHECK_WIDE(PROPDEF_AMMO, MultiAmmoCrateRecord);
    CHECK_WIDE(PROPDEF_ARMOUR, BodyArmourRecord);
    CHECK_WIDE(PROPDEF_VEHICHLE, VehichleRecord);
    CHECK_AT(PROPDEF_VEHICHLE, 32, VehichleRecord, ailist);
    CHECK_AT(PROPDEF_VEHICHLE, 33, VehichleRecord, aioffset);
    CHECK_AT(PROPDEF_VEHICHLE, 41, VehichleRecord, path);
    CHECK_AT(PROPDEF_VEHICHLE, 42, VehichleRecord, nextstep);
    CHECK_AT(PROPDEF_VEHICHLE, 43, VehichleRecord, Sound);
    CHECK_WIDE(PROPDEF_AIRCRAFT, AircraftRecord);
    CHECK_AT(PROPDEF_AIRCRAFT, 32, AircraftRecord, ailist);
    CHECK_AT(PROPDEF_AIRCRAFT, 42, AircraftRecord, nextstep);
    CHECK_AT(PROPDEF_AIRCRAFT, 43, AircraftRecord, path);
    CHECK_AT(PROPDEF_AIRCRAFT, 44, AircraftRecord, Sound);
    CHECK_WIDE(PROPDEF_TANK, TankRecord);
    CHECK_AT(PROPDEF_TANK, 33, TankRecord, rect);
    CHECK_AT(PROPDEF_TANK, 55, TankRecord, tank_orientation_angle);
    CHECK_WIDE(PROPDEF_TINTED_GLASS, TintedGlassRecord);
    CHECK_AT(PROPDEF_TINTED_GLASS, 36, TintedGlassRecord, unk90);
    /* CCTVRecord is four words longer than the file's 59-word record (unkEC
     * to unkF8, which no code names), so its sizeof is not the record's. */
    CHECK_RULE_ONLY(PROPDEF_CCTV);
    CHECK_OBJECT(PROPDEF_CCTV, CCTVRecord);
    CHECK_AT(PROPDEF_CCTV, 32, CCTVRecord, lookpad);
    CHECK_AT(PROPDEF_CCTV, 58, CCTVRecord, unkE8);
}

static void testSmallTypes(void)
{
    static const u32 header[] = {PROPDEF_NOTHING, PROPDEF_DEBRIS, PROPDEF_UNK16, PROPDEF_OBJECTIVE_END,
        PROPDEF_END};
    u32 i;

    // Arrange, Act, Assert: each check reads the table and the compiler
    for (i = 0; i < sizeof(header) / sizeof(header[0]); i++)
    {
        CHECK_WIDE(header[i], PropDefHeaderRecord);
    }
    CHECK_WIDE(PROPDEF_OBJECTIVE_NULL, NULLObjectRecord);
    CHECK_WIDE(PROPDEF_DOOR_SCALE, GlobalDoorScaleRecord);
    CHECK_WIDE(PROPDEF_GUARD, GuardRecord);
    CHECK_AT(PROPDEF_GUARD, 3, GuardRecord, Preset);
    CHECK_AT(PROPDEF_GUARD, 6, GuardRecord, Data);
    CHECK_WIDE(PROPDEF_GUARD_ATTRIBUTE, GuardAttributeRecord);
    CHECK_AT(PROPDEF_GUARD_ATTRIBUTE, 2, GuardAttributeRecord, unk8);
    CHECK_WIDE(PROPDEF_SWITCH, SwitchRecord);
    CHECK_AT(PROPDEF_SWITCH, 1, SwitchRecord, first);
    CHECK_AT(PROPDEF_SWITCH, 2, SwitchRecord, second);
    CHECK_AT(PROPDEF_SWITCH, 3, SwitchRecord, next);
    /* LINK is read through LinkRecord's two index unions; the file's record
     * has three words and no room for the struct's next (which only SWITCH
     * links), so LinkRecord's sizeof is not the record's. */
    CHECK_RULE_ONLY(PROPDEF_LINK);
    CHECK_AT(PROPDEF_LINK, 1, LinkRecord, Index1);
    CHECK_AT(PROPDEF_LINK, 2, LinkRecord, Index2);
    CHECK_WIDE(PROPDEF_TAG, TagObjectRecord);
    CHECK_AT(PROPDEF_TAG, 2, TagObjectRecord, NextTag);
    CHECK_AT(PROPDEF_TAG, 3, TagObjectRecord, TaggedObject);
    CHECK_WIDE(PROPDEF_OBJECTIVE_START, struct objective_entry);
    CHECK_AT(PROPDEF_OBJECTIVE_START, 2, struct objective_entry, reserved);
    CHECK_AT(PROPDEF_OBJECTIVE_START, 3, struct objective_entry, unkC);
    CHECK_WIDE(PROPDEF_OBJECTIVE_DESTROY_OBJECT, DestroyObjectRecord);
    CHECK_WIDE(PROPDEF_OBJECTIVE_COMPLETE_CONDITION, CompleteConditionRecord);
    CHECK_WIDE(PROPDEF_OBJECTIVE_FAIL_CONDITION, FailConditionRecord);
    CHECK_WIDE(PROPDEF_OBJECTIVE_COLLECT_OBJECT, CollectObjectRecord);
    CHECK_WIDE(PROPDEF_OBJECTIVE_DEPOSIT_OBJECT, DepositObjectRecord);
    CHECK_WIDE(PROPDEF_OBJECTIVE_PHOTOGRAPH, struct criteria_picture);
    CHECK_WIDE(PROPDEF_OBJECTIVE_PHOTOGRAPH, PhotographObjectRecord);
    CHECK_AT(PROPDEF_OBJECTIVE_PHOTOGRAPH, 3, struct criteria_picture, next);
    CHECK_WIDE(PROPDEF_OBJECTIVE_ENTER_ROOM, struct criteria_roomentered);
    CHECK_AT(PROPDEF_OBJECTIVE_ENTER_ROOM, 2, struct criteria_roomentered, status);
    CHECK_AT(PROPDEF_OBJECTIVE_ENTER_ROOM, 3, struct criteria_roomentered, next);
    CHECK_WIDE(PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM, struct criteria_deposit);
    CHECK_AT(PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM, 3, struct criteria_deposit, flag);
    CHECK_AT(PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM, 4, struct criteria_deposit, next);
    /* COPY_ITEM's CoopyObjectRecord has a u16 after the header; the file's
     * record is the header alone, and the game reads only its type. */
    CHECK_RULE_ONLY(PROPDEF_OBJECTIVE_COPY_ITEM);
    CHECK_WIDE(PROPDEF_WATCH_MENU_OBJECTIVE_TEXT, struct setup_objective_text);
    CHECK_WIDE(PROPDEF_WATCH_MENU_OBJECTIVE_TEXT, struct watchMenuObjectiveText);
    CHECK_AT(PROPDEF_WATCH_MENU_OBJECTIVE_TEXT, 3, struct setup_objective_text, next);
    CHECK_AT(PROPDEF_WATCH_MENU_OBJECTIVE_TEXT, 3, struct watchMenuObjectiveText, nextentry);
    CHECK_WIDE(PROPDEF_RENAME, RenameObjectRecord);
    CHECK_WIDE(PROPDEF_RENAME, textoverride);
    CHECK_AT(PROPDEF_RENAME, 8, textoverride, next);
    CHECK_AT(PROPDEF_RENAME, 9, textoverride, obj);
    CHECK_AT(PROPDEF_RENAME, 9, RenameObjectRecord, renobj);
    CHECK_WIDE(PROPDEF_LOCK_DOOR, LockDoorRecord);
    CHECK_AT(PROPDEF_LOCK_DOOR, 3, LockDoorRecord, next);
    CHECK_WIDE(PROPDEF_SAFE_ITEM, SafeObjectRecord);
    CHECK_AT(PROPDEF_SAFE_ITEM, 3, SafeObjectRecord, Index3);
    CHECK_AT(PROPDEF_SAFE_ITEM, 4, SafeObjectRecord, next);
    CHECK_WIDE(PROPDEF_CAMERAPOS, CutsceneRecord);
    /* UNK41 is one word in the file; the game reads it as an ObjectRecord
     * (prop.c), which no stock setup holds. No C record is its record. */
    CHECK_RULE_ONLY(PROPDEF_UNK41);
}

static void testEveryEntry(void)
{
    u32 type, p;

    // Arrange, Act, Assert: the 49 entries against the kinds table
    for (type = 0; type < SETUP_PROPDEF_TYPES; type++)
    {
        const struct PortSetupWide *w = portSetupWide(type);

        CHECK(w != NULL && w->filewords == kSetupPropdefKinds[type].words);
        CHECK(sizeof(void *) == 8 ? w->widewords % 2 == 0 : w->widewords == w->filewords);
        CHECK(w->npointers <= sizeof(w->pointerword));
        for (p = 0; p < w->npointers; p++)
        {
            CHECK(w->pointerword[p] > 0 && w->pointerword[p] < w->filewords);
            CHECK(p == 0 || w->pointerword[p] > w->pointerword[p - 1]);
            CHECK(kSetupPropdefKinds[type].kind[w->pointerword[p]] == SETUP_KIND_K32);
        }
    }
    CHECK(portSetupWide(SETUP_PROPDEF_TYPES) == NULL);
}

/* ==== a prop list built here ========================================= */

#define LIST_RECORDS 8
#define LIST_CAP 0x800u
#define DOOR_DAMAGE 0x12345678u   /* 16.16; the file's halves swapped by K16, swapped back by prop.c's fix-up */
#define DOOR_LINKED_OFFSET 3u

static const u32 kListTypes[LIST_RECORDS] = {PROPDEF_GUARD, PROPDEF_DOOR, PROPDEF_COLLECTABLE, PROPDEF_LINK,
    PROPDEF_TAG, PROPDEF_SAFE_ITEM, PROPDEF_RENAME, PROPDEF_END};
static u8 g_List[LIST_CAP];
static u32 g_ListSize;
static u32 g_ListAt[LIST_RECORDS];
static u8 g_Out[2][LIST_CAP] __attribute__((aligned(16)));

static int isPointerWord(const struct PortSetupWide *w, u32 word)
{
    u32 p;

    for (p = 0; p < w->npointers && w->pointerword[p] != word; p++)
    {
    }
    return p < w->npointers;
}

/* Word WORD of record R as the file holds it: a distinct value that every
 * swap changes; zero in a pointer word but the index unions'. */
static u32 listWord(u32 r, u32 word)
{
    const u32 type = kListTypes[r];

    if (word == 0)
    {
        return 0x01000000u | (r << 8) | type; /* scale 0x0100, state R, the type */
    }
    if (type == PROPDEF_DOOR && word == 29)
    {
        return DOOR_DAMAGE;
    }
    if (type == PROPDEF_DOOR && word == 32)
    {
        return DOOR_LINKED_OFFSET;
    }
    if ((type == PROPDEF_LINK && word <= 2) || (type == PROPDEF_SAFE_ITEM && word <= 3))
    {
        return (u32) -(s32) (word + 1); /* the indexes, relative and negative */
    }
    if (isPointerWord(portSetupWide(type), word))
    {
        return 0;
    }
    return 0x10000000u * (r + 1) + word * 0x00030507u + 0x00010200u;
}

static void buildList(void)
{
    u32 r, word;

    g_ListSize = 0;
    for (r = 0; r < LIST_RECORDS; r++)
    {
        g_ListAt[r] = g_ListSize;
        for (word = 0; word < portSetupWide(kListTypes[r])->filewords; word++)
        {
            const u32 v = listWord(r, word);

            g_List[g_ListSize++] = (u8) (v >> 24);
            g_List[g_ListSize++] = (u8) (v >> 16);
            g_List[g_ListSize++] = (u8) (v >> 8);
            g_List[g_ListSize++] = (u8) v;
        }
    }
}

/* The in-place swap of a word of KIND (romfile_setup.c), as the test states it. */
static u32 swapped(const u8 *b, int kind)
{
    switch (kind)
    {
    case SETUP_KIND_K32: return (u32) b[3] | (u32) b[2] << 8 | (u32) b[1] << 16 | (u32) b[0] << 24;
    case SETUP_KIND_K16: return (u32) b[1] | (u32) b[0] << 8 | (u32) b[3] << 16 | (u32) b[2] << 24;
    case SETUP_KIND_KSK: return (u32) b[1] | (u32) b[0] << 8 | (u32) b[2] << 16 | (u32) b[3] << 24;
    case SETUP_KIND_KKS: return (u32) b[0] | (u32) b[1] << 8 | (u32) b[3] << 16 | (u32) b[2] << 24;
    default: return (u32) b[0] | (u32) b[1] << 8 | (u32) b[2] << 16 | (u32) b[3] << 24;
    }
}

/* Every byte of the record of R at OUT: each word swapped at the rule's
 * place, a pointer word's upper half and every pad byte zero. */
static int recordIsExact(u32 r, const u8 *out)
{
    const struct PortSetupWide *w = portSetupWide(kListTypes[r]);
    u8 expected[LIST_CAP] = {0};
    u32 word;

    for (word = 0; word < w->filewords; word++)
    {
        const int kind = word == 0 ? SETUP_KIND_KSK : kSetupPropdefKinds[kListTypes[r]].kind[word];
        const u32 v = swapped(g_List + g_ListAt[r] + word * 4, kind);

        __builtin_memcpy(expected + ruleOffset(w, word), &v, 4);
    }
    return __builtin_memcmp(expected, out, (size_t) w->widewords * 4) == 0;
}

static void testPropList(void)
{
    const u8 *rec[LIST_RECORDS];
    u32 total, r, at = 0, i;
    s32 converted[2];
    u32 damage;

    // Arrange
    buildList();
    __builtin_memset(g_Out, 0xA5, sizeof(g_Out));

    // Act
    total = portSetupPropsSize(g_List, g_ListSize);
    converted[0] = portSetupPropsConvert(g_List, g_ListSize, g_Out[0], LIST_CAP);
    converted[1] = portSetupPropsConvert(g_List, g_ListSize, g_Out[1], LIST_CAP);

    // Assert: the walk by WIDEWORDS visits the eight types in order
    CHECK(total != 0 && converted[0] == 1 && converted[1] == 1);
    CHECK(total == (sizeof(void *) == 8 ? 632u : g_ListSize));
    for (r = 0; r < LIST_RECORDS && at < total; r++)
    {
        rec[r] = g_Out[0] + at;
        CHECK(((const PropDefHeaderRecord *) rec[r])->type == kListTypes[r]);
        CHECK(recordIsExact(r, rec[r]));
        at += portSetupWide(((const PropDefHeaderRecord *) rec[r])->type)->widewords * 4u;
    }
    CHECK(r == LIST_RECORDS && at == total);
    // Assert: the measured bytes are the bytes written, and two calls agree
    for (i = total; i < LIST_CAP && g_Out[0][i] == 0xA5; i++)
    {
    }
    CHECK(i == LIST_CAP);
    CHECK(__builtin_memcmp(g_Out[0], g_Out[1], LIST_CAP) == 0);
    if (r != LIST_RECORDS)
    {
        return;
    }
    // Assert: the members the game reads
    CHECK(((const ObjectRecord *) rec[1])->prop == NULL && ((const ObjectRecord *) rec[1])->model == NULL);
    __builtin_memcpy(&damage, &((const ObjectRecord *) rec[1])->damage, 4);
    CHECK(((damage >> 16) | (damage << 16)) == DOOR_DAMAGE);
    CHECK(((const DoorRecord *) rec[1])->linkedDoorOffset == (s32) DOOR_LINKED_OFFSET);
    CHECK(((const DoorRecord *) rec[1])->linkedDoor == NULL);
    CHECK(((const WeaponObjRecord *) rec[2])->dualweapon == NULL);
    CHECK(((const LinkRecord *) rec[3])->Index1 == -2 && ((const LinkRecord *) rec[3])->Index2 == -3);
    CHECK((uintptr_t) ((const LinkRecord *) rec[3])->first == 0xFFFFFFFEu); /* zero-extended at 64 bits */
    CHECK(((const TagObjectRecord *) rec[4])->NextTag == NULL && ((const TagObjectRecord *) rec[4])->TaggedObject == NULL);
    CHECK(((const SafeObjectRecord *) rec[5])->Index1 == -2 && ((const SafeObjectRecord *) rec[5])->Index3 == -4);
    CHECK(((const SafeObjectRecord *) rec[5])->next == NULL);
    CHECK(((const textoverride *) rec[6])->next == NULL && ((const textoverride *) rec[6])->obj == NULL);
    CHECK(((const RenameObjectRecord *) rec[6])->TagID == (s32) listWord(6, 1));
}

static void testListRefusals(void)
{
    u32 total;
    u32 i;

    // Arrange
    buildList();
    total = portSetupPropsSize(g_List, g_ListSize);
    __builtin_memset(g_Out, 0xA5, sizeof(g_Out));

    // Act, Assert: an OUTSIZE one byte short writes nothing
    CHECK(portSetupPropsConvert(g_List, g_ListSize, g_Out[0], total - 1) == 0);
    // Act, Assert: bytes after the end record, a list cut short, no list
    CHECK(portSetupPropsSize(g_List, g_ListSize + 4) == 0);
    CHECK(portSetupPropsConvert(g_List, g_ListSize + 4, g_Out[0], LIST_CAP) == 0);
    CHECK(portSetupPropsSize(g_List, g_ListSize - 4) == 0);
    CHECK(portSetupPropsConvert(g_List, g_ListSize - 4, g_Out[0], LIST_CAP) == 0);
    CHECK(portSetupPropsSize(NULL, 4) == 0 && portSetupPropsConvert(NULL, 4, g_Out[0], LIST_CAP) == 0);
    CHECK(portSetupPropsConvert(g_List, g_ListSize, NULL, LIST_CAP) == 0);
    // Act, Assert: a record of an unknown type
    g_List[g_ListAt[2] + 3] = SETUP_PROPDEF_TYPES;
    CHECK(portSetupPropsSize(g_List, g_ListSize) == 0);
    CHECK(portSetupPropsConvert(g_List, g_ListSize, g_Out[0], LIST_CAP) == 0);
    for (i = 0; i < LIST_CAP && g_Out[0][i] == 0xA5; i++)
    {
    }
    CHECK(i == LIST_CAP);
}

int main(void)
{
    testObjectTypes();
    testSmallTypes();
    testEveryEntry();
    testPropList();
    testListRefusals();
    if (s_Failures != 0)
    {
        printf("test_romconv_setup_props: %d failed\n", s_Failures);
        return 1;
    }
    printf("test_romconv_setup_props: OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
