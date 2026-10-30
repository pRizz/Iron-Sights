/* Fork identity on the inherited legal screen; original credits stay intact. */
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
