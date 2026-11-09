/* Fork identity on the inherited legal screen (original credits stay intact)
 * and a dim build stamp on the file select screen. */
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "game/textrelated.h"
#include "port_buildinfo.h"
#include "port_build_time.h"
#include "port_startup.h"

/* front.c's legal-screen primitive, in its 440 x 330 logical viewport. */
extern Gfx *display_aligned_white_text_to_screen(Gfx *gdl, s32 x, s32 y,
    s32 halign, s32 valign, u8 *text, s32 chars, s32 font);

static Gfx *startupLine(Gfx *gdl, s32 y, const char *text)
{
    return display_aligned_white_text_to_screen(gdl, 220, y,
        CENTER_ALIGN, CENTER_ALIGN, (u8 *)text,
        (s32)ptrFontZurichBoldChars, (s32)ptrFontZurichBold);
}

Gfx *portStartupAttribution(Gfx *gdl)
{
    char line[128], builtAt[96];
    gdl = startupLine(gdl, 16, "Iron Sights - a fork of Project Midas\n");
    snprintf(line, sizeof(line), "Build %s - %s%s\n", PORT_BUILD_STRING,
        PORT_BUILD_COMMIT, PORT_BUILD_DIRTY ? " (modified)" : "");
    gdl = startupLine(gdl, 300, line);
    portFormatBuildTime(builtAt, sizeof(builtAt), PORT_BUILD_TIME);
    snprintf(line, sizeof(line), "Built at: %s\n", builtAt);
    return startupLine(gdl, 315, line);
}

/* Small and grey so it reads as a footnote beside Rare's SELECT FILE art. */
#define STAMP_SCALE   0.5f
#define STAMP_COLOUR  0x9C9C9CB4u
#define STAMP_GAP     3

extern s16 viGetX(void);
extern s16 viGetY(void);

static Gfx *stampLine(Gfx *gdl, s32 left, s32 *y, char *text)
{
    s32 x = left, height = 0, width = 0, top = *y;
    textMeasure(&height, &width, text, ptrFontZurichBoldChars, ptrFontZurichBold, 0);
    gdl = textRender(gdl, &x, &top, text, ptrFontZurichBoldChars, ptrFontZurichBold,
        STAMP_COLOUR, viGetX(), viGetY(), 0, 0);
    *y += height;
    return gdl;
}

Gfx *portFileSelectBuildStamp(Gfx *gdl, s32 left, s32 top)
{
    char line[128], builtAt[96];
    s32 y = top + STAMP_GAP;
    gdl = microcode_constructor(gdl); /* text setup, not the art's combiner */
    portTextScale(STAMP_SCALE);
    snprintf(line, sizeof(line), "Build %s - %s\n", PORT_BUILD_STRING, PORT_BUILD_COMMIT);
    gdl = stampLine(gdl, left, &y, line);
    /* "modified" rides the shorter line so neither reaches the button column
     * (Import/Export start near x 185 on web and save-transfer builds). */
    portFormatBuildTimeCompact(builtAt, sizeof(builtAt), PORT_BUILD_TIME);
    snprintf(line, sizeof(line), "Built %s%s\n", builtAt, PORT_BUILD_DIRTY ? " - modified" : "");
    gdl = stampLine(gdl, left, &y, line);
    portTextScale(1.0f);
    return gdl;
}
