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
/* Distance from the viewport bottom to the top of the strip. */
s32 portHudWeaponStripTopInset(void);
s32 portHudAlwaysHealthShown(void);
struct damage_display_val;
void portHudStyleGauge(struct damage_display_val *vertices, s32 count);
Gfx *portHudDraw(Gfx *gdl);
/* Shared port overlay primitives, in native viewport coordinates. */
Gfx *portHudPanel(Gfx *gdl, f32 x, f32 y, f32 w, f32 h, u32 color);
Gfx *portHudLabel(Gfx *gdl, const char *text, s32 x, s32 y, f32 scale, u32 color, s32 maxWidth);
/* Cheat targets apply only to the corresponding solo stage and difficulty. */
s32 portHudMissionTargetSeconds(s32 stage, s32 difficulty);
s32 portHudMissionBestSeconds(s32 stage, s32 difficulty);
void portHudMissionTimerText(s32 ticks, s32 target, char *text, s32 size);
u32 portHudMissionTargetColor(s32 ticks, s32 target);
/* Shared smooth strokes for gameplay sights and menu/watch cursors. */
Gfx *portHudReticle(Gfx *gdl, f32 x, f32 y, f32 scaleX, f32 scaleY, u32 color, s32 ring);
#endif
