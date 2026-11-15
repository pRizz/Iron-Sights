#ifndef PORT_HITMAP_DRAW_H
#define PORT_HITMAP_DRAW_H
/* Shared by the debrief hit map's drawing files (FORK-048): port_hitmap_draw.c
 * draws the overview, the closer look's figures and its tabs,
 * port_hitmap_parts.c draws the closer look's Parts view and
 * port_hitmap_guns.c its gun filter. */
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
/* The menu cursor inside a native-page rectangle. */
s32 portHitMapCursorIn(f32 x, f32 y, f32 w, f32 h);
/* The tabs' click, for the map's own buttons. */
void portHitMapSound(void);
/* The Parts view: hits, kill shots and damage by body part, and the tally. */
Gfx *portHitMapPartsDraw(Gfx *g, s32 bodyHits);

/* The gun filter (port_hitmap_guns.c): the gun shown (PORT_HITMAP_ALL_GUNS
 * or an ITEM_IDS gun) and whether its list is open. */
s32 portHitMapGunShown(void);
s32 portHitMapGunsOpen(void);
/* Back to All guns with the list shut, as the debrief opens. */
void portHitMapGunsReset(void);
/* Test harness only: pick the list's nth entry (0 is All guns), open the list. */
void portHitMapGunsPick(s32 entry, s32 open);
/* This frame's presses (confirm, B); TRUE when they were the filter's. While
 * the list is open it takes every press, and B or a confirm off it shuts it. */
s32 portHitMapGunsInput(s32 confirm, s32 back);
Gfx *portHitMapGunsRow(Gfx *g);
/* The open list, drawn over the rest of the closer look. */
Gfx *portHitMapGunsList(Gfx *g);
/* The debrief's head, body and limb hits for the shown guns. */
s32 portHitMapGunBodyHits(s32 bodyHits);
#endif
