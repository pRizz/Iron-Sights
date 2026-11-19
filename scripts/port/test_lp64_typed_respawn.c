/* ROM-free check that the multiplayer respawn walk steps a watch or credits
 * intro record by its own size at 64 bits (LP64-005 strict batch 1, entry 16,
 * test_lp64_typed_reads.sh). bondview2.c is included whole, with stubs for
 * what mp_respawn_handler calls; --gc-sections drops the rest. Each intro is
 * built here from invented numbers: a watch or credits record whose words
 * after the type would read as other record types, then an item record the
 * walk must reach, then the end. Stepping the first record by 4 bytes, as the
 * N64 code does, reads those words as types and misses the item. At 32 bits
 * the walk is left as inherited (the owner section of FORK-089's entry 16),
 * and the test expects the item to be missed there. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/game/bondview2.c"

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_respawn: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

/* An intro record's type word, as INTRO_TYPE reads it on PC. */
#define TYPE_WORD(type) ((s32) ((u32) (type) << 24))
#define ITEM_GIVEN 5

struct player *g_CurrentPlayer;
struct player_data *g_playerPerm;
stagesetup g_CurrentSetup;
PadRecord *g_Startpad[16];
s32 startpadcount;
coord3d ZeroCoordSpawnPos;
struct PropRecord *g_PlayerTankProp;
f32 g_PlayerTankYOffset;
s32 g_UpperTextDisplayFlag;

static s32 itemsAdded;
static s32 lastItemAdded;

void init_player_BONDdata(void) {}
void bondviewPlayerBeginLife(void) {}
s32 bondviewGetRandomSpawnPadIndex(void) { return 0; }
f32 get_player_mp_char_height(s32 player) { return 0.0f; }
s32 getPlayerCount(void) { return 2; }
s32 get_cur_playernum(void) { return 0; }
u32 randomGetNext(void) { return 0; }
s32 getTileRoom(StandTile *tile) { return 0; }
f32 stanGetPositionYValue(StandTile *tile, f32 p_x, f32 p_z) { return 0.0f; }
void bondinvReinitInv(void) {}
s32 check_ramrom_flags(void) { return 0; }
s32 get_is_ramrom_flag(void) { return 0; }
s32 portMpSpawnWeapon(void) { return 0; }
/* Only Spawn with Weapon reaches these; the test leaves it off. */
struct s_mp_weapon_set *getPtrMPWeaponSetData(void) { abort(); }
s32 get_max_ammo_for_type(s32 type) { abort(); }
WeaponStats *get_ptr_item_statistics(ITEM_IDS item) { abort(); }
void give_cur_player_ammo(s32 type, s32 quantity) {}
s32 currentPlayerEquipWeaponWrapper(GUNHAND hand, s32 next_weapon) { return 0; }

int bondinvAddInvItem(ITEM_IDS item)
{
    itemsAdded++;
    lastItemAdded = item;
    return 1;
}

int bondinvAddDoublesInvItem(ITEM_IDS right, ITEM_IDS left)
{
    itemsAdded++;
    return 1;
}

/* Room for the intro, aligned for the 64-bit credits record. */
static union
{
    u8 bytes[128];
    uintptr_t align;
} intro;

static void respawnWithIntro(size_t firstRecordBytes)
{
    static struct player player;
    static PropRecord prop;
    static PadRecord pad;
    static struct player_data perm;
    struct SetupIntroItem item = {TYPE_WORD(INTROTYPE_ITEM), ITEM_GIVEN, -1, 0};
    s32 end = TYPE_WORD(INTROTYPE_END);
    memcpy(intro.bytes + firstRecordBytes, &item, sizeof(item));
    memcpy(intro.bytes + firstRecordBytes + sizeof(item), &end, sizeof(end));
    memset(&player, 0, sizeof(player));
    player.prop = &prop;
    g_CurrentPlayer = &player;
    g_playerPerm = &perm;
    g_Startpad[0] = &pad;
    g_CurrentSetup.intro = (void *) intro.bytes;
    itemsAdded = 0;
    lastItemAdded = 0;
    mp_respawn_handler();
}

/* A watch record's hours and minutes are not record types. */
static void testWalkStepsAWatchRecordWhole(void)
{
    // Arrange: hours 0 reads as SPAWN (12 bytes), which lands on the item's
    // first item word, 5 >> 24 = 0, SPAWN again, then on the end.
    struct SetupIntroWatch watch = {TYPE_WORD(INTROTYPE_WATCH), 0, 0};
    memset(intro.bytes, 0, sizeof(intro.bytes));
    memcpy(intro.bytes, &watch, sizeof(watch));

    // Act
    respawnWithIntro(sizeof(watch));

    // Assert
    if (IS_64_BIT)
    {
        expect(itemsAdded == 1 && lastItemAdded == ITEM_GIVEN, "the walk reaches the item after a watch record");
    }
    else
    {
        /* pins the inherited wrong 32-bit walk: FORK_CHANGES.md, "Inherited at 32 bits, for the owner", entry 16 */
        expect(itemsAdded == 0, "the 32-bit walk keeps the inherited 4-byte step past a watch record");
    }
}

/* A credits record's words after its type are not record types. */
static void testWalkStepsACreditsRecordWhole(void)
{
    // Arrange: the word after the type reads as END.
    struct SetupIntroCredits credits;
    memset(&credits, 0, sizeof(credits));
    credits.type = TYPE_WORD(INTROTYPE_CREDITS);
    memset(intro.bytes, 0, sizeof(intro.bytes));
    memcpy(intro.bytes, &credits, sizeof(credits));
    {
        s32 end = TYPE_WORD(INTROTYPE_END);
        memcpy(intro.bytes + sizeof(s32), &end, sizeof(end));
    }

    // Act
    respawnWithIntro(sizeof(credits));

    // Assert
    if (IS_64_BIT)
    {
        expect(itemsAdded == 1 && lastItemAdded == ITEM_GIVEN, "the walk reaches the item after a credits record");
    }
    else
    {
        /* pins the inherited wrong 32-bit walk: FORK_CHANGES.md, "Inherited at 32 bits, for the owner", entry 16 */
        expect(itemsAdded == 0, "the 32-bit walk keeps the inherited 4-byte step past a credits record");
    }
}

int main(void)
{
    testWalkStepsAWatchRecordWhole();
    testWalkStepsACreditsRecordWhole();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_respawn: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
