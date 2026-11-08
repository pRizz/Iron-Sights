/* FORK-058: Shiny Gold Guns (port_shiny.c, port/fast3d/gfx_shiny.h). */
#ifndef PORT_SHINY_H
#define PORT_SHINY_H

#include <ultra64.h>

/* Opens (on) or closes the first-person draw span of a shiny weapon: the
 * Cougar Magnum, Golden Gun, Silver PP7 and Gold PP7. Other items emit nothing. */
Gfx *portShinyWeaponTag(Gfx *gdl, s32 item, s32 on);

#endif
