#ifndef PORT_HUD_WEAPONS_H
#define PORT_HUD_WEAPONS_H
#include <ultra64.h>

/* One weapon-strip card per weapon-cycle stop. left is ITEM_UNARMED for a
 * single gun; otherwise the card is a two-gun pair (doubles or mismatched). */
typedef struct { s32 item, left, inventory; } HudWeapon;

#define HUD_WEAPONS 64

/* Cards in cycle order; selected is the card in hand, or -1. */
s32 portHudWeapons(HudWeapon *cards, s32 max, s32 *selected);
/* "PP7", "PP7 x2", or the left and right guns: "AR33 + RC-P90". */
void portHudWeaponName(const HudWeapon *cards, s32 count, const HudWeapon *card, char *out, s32 size);
/* Whether the unarmed card is the rifle butt: native switches to unarmed use the
 * sniper rifle as a club while Bond owns one. */
s32 portHudRifleButt(void);
/* Short name of one gun, preferring its single card's mission text. */
void portHudWeaponItemName(const HudWeapon *cards, s32 count, s32 item, char *out, s32 size);

#endif
