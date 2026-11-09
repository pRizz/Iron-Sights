#ifndef PORT_HITMAP_DRAW_H
#define PORT_HITMAP_DRAW_H
/* Shared by the debrief hit map's drawing files (FORK-048): port_hitmap_draw.c
 * draws the overview, the closer look's figures and its tabs, and
 * port_hitmap_parts.c draws the closer look's Parts view. */
#include <ultra64.h>

/* Ink on the dossier: parts outlined in ink and shaded red by their hits. */
#define PAPER_TEXT  0x000000ffu
#define INK         0x22303cffu
#define HEAT_RGB    0xc81e2300u /* crimson, up to 85% over the paper */

/* The closer look's top row, below the stage name. */
#define ZOOM_TOP 143

/* Text in the dossier's Zurich Bold at scale, and its width. */
Gfx *portHitMapInk(Gfx *g, const char *text, s32 x, s32 y, f32 scale, u32 color);
s32 portHitMapInkWidth(const char *text, f32 scale);
/* The Parts view: hits, kill shots and damage by body part, and the tally. */
Gfx *portHitMapPartsDraw(Gfx *g, s32 bodyHits);
#endif
