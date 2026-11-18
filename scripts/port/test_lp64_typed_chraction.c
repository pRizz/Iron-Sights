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

int main(void)
{
    testOneHandedReadsTheWeaponNumber();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_chraction: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
