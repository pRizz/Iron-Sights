/* Reinforcements (FORK-081) on each mission's difficulty page: a row under
 * the best times that shows the setting, so it can be raised for any
 * mission without visiting the options. Pointing at it highlights it like a
 * difficulty; the confirm press (A, Start or Z on a pad, Enter, a mouse click
 * or a tap) moves it to the next level, Original after Relentless. It writes
 * the same midas.ini key as Gameplay > Iron Sights > Enemies. */
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <fr.h>
#include "game/textrelated.h"
#include "port_reinforcements.h"
#include "port_reinforcements_menu.h"

extern Gfx *frontPrintText(Gfx *dl, s32 *x, s32 *y, s8 *text, s32 chars, s32 font,
    s32 color, s32 viewX, s32 viewY, s32 arg9, s32 arga);
extern s32 portOptFind(const char *key);
extern void portOptStep(s32 i, s32 dir);
extern const char *portOptValueText(s32 i);

/* Under the best times, one difficulty pitch below the 007 row, ending
 * clear of the dossier's tabs; the cheat footer sits at the left. */
#define ROW_RIGHT_X 370
#define ROW_Y 300
#define ROW_PAD_X 4

static void rowText(char *text, s32 size)
{
    const s32 opt = portOptFind("reinforcements");
    const char *value = opt >= 0 ? portOptValueText(opt) : "Original\n";
    snprintf(text, size, "Reinforcements: %s", value);
}

static s32 rowLeftX(void)
{
    char text[48];
    s32 height, width;
    if (!ptrFontZurichBoldChars || !ptrFontZurichBold) return ROW_RIGHT_X;
    rowText(text, sizeof(text));
    textMeasure(&height, &width, text, ptrFontZurichBoldChars, ptrFontZurichBold, 0);
    return ROW_RIGHT_X - width;
}

s32 portReinforcementsMenuHit(f32 cursorX, f32 cursorY)
{
    return cursorX >= (f32)(rowLeftX() - ROW_PAD_X) && cursorX < (f32)(ROW_RIGHT_X + ROW_PAD_X)
        && cursorY >= (f32)(ROW_Y - 2) && cursorY < (f32)(ROW_Y + 15);
}

void portReinforcementsMenuStep(void)
{
    const s32 opt = portOptFind("reinforcements");
    if (opt >= 0) portOptStep(opt, 1);
}

Gfx *portReinforcementsMenuDraw(Gfx *dl, s32 highlighted)
{
    char text[48];
    s32 x = rowLeftX(), y = ROW_Y;
    if (!ptrFontZurichBoldChars || !ptrFontZurichBold) return dl;
    if (highlighted)
    {
        /* the same box the difficulty rows get */
        dl = microcode_constructor_related_to_menus(dl, x - ROW_PAD_X, ROW_Y - 2, ROW_RIGHT_X + ROW_PAD_X, ROW_Y + 15, 0x32);
    }
    rowText(text, sizeof(text));
    dl = microcode_constructor(dl);
    return frontPrintText(dl, &x, &y, (s8 *)text, (s32)ptrFontZurichBoldChars, (s32)ptrFontZurichBold,
        0x000000ff, viGetX(), viGetY(), 0, 0);
}
