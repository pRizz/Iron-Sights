/* Headshots Only (FORK-083): its row on each mission's difficulty page
 * (front.c constructor_menu08_difficulty / interface_menu08_difficulty), its
 * mark and the debrief badge. */
#ifndef PORT_HEADSHOTS_MENU_H
#define PORT_HEADSHOTS_MENU_H

#include <ultra64.h>

s32 portHeadshotsMenuHit(f32 cursorX, f32 cursorY);
void portHeadshotsMenuStep(void);
Gfx *portHeadshotsMenuDraw(Gfx *dl, s32 highlighted);
/* The crosshair-and-head mark, centred at x,y with ring radius r (its ticks
 * reach about 1.37 r). */
Gfx *portHeadshotsMarkDraw(Gfx *dl, f32 x, f32 y, f32 r, u32 color);
/* The debrief's badge after its STATISTICS heading, for a Headshots Only run. */
Gfx *portHeadshotsDebriefDraw(Gfx *dl, s32 headingRightX, s32 y);

#endif
