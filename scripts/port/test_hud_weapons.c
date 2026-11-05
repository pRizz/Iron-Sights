/* ROM-free weapon strip card checks: one card per cycle stop, including pairs. */
#include <assert.h>
#include <stdarg.h>
#include "../../port/src/port_hud_weapons.c"
#include "../../port/src/port_hud_ammo.c"
/* The decomp's minimal string.h omits these CRT declarations. */
extern int strcmp(const char *a, const char *b);
extern void *memset(void *dst, int value, size_t size);
extern int vfprintf(FILE *file, const char *fmt, va_list args);
extern void abort(void);

void osSyncPrintf(const char *fmt, ...)
{
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args); abort();
}

static struct player testPlayer;
struct player *g_CurrentPlayer = &testPlayer;
static s32 inventory[40], inventoryCount, rightHand, leftHand, allGunsDoubles;
static InvItem items[8];
static s32 itemCount;
s32 bondinvCountTotalItemsInInv(void) { return inventoryCount; }
s32 bondinvGetTextbyInvIndex(s32 i) { return inventory[i]; }
ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand) { return hand == GUNRIGHT ? rightHand : leftHand; }
s32 bondinvItemAvailableForHand(ITEM_IDS right, ITEM_IDS left)
{
    /* Stand-in for the dual-wield flag: pistols through launchers. */
    return allGunsDoubles && right == left && right >= ITEM_WPPK && right <= ITEM_ROCKETLAUNCH;
}
static s32 sniperOwned;
s32 bondinvItemAvailable(ITEM_IDS item) { return item == ITEM_SNIPERRIFLE && sniperOwned; }
/* A mission override renames the RC-P90's single card only. */
u16 *bondinvGetNameByIndex(s32 i) { return (u16 *)(inventory[i] == ITEM_FNP90 ? "RC-P90\n" : ""); }
u16 *get_ptr_short_watch_text_for_item(ITEM_IDS item)
{
    switch (item) {
    case ITEM_WPPK: return (u16 *)"PP7\n";
    case ITEM_M16: return (u16 *)"AR33\n";
    case ITEM_FNP90: return (u16 *)"P90 watch\n";
    case ITEM_KNIFE: return (u16 *)"Hunting Knife\n";
    default: return (u16 *)"";
    }
}

static void resetFixture(void)
{
    memset(items, 0, sizeof(items));
    itemCount = inventoryCount = allGunsDoubles = 0;
    rightHand = leftHand = ITEM_UNARMED;
    testPlayer.ptr_inventory_first_in_cycle = NULL;
}

/* Native cycle lists are circular; order here is irrelevant to the cards. */
static void addPair(s32 right, s32 left)
{
    InvItem *item = &items[itemCount++];
    item->type = INV_ITEM_DUAL;
    item->type_inv_item.type_dual.weapon_right = right;
    item->type_inv_item.type_dual.weapon_left = left;
    if (!testPlayer.ptr_inventory_first_in_cycle) {
        item->next = item->prev = item;
        testPlayer.ptr_inventory_first_in_cycle = item;
    } else {
        InvItem *first = testPlayer.ptr_inventory_first_in_cycle;
        item->next = first; item->prev = first->prev;
        first->prev->next = item; first->prev = item;
    }
}

static void testPickedUpPairs(void)
{
    HudWeapon cards[HUD_WEAPONS];
    s32 n, selected;
    /* Arrange: Jungle-style mismatched pair plus doubles, both after their right gun. */
    resetFixture();
    inventory[0] = ITEM_UNARMED; inventory[1] = ITEM_WPPK; inventory[2] = ITEM_M16; inventory[3] = ITEM_FNP90;
    inventoryCount = 4;
    addPair(ITEM_FNP90, ITEM_M16);
    addPair(ITEM_WPPK, ITEM_WPPK);
    addPair(ITEM_FNP90, ITEM_FNP90);
    rightHand = ITEM_FNP90; leftHand = ITEM_M16;

    /* Act. */
    n = portHudWeapons(cards, HUD_WEAPONS, &selected);

    /* Assert: the cycle order the native sort gives (right gun, then left gun). */
    assert(n == 7);
    assert(cards[1].item == ITEM_WPPK && cards[1].left == ITEM_UNARMED);
    assert(cards[2].item == ITEM_WPPK && cards[2].left == ITEM_WPPK);
    assert(cards[3].item == ITEM_M16 && cards[3].left == ITEM_UNARMED);
    assert(cards[4].item == ITEM_FNP90 && cards[4].left == ITEM_UNARMED);
    assert(cards[5].item == ITEM_FNP90 && cards[5].left == ITEM_M16 && cards[5].inventory == 3);
    assert(cards[6].item == ITEM_FNP90 && cards[6].left == ITEM_FNP90);
    assert(selected == 5);
    leftHand = ITEM_UNARMED;
    portHudWeapons(cards, HUD_WEAPONS, &selected);
    assert(selected == 4);
    rightHand = leftHand = ITEM_WPPK;
    portHudWeapons(cards, HUD_WEAPONS, &selected);
    assert(selected == 2);
}

static void testAllGunsDoubles(void)
{
    HudWeapon cards[HUD_WEAPONS];
    s32 n, selected;
    /* Arrange: the guns cheat offers doubles of dual-capable guns, not the knife. */
    resetFixture();
    inventory[0] = ITEM_UNARMED; inventory[1] = ITEM_KNIFE; inventory[2] = ITEM_WPPK; inventory[3] = ITEM_REMOTEMINE;
    inventoryCount = 4; allGunsDoubles = 1;
    addPair(ITEM_WPPK, ITEM_WPPK); /* a picked-up double is the same cycle stop */
    rightHand = leftHand = ITEM_WPPK;

    /* Act. */
    n = portHudWeapons(cards, HUD_WEAPONS, &selected);

    /* Assert: no unarmed or knife pair, one PP7 double, gadgets stay single. */
    assert(n == 5);
    assert(cards[0].item == ITEM_UNARMED && cards[0].left == ITEM_UNARMED);
    assert(cards[1].item == ITEM_KNIFE && cards[1].left == ITEM_UNARMED);
    assert(cards[2].item == ITEM_WPPK && cards[2].left == ITEM_UNARMED);
    assert(cards[3].item == ITEM_WPPK && cards[3].left == ITEM_WPPK && selected == 3);
    assert(cards[4].item == ITEM_REMOTEMINE && cards[4].left == ITEM_UNARMED);
    /* The detonator keeps the Remote Mine card selected, whatever the left hand holds. */
    rightHand = ITEM_TRIGGER; leftHand = ITEM_REMOTEMINE;
    portHudWeapons(cards, HUD_WEAPONS, &selected);
    assert(selected == 4);
}

static void testOrphanPairAndCapacity(void)
{
    HudWeapon cards[HUD_WEAPONS];
    s32 n, selected;
    /* Arrange: a mission removed the single AR33, but its pair stays in the cycle. */
    resetFixture();
    inventory[0] = ITEM_WPPK; inventoryCount = 1;
    addPair(ITEM_M16, ITEM_WPPK);
    addPair(ITEM_TRIGGER, ITEM_UNARMED); /* never a strip card */
    rightHand = ITEM_M16; leftHand = ITEM_WPPK;

    /* Act / assert. */
    n = portHudWeapons(cards, HUD_WEAPONS, &selected);
    assert(n == 2 && cards[1].item == ITEM_M16 && cards[1].left == ITEM_WPPK && cards[1].inventory == -1);
    assert(selected == 1);
    n = portHudWeapons(cards, 1, &selected);
    assert(n == 1 && selected == -1);
}

static void testNames(void)
{
    HudWeapon cards[HUD_WEAPONS];
    char name[64], tiny[6];
    s32 n, selected;
    const HudWeapon unknown = { ITEM_TASER, ITEM_UNARMED, -1 };
    /* Arrange. */
    resetFixture();
    inventory[0] = ITEM_WPPK; inventory[1] = ITEM_M16; inventory[2] = ITEM_FNP90; inventoryCount = 3;
    addPair(ITEM_FNP90, ITEM_M16);
    addPair(ITEM_WPPK, ITEM_WPPK);
    n = portHudWeapons(cards, HUD_WEAPONS, &selected);
    assert(n == 5);

    /* Act / assert: singles, doubles, and mismatched pairs left gun first. */
    portHudWeaponName(cards, n, &cards[0], name, sizeof(name));
    assert(strcmp(name, "PP7") == 0);
    portHudWeaponName(cards, n, &cards[1], name, sizeof(name));
    assert(strcmp(name, "PP7 x2") == 0);
    portHudWeaponName(cards, n, &cards[4], name, sizeof(name));
    assert(strcmp(name, "AR33 + RC-P90") == 0); /* the RC-P90 keeps its mission name */
    portHudWeaponItemName(cards, n, ITEM_FNP90, name, sizeof(name));
    assert(strcmp(name, "RC-P90") == 0);
    portHudWeaponItemName(cards, 0, ITEM_FNP90, name, sizeof(name));
    assert(strcmp(name, "P90 watch") == 0);
    portHudWeaponName(cards, n, &unknown, name, sizeof(name));
    assert(strcmp(name, "Unarmed") == 0);
    portHudWeaponName(cards, n, &cards[4], tiny, sizeof(tiny));
    assert(strcmp(tiny, "AR33 ") == 0);
}

static sImageTableEntry ammoImages[14];
struct sImageTableEntry *ammo9mmimage = &ammoImages[0], *rifleammoimage = &ammoImages[1],
    *shotgunammoimage = &ammoImages[2], *knifeammoimage = &ammoImages[3], *glaunchammoimage = &ammoImages[4],
    *rocketammoimage = &ammoImages[5], *genericmineammoimage = &ammoImages[6], *grenadeammoimage = &ammoImages[7],
    *magnumammoimage = &ammoImages[8], *goldengunammoimage = &ammoImages[9], *remotemineammoimage = &ammoImages[10],
    *timedmineammoimage = &ammoImages[11], *proxmineammoimage = &ammoImages[12], *tankammoimage = &ammoImages[13];

static void testAmmoIconTags(void)
{
    Gfx list[2], *end;
    sImageTableEntry other;
    s32 i;
    /* Slots follow portAmmoIconPtr's order, which the embedded art shares. */
    for (i = 0; i < 14; i++) assert(portHudAmmoIconSlot(&ammoImages[i]) == i);
    assert(portHudAmmoIconSlot(&other) == -1 && portHudAmmoIconSlot(NULL) == -1);
    /* The tag carries slot + 1; NULL (or an unknown image) clears it. */
    end = portHudAmmoIconTag(list, tankammoimage);
    end = portHudAmmoIconTag(end, NULL);
    assert(end == list + 2);
    assert(list[0].words.w1 == 0x5B00000Eu && list[1].words.w1 == 0x5B000000u);
}

static void testRifleButt(void)
{
    char name[64];
    const HudWeapon unarmed = { ITEM_UNARMED, ITEM_UNARMED, 0 };
    /* Arrange: Bond holds a gun, so ownership predicts the next unarmed switch. */
    resetFixture();
    rightHand = ITEM_WPPK; sniperOwned = 1;

    /* Act / assert. */
    assert(portHudRifleButt());
    portHudWeaponName(&unarmed, 1, &unarmed, name, sizeof(name));
    assert(strcmp(name, "Buttstroke") == 0);
    sniperOwned = 0;
    assert(!portHudRifleButt());
    /* In hand, the club recorded at the last swap is what Bond holds. */
    rightHand = ITEM_FIST; sniperOwned = 1; testPlayer.cur_item_weapon_getname = ITEM_FIST;
    assert(!portHudRifleButt());
    rightHand = ITEM_UNARMED; sniperOwned = 0; testPlayer.cur_item_weapon_getname = ITEM_SNIPERRIFLE;
    assert(portHudRifleButt());
    testPlayer.cur_item_weapon_getname = ITEM_FIST;
}

int main(void)
{
    testPickedUpPairs();
    testAllGunsDoubles();
    testOrphanPairAndCapacity();
    testNames();
    testRifleButt();
    testAmmoIconTags();
    puts("HUD weapon card checks passed: picked-up pairs, All Guns doubles, selection, orphan pairs, capacity, names, rifle butt, ammo icon tags.");
    return 0;
}
