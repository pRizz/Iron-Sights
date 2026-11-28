/* Fork identity on the inherited legal screen (original credits stay intact)
 * and a dim build stamp on the file select screen. */
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "game/textrelated.h"
#include "game/front.h"
#include "port_buildinfo.h"
#include "port_build_time.h"
#include "port_startup.h"
#include "platform_info.h"

/* LP64-007: a 64-bit build says so on its legal screen; a comparison build
 * keeps its fixed stamp. */
#if IS_64_BIT && !defined(GE_COMPARE)
#define STAMP_WIDTH " - 64-bit"
#endif

#ifdef GE_COMPARE
/* LP64-006: a comparison build draws one legal screen and file-select stamp
 * whatever tree, commit, minute or time zone built it, so that two builds of
 * one source draw the same picture. */
#define STAMP_NUMBER  "comparison"
#define STAMP_COMMIT  "build"
#define STAMP_DIRTY   0
#define STAMP_TIME    NULL
#else
#define STAMP_NUMBER  PORT_BUILD_STRING
#define STAMP_COMMIT  PORT_BUILD_COMMIT
#define STAMP_DIRTY   PORT_BUILD_DIRTY
#define STAMP_TIME    PORT_BUILD_TIME
#endif

/* front.c's legal-screen primitive draws in its 440 x 330 logical viewport. */
static Gfx *startupLine(Gfx *gdl, s32 y, const char *text)
{
    return display_aligned_white_text_to_screen(gdl, 220, y,
        CENTER_ALIGN, CENTER_ALIGN, (u8 *)text,
        ptrFontZurichBoldChars, ptrFontZurichBold);
}

Gfx *portStartupAttribution(Gfx *gdl)
{
    char line[128], builtAt[96];
#ifdef GE_UPDATER
    { /* FORK-095: a freshly installed build has started properly */
        extern void portUpdateLegalScreen(void);
        portUpdateLegalScreen();
    }
#endif
    gdl = startupLine(gdl, 16, "Iron Sights - a fork of Project Midas\n");
#ifdef STAMP_WIDTH
    snprintf(line, sizeof(line), "Build %s - %s%s" STAMP_WIDTH "\n", STAMP_NUMBER,
        STAMP_COMMIT, STAMP_DIRTY ? " (modified)" : "");
#else
    snprintf(line, sizeof(line), "Build %s - %s%s\n", STAMP_NUMBER,
        STAMP_COMMIT, STAMP_DIRTY ? " (modified)" : "");
#endif
    gdl = startupLine(gdl, 300, line);
    portFormatBuildTime(builtAt, sizeof(builtAt), STAMP_TIME);
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
    snprintf(line, sizeof(line), "Build %s - %s\n", STAMP_NUMBER, STAMP_COMMIT);
    gdl = stampLine(gdl, left, &y, line);
    /* "modified" rides the shorter line so neither reaches the button column
     * (Import/Export start near x 185 on web and save-transfer builds). */
    portFormatBuildTimeCompact(builtAt, sizeof(builtAt), STAMP_TIME);
    snprintf(line, sizeof(line), "Built %s%s\n", builtAt, STAMP_DIRTY ? " - modified" : "");
    gdl = stampLine(gdl, left, &y, line);
    portTextScale(1.0f);
    return gdl;
}
