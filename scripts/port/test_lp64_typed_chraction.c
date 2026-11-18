/* ROM-free checks that chraction.c reads records through their own types at
 * each pointer width (LP64-005 Task 4, test_lp64_typed_reads.sh). The game
 * file is included whole, as test_stan_callbacks.c does, with stubs for the
 * functions the tested ones call; --gc-sections drops everything else. Each
 * check stores a value in the member a site means and none elsewhere, so a
 * read by position at 64 bits sees zero instead. */
#include <stdio.h>
#include <stdlib.h>
#include "../../src/game/chraction.c"

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_chraction: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

/* Room for any record read here, so a read by position stays inside it. */
typedef union
{
    ChrRecord chr;
    WeaponObjRecord weapon;
    u8 bytes[2 * sizeof(ChrRecord)];
} RecordRoom;

#define ONE_HANDED_ITEM ITEM_WPPK

u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 mask)
{
    return item == ONE_HANDED_ITEM && mask == WEAPONSTATBITFLAG_ONLY_1_HANDED;
}

/* The functions the animation choosers call, recording what they are given. */
struct ChrModelFileRecord c_item_entries[1];
static u8 animTable[sizeof(struct animation_table_data) + 64];
struct animation_table_data *ptr_animation_table = (struct animation_table_data *) animTable;
static ModelAnimation *lastAnimation;

PropRecord *chrGetEquippedWeaponProp(ChrRecord *self, GUNHAND hand)
{
    return self->weapons_held[hand];
}

u32 randomGetNext(void)
{
    return 0;
}

void modelSetAnimation(Model *model, ModelAnimation *animation, s32 flip, f32 startframe, f32 speed, f32 merge)
{
    lastAnimation = animation;
}

/* weaponIsOneHanded reads the weapon prop's WeaponObjRecord.weaponnum. */
static void testOneHandedReadsTheWeaponNumber(void)
{
    // Arrange
    static RecordRoom room;
    PropRecord prop = {0};
    memset(&room, 0, sizeof(room));
    room.weapon.weaponnum = ONE_HANDED_ITEM;
    prop.type = PROP_TYPE_WEAPON;
    prop.weapon = &room.weapon;

    // Act
    u32 onehanded = weaponIsOneHanded(&prop);

    // Assert
    expect(onehanded != 0, "weaponIsOneHanded reads WeaponObjRecord.weaponnum");
}

/* A guard sent along a course runs at the speed act_gopos.unk59 holds. */
static void testCourseSpeedIsTheGoposSpeed(void)
{
    // Arrange: an unarmed male guard told to sprint (speed 2).
    static RecordRoom room;
    memset(&room, 0, sizeof(room));
    room.chr.actiontype = ACT_GOPOS;
    room.chr.act_gopos.unk59 = SPEED_SPRINT;
    c_item_entries[0].isMale = 1;
    lastAnimation = NULL;

    // Act
    play_hit_soundeffect_and_proper_volume(&room.chr);

    // Assert
    expect(lastAnimation == ANIM_FROM_TABLE(PTR_ANIM_sprinting_one_handed_weapon),
           "play_hit_soundeffect_and_proper_volume reads act_gopos.unk59");
}

int main(void)
{
    testOneHandedReadsTheWeaponNumber();
    testCourseSpeedIsTheGoposSpeed();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_chraction: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
