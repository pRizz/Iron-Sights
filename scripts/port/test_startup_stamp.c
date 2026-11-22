/* ROM-free regression (LP64-006 Task 8): a comparison build's legal screen and
 * file-select stamp draw the same text whatever build information the tree
 * baked in. Prints each line port_startup.c asks to draw; the shell compares
 * two builds of it made with different port_buildinfo.h values. */
#include <stdio.h>
#include <ultra64.h>
#include "game/textrelated.h"
#include "game/front.h"
#include "port_startup.h"

struct font *ptrFontZurichBold;
struct fontchar *ptrFontZurichBoldChars;

Gfx *display_aligned_white_text_to_screen(Gfx *dl, s32 x, s32 y, s32 halign, s32 valign, u8 *text,
                                          struct fontchar *chars, struct font *font)
{
    printf("legal %d: %s", (int) y, (const char *) text);
    return dl;
}

Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text, struct fontchar *chars, struct font *font, u32 colour,
                s32 width, s32 height, u32 yOffset, s32 lineheight)
{
    printf("stamp: %s", text);
    return gdl;
}

void textMeasure(s32 *textheight, s32 *textwidth, char *text, struct fontchar *font1, struct font *font2,
                 s32 lineheight)
{
    *textheight = 10;
    *textwidth = 100;
}

void portTextScale(f32 scale) {}
Gfx *microcode_constructor(Gfx *gdl) { return gdl; }
s16 viGetX(void) { return 320; }
s16 viGetY(void) { return 240; }

int main(void)
{
    Gfx list[4];
    portStartupAttribution(list);
    portFileSelectBuildStamp(list, 10, 20);
    return 0;
}
