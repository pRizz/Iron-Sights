/* FORK-058: Richer Reflections and Gun Glint (port_shiny.c, port/fast3d/gfx_shiny.h). */
#ifndef PORT_SHINY_H
#define PORT_SHINY_H

#include <ultra64.h>

/* Opens (on) or closes the first-person draw span of a shiny weapon (the
 * Cougar Magnum, Golden Gun, Silver PP7 and Gold PP7) or an ordinary gun, PP7
 * to sniper rifle (Gun Glint). Other items emit nothing. */
Gfx *portShinyWeaponTag(Gfx *gdl, s32 item, s32 on);
/* Brackets draws inside that span that never take a glint: the generated
 * sleeve and the native hand pass, which stay exactly as drawn. */
Gfx *portShinyGlintBlock(Gfx *gdl, s32 on);

#endif
