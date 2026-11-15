/* Reinforcements (FORK-081): its row on each mission's difficulty page
 * (front.c constructor_menu08_difficulty / interface_menu08_difficulty). */
#ifndef PORT_REINFORCEMENTS_MENU_H
#define PORT_REINFORCEMENTS_MENU_H

#include <ultra64.h>

s32 portReinforcementsMenuHit(f32 cursorX, f32 cursorY);
void portReinforcementsMenuStep(void);
Gfx *portReinforcementsMenuDraw(Gfx *dl, s32 highlighted);

#endif
