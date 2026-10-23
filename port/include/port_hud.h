#ifndef PORT_HUD_H
#define PORT_HUD_H
#include <ultra64.h>
#define PORT_HUD_WEAPON_BOTTOM 36

s32 portHudOptionGet(const char *key, s32 *value);
s32 portHudOptionSet(const char *key, s32 value);
void portHudReset(s32 player);
void portHudTick(void);
void portHudHit(s32 attacker, s32 headshot);
void portHudDamage(s32 victim, f32 incomingX, f32 incomingZ);
s32 portHudObjective(s32 index, s32 letter, s32 status, const char *text);
s32 portHudWeaponStripShown(void);
s32 portHudAlwaysHealthShown(void);
struct damage_display_val;
void portHudStyleGauge(struct damage_display_val *vertices, s32 count);
Gfx *portHudDraw(Gfx *gdl);
/* Shared smooth strokes for gameplay sights and menu/watch cursors. */
Gfx *portHudReticle(Gfx *gdl, f32 x, f32 y, f32 scaleX, f32 scaleY, u32 color, s32 ring);
#endif
