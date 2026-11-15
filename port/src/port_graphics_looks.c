/* Graphics (FORK-082): Looks - named sets of the Tone Mapping sliders.
 *
 * Choosing a Look (the Look row, or the Insert hotkey) writes every grading
 * slider at once; the hotkey also turns Tone Mapping on (the row and midas.ini
 * leave it alone, so a saved Look never re-enables it). Changing any of those sliders
 * afterwards, from a menu, a hotkey or midas.ini, leaves the values as they are
 * and the Look reads Custom; a Look whose values all match again reads as that
 * Look. midas.ini writes graphics_look before the sliders, so loading a saved
 * Look applies it and the sliders that follow agree with it. HDR calibration
 * and Baked Lighting are not part of a Look. */
#include <string.h>
#include "port_graphics.h"

/* the grading sliders a Look sets, in table column order */
static const char *const sLookKeys[] = {
    "tone_brightness", "tone_contrast", "tone_intensity", "vibrance", "tone_clarity", "tone_sharpen", "tone_glow",
    "tone_saturation", "tone_temperature", "tone_split", "tone_split_colours", "tone_fade", "tone_vignette",
    "tone_grain", "tone_auto_exposure", "tone_adapt_speed",
};
#define LOOK_KEYS ((s32) (sizeof(sLookKeys) / sizeof(sLookKeys[0])))

/* split colours: 0 teal / orange, 1 green / gold, 2 blue / amber, 3 sepia (brown / cream) */
static const struct { const char *name; s32 v[LOOK_KEYS]; } sLooks[PORT_LOOK_COUNT] = {
    /*                 bri  con  int  vib  cla  shp  glo  sat  tmp  spl col fad vig grn  ae  spd */
    [PORT_LOOK_CUSTOM]    = { "Custom",     { 0 } },
    [PORT_LOOK_NATURAL]   = { "Natural",    { 100, 100,  50,  50,  25,  25,  25, 100,   0,   0, 0,  0,  0,  0, 50, 100 } },
    /* the owner's own settings of 2026-11-15, with Auto Exposure for bright snow */
    [PORT_LOOK_VIBRANT]   = { "Vibrant",    { 150,  90,  50,  80,  60,  50, 100, 100,   0,   0, 0,  0,  0,  0, 60, 100 } },
    [PORT_LOOK_CINEMATIC] = { "Cinematic",  { 105, 115,  70,  40,  35,  30,  40, 100,   0,  40, 0,  0, 30,  0, 50, 100 } },
    [PORT_LOOK_NOIR]      = { "Noir",       { 100, 130,  60,   0,  50,  40,  30,   0,   0,   0, 0, 10, 50, 35, 40, 100 } },
    [PORT_LOOK_MUTED]     = { "Muted",      { 100,  85,  40,   0,  15,  20,   0,  70,   0,   0, 0, 25, 10,  0, 40, 100 } },
    [PORT_LOOK_COLD_WAR]  = { "Cold War",   {  95, 105,  50,  20,  35,  30,  20,  80, -40,  40, 1, 10, 20, 10, 50, 100 } },
    [PORT_LOOK_RETRO]     = { "Warm Retro", { 105,  95,  60,  30,  20,  20,  45,  90,  60,  40, 3, 30, 25, 30, 40, 100 } },
    [PORT_LOOK_DARK]      = { "Dark",       {  75, 120,  60,  30,  40,  30,  30,  90, -10,   0, 0,  0, 50, 10, 25, 100 } },
};

static s32 sLook = PORT_LOOK_DEFAULT;
static s32 sApplying;

const char *portGraphicsLookName(s32 look)
{
    return (look >= 0 && look < PORT_LOOK_COUNT) ? sLooks[look].name : "Custom";
}

s32 portGraphicsLook(void)
{
    return sLook;
}

static s32 lookMatches(s32 look)
{
    s32 i, v;
    for (i = 0; i < LOOK_KEYS; i++) {
        if (!portGraphicsOptionGet(sLookKeys[i], &v) || v != sLooks[look].v[i]) return FALSE;
    }
    return TRUE;
}

void portGraphicsLookSet(s32 look)
{
    s32 i;
    if (look <= PORT_LOOK_CUSTOM || look >= PORT_LOOK_COUNT) {
        sLook = PORT_LOOK_CUSTOM;
        return;
    }
    sApplying = TRUE;
    for (i = 0; i < LOOK_KEYS; i++) portGraphicsOptionSet(sLookKeys[i], sLooks[look].v[i]);
    sApplying = FALSE;
    sLook = look;
}

void portGraphicsLookRecheck(void)
{
    s32 look;
    if (sApplying) return;
    if (sLook != PORT_LOOK_CUSTOM && lookMatches(sLook)) return;
    sLook = PORT_LOOK_CUSTOM;
    for (look = PORT_LOOK_CUSTOM + 1; look < PORT_LOOK_COUNT; look++) {
        if (lookMatches(look)) {
            sLook = look;
            return;
        }
    }
}

s32 portGraphicsLookNext(void)
{
    portGraphicsLookSet(sLook + 1 < PORT_LOOK_COUNT ? sLook + 1 : PORT_LOOK_CUSTOM + 1);
    return sLook;
}
