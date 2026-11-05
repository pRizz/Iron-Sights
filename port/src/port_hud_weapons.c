/* Weapon strip cards, one per weapon-cycle stop. The native inventory index
 * (the watch list) omits two-gun pairs, so the strip also walks the cycle list
 * for picked-up pairs (INV_ITEM_DUAL) and, with All Guns, the doubles that
 * bondinvCycleForward offers after each dual-capable gun. */
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "game/bondview.h"
#include "game/bondinv.h"
#include "game/gun.h"
#include "game/player.h"
#include "port_hud_weapons.h"

extern s32 bondinvGetTextbyInvIndex(s32 index);
extern u16 *bondinvGetNameByIndex(s32 index);

static s32 phwStripItem(s32 item)
{
    /* The tank's cannon joins the inventory while Bond drives it. */
    return (item >= ITEM_UNARMED && item <= ITEM_TASER && item != ITEM_TRIGGER) || item == ITEM_TANKSHELLS;
}

static s32 phwFind(const HudWeapon *cards, s32 count, s32 item, s32 left)
{
    s32 i;
    for (i = 0; i < count; i++) if (cards[i].item == item && cards[i].left == left) return i;
    return -1;
}

static void phwAdd(HudWeapon *cards, s32 *count, s32 max, s32 item, s32 left, s32 inventory)
{
    if (*count >= max || phwFind(cards, *count, item, left) >= 0) return;
    cards[*count].item = item; cards[*count].left = left; cards[*count].inventory = inventory;
    (*count)++;
}

/* Pairs whose right gun is item, in the native sort order (by left gun). */
static void phwAddPairs(HudWeapon *cards, s32 *count, s32 max, s32 item, s32 inventory)
{
    InvItem *const first = g_CurrentPlayer->ptr_inventory_first_in_cycle;
    InvItem *inv = first;
    s32 lefts[ITEM_IDS_MAX], n = 0, i, j;
    while (inv && n < ITEM_IDS_MAX) {
        if (inv->type == INV_ITEM_DUAL && inv->type_inv_item.type_dual.weapon_right == item)
            lefts[n++] = inv->type_inv_item.type_dual.weapon_left;
        inv = inv->next;
        if (inv == first) break;
    }
    /* The native hand rule covers All Guns doubles, solo play and the knife. */
    if (item != ITEM_UNARMED && item < ITEM_BOMBCASE && n < ITEM_IDS_MAX
        && bondinvItemAvailableForHand(item, item)) lefts[n++] = item;
    for (i = 1; i < n; i++) {
        const s32 left = lefts[i];
        for (j = i; j > 0 && lefts[j-1] > left; j--) lefts[j] = lefts[j-1];
        lefts[j] = left;
    }
    for (i = 0; i < n; i++) phwAdd(cards, count, max, item, lefts[i], inventory);
}

s32 portHudWeapons(HudWeapon *cards, s32 max, s32 *selected)
{
    const s32 total = bondinvCountTotalItemsInInv();
    /* The watch detonator belongs to the Remote Mine card, which stays selected. */
    const s32 held = getCurrentPlayerWeaponId(GUNRIGHT);
    const s32 current = held == ITEM_TRIGGER ? ITEM_REMOTEMINE : held;
    InvItem *const first = g_CurrentPlayer->ptr_inventory_first_in_cycle;
    InvItem *inv = first;
    s32 i, n = 0;
    for (i = 0; i < total && n < max; i++) {
        const s32 item = bondinvGetTextbyInvIndex(i);
        if (!phwStripItem(item) || phwFind(cards, n, item, ITEM_UNARMED) >= 0) continue;
        phwAdd(cards, &n, max, item, ITEM_UNARMED, i);
        phwAddPairs(cards, &n, max, item, i);
    }
    /* A pair outlives its single gun when a mission removes that gun. */
    while (inv) {
        if (inv->type == INV_ITEM_DUAL && phwStripItem(inv->type_inv_item.type_dual.weapon_right))
            phwAdd(cards, &n, max, inv->type_inv_item.type_dual.weapon_right,
                inv->type_inv_item.type_dual.weapon_left, -1);
        inv = inv->next;
        if (inv == first) break;
    }
    *selected = phwFind(cards, n, current, current == held ? getCurrentPlayerWeaponId(GUNLEFT) : ITEM_UNARMED);
    if (*selected < 0) *selected = phwFind(cards, n, current, ITEM_UNARMED);
    return n;
}

static void phwCopy(char *out, s32 size, const char *text)
{
    s32 n = 0;
    if (size <= 0) return;
    if (text) while (*text && n < size - 1) {
        const char c = *text++;
        out[n++] = c == '\n' || c == '\r' || c == '\t' ? ' ' : c;
    }
    while (n && out[n-1] == ' ') n--;
    out[n] = '\0';
}

s32 portHudRifleButt(void)
{
    const s32 held = getCurrentPlayerWeaponId(GUNRIGHT);
    /* Every weapon swap records the club the next unarmed switch uses (gunfire.c):
     * in hand it is the truth; otherwise ownership predicts the next switch. */
    if (held == ITEM_UNARMED || held == ITEM_FIST)
        return g_CurrentPlayer->cur_item_weapon_getname == ITEM_SNIPERRIFLE;
    return bondinvItemAvailable(ITEM_SNIPERRIFLE) != 0;
}

void portHudWeaponItemName(const HudWeapon *cards, s32 count, s32 item, char *out, s32 size)
{
    const s32 single = phwFind(cards, count, item, ITEM_UNARMED);
    const char *name = NULL;
    if ((item == ITEM_UNARMED || item == ITEM_FIST) && portHudRifleButt()) { phwCopy(out, size, "Buttstroke"); return; }
    if (single >= 0 && cards[single].inventory >= 0)
        name = (const char *)bondinvGetNameByIndex(cards[single].inventory);
    if (!name || !name[0]) name = (const char *)get_ptr_short_watch_text_for_item(item);
    phwCopy(out, size, name && name[0] ? name : "Unarmed");
}

void portHudWeaponName(const HudWeapon *cards, s32 count, const HudWeapon *card, char *out, s32 size)
{
    char right[64], left[64];
    portHudWeaponItemName(cards, count, card->item, right, sizeof(right));
    if (card->left == ITEM_UNARMED) { phwCopy(out, size, right); return; }
    if (card->left == card->item) { snprintf(out, size, "%s x2", right); return; }
    /* Screen order: the left hand's gun first. */
    portHudWeaponItemName(cards, count, card->left, left, sizeof(left));
    snprintf(out, size, "%s + %s", left, right);
}
