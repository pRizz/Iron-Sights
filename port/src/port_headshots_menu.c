/* Headshots Only (FORK-083) on each mission's difficulty page.
 *
 * A "Headshots Only:" row on the Reinforcements line, from the page's left
 * margin: pointing at it highlights it like a difficulty, and the confirm
 * press (A, Start or Z on a pad, Enter, a mouse click or a tap) flips it. It
 * writes the same midas.ini key as Gameplay > Iron Sights > Enemies.
 *
 * The crosshair-and-head mark, in the stock checkmark's red, heads the
 * Headshots Only best-times column (port_mission_times.c) and the debrief's
 * badge. */
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <fr.h>
#include "game/textrelated.h"
#include "port_headshots_only.h"
#include "port_headshots_menu.h"
#include "port_hud.h"

extern Gfx *frontPrintText(Gfx *dl, s32 *x, s32 *y, s8 *text, s32 chars, s32 font,
    s32 color, s32 viewX, s32 viewY, s32 arg9, s32 arga);
extern s32 portOptFind(const char *key);
extern void portOptStep(s32 i, s32 dir);
extern const char *portOptValueText(s32 i);

/* The Reinforcements row's line (port_reinforcements_menu.c), from the
 * "DIFFICULTY:" heading's x; it ends well left of that row's text. */
#define ROW_X 55
#define ROW_Y 295
#define ROW_PAD_X 4
#define ROW_LABEL "Headshots Only: "
#define VALUE_RED 0xA00000FFu   /* the dossier's emphasis red, as the cheat badges */
#define MARK_RADIUS 4.4f   /* the debrief mark's ring; its ticks reach about 6 */
#define MARK_RED 0xB40000FFu   /* the checkmark's red (front.c draws it 0xB4,0,0) */

static void rowText(char *text, s32 size)
{
    const s32 opt = portOptFind("headshots_only");
    const char *value = opt >= 0 ? portOptValueText(opt) : "OFF\n";
    snprintf(text, size, ROW_LABEL "%s", value);
}

static s32 rowRightX(void)
{
    char text[48];
    s32 height, width;
    if (!ptrFontZurichBoldChars || !ptrFontZurichBold) return ROW_X;
    rowText(text, sizeof(text));
    textMeasure(&height, &width, text, ptrFontZurichBoldChars, ptrFontZurichBold, 0);
    return ROW_X + width;
}

s32 portHeadshotsMenuHit(f32 cursorX, f32 cursorY)
{
    return cursorX >= (f32)(ROW_X - ROW_PAD_X) && cursorX < (f32)(rowRightX() + ROW_PAD_X)
        && cursorY >= (f32)(ROW_Y - 2) && cursorY < (f32)(ROW_Y + 15);
}

void portHeadshotsMenuStep(void)
{
    const s32 opt = portOptFind("headshots_only");
    if (opt >= 0) portOptStep(opt, 1);
}

Gfx *portHeadshotsMenuDraw(Gfx *dl, s32 highlighted)
{
    const s32 opt = portOptFind("headshots_only");
    const char *value = opt >= 0 ? portOptValueText(opt) : "OFF\n";
    /* ON reads in red */
    const u32 valueColor = portHeadshotsOnlySetting() ? VALUE_RED : 0x000000ffu;
    s32 x = ROW_X, y = ROW_Y, labelHeight, labelWidth;
    if (!ptrFontZurichBoldChars || !ptrFontZurichBold) return dl;
    if (highlighted)
    {
        /* the same box the difficulty rows get */
        dl = microcode_constructor_related_to_menus(dl, ROW_X - ROW_PAD_X, ROW_Y - 2, rowRightX() + ROW_PAD_X, ROW_Y + 15, 0x32);
    }
    textMeasure(&labelHeight, &labelWidth, ROW_LABEL, ptrFontZurichBoldChars, ptrFontZurichBold, 0);
    dl = microcode_constructor(dl);
    {
        s32 lx = x, ly = y;
        dl = frontPrintText(dl, &lx, &ly, (s8 *)ROW_LABEL, (s32)ptrFontZurichBoldChars, (s32)ptrFontZurichBold,
            0x000000ff, viGetX(), viGetY(), 0, 0);
    }
    x += labelWidth;
    dl = microcode_constructor(dl);
    return frontPrintText(dl, &x, &y, (s8 *)value, (s32)ptrFontZurichBoldChars, (s32)ptrFontZurichBold,
        valueColor, viGetX(), viGetY(), 0, 0);
}

/* A scope reticle around a head and shoulders, ring radius r. Each of the
 * four straight ticks shows as much of itself inside the ring as outside it
 * (0.28 r each way, clear of the figure's head); the reach is about 1.37 r. */
Gfx *portHeadshotsMarkDraw(Gfx *dl, f32 x, f32 y, f32 r, u32 color)
{
    const f32 stroke = r * 0.17f, part = r * 0.28f;
    const f32 tickIn = r - stroke * 0.5f - part, tickOut = r + stroke * 0.5f + part;
    const f32 tick = tickOut - tickIn;
    dl = portHudEllipse(dl, x, y, r, r, 0.0f, stroke, color);
    dl = portHudPanel(dl, x - stroke * 0.5f, y - tickOut, stroke, tick, color);
    dl = portHudPanel(dl, x - stroke * 0.5f, y + tickIn, stroke, tick, color);
    dl = portHudPanel(dl, x - tickOut, y - stroke * 0.5f, tick, stroke, color);
    dl = portHudPanel(dl, x + tickIn, y - stroke * 0.5f, tick, stroke, color);
    /* the head centred on the ring's centre; the shoulders below it meet
     * the ring's inner edge */
    dl = portHudEllipse(dl, x, y, r * 0.31f, r * 0.33f, 0.0f, 0.0f, color);
    return portHudEllipse(dl, x, y + r * 0.67f, r * 0.52f, r * 0.28f, 0.0f, 0.0f, color);
}

/* The debrief: the mark and "HEADSHOTS ONLY" after its STATISTICS heading,
 * when the mission just played ran the mode (the latch holds until the next
 * stage loads; the front end is not one). */
Gfx *portHeadshotsDebriefDraw(Gfx *dl, s32 headingRightX, s32 y)
{
    s32 x = headingRightX + 24;
    if (!portHeadshotsOnlyStageOn() || !ptrFontZurichBoldChars || !ptrFontZurichBold) return dl;
    dl = portHeadshotsMarkDraw(dl, (f32)(headingRightX + 13), (f32)(y + 6), MARK_RADIUS, MARK_RED);
    dl = microcode_constructor(dl);
    return frontPrintText(dl, &x, &y, (s8 *)"HEADSHOTS ONLY", (s32)ptrFontZurichBoldChars, (s32)ptrFontZurichBold,
        0xA00000FF, viGetX(), viGetY(), 0, 0);
}
