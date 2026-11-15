/* Graphics (FORK-082): Gameplay > Iron Sights > Graphics.
 *
 * - Tone Mapping: the level, characters and (with Gun and Hands) the
 *   first-person viewmodel are re-graded after they draw - adaptive sharpening,
 *   brightness, contrast, glow from bright areas, a filmic highlight roll-off
 *   that leaves shadows alone, local contrast (Clarity) and vibrance - while
 *   the HUD, watch and menus keep their stock colours (gfx_grade.inl).
 * - HDR Output (Windows): a DXGI scRGB swap chain presents the frame; graded
 *   highlights go above paper white up to the display's peak (gfx_hdr.inl,
 *   gfx_hdr_dxgi.cpp). Only while Windows has HDR on for that monitor.
 * - Baked Lighting: ray-traced ambient occlusion and bounce light baked into
 *   the room vertex colours once per stage on worker threads
 *   (port_baked_light.c, port_baked_trace.c); with Gun and Hands, a probe at
 *   the camera lights the viewmodel to match its surroundings.
 * Each has a bindable toggle (F2-F4, F7; F11 splits the screen stock / graded)
 * that shows a console message and saves like the menu row. Every slider on
 * the page can be changed in play too: Home / End pick the slider, Page Up /
 * Page Down step it by its menu step (clamped, not wrapped), with a message.
 * Looks (Insert cycles them) set all the grading sliders at once. */
#ifndef PORT_GRAPHICS_H
#define PORT_GRAPHICS_H

#include <ultra64.h>
#include "port_graphics_render.h"

/* On by default on Windows and Linux, with the Vibrant Look (2026-11-15, the
 * owner's choice); off in the browser and on the Xbox, where they are not yet
 * tested. Capture runs (GE_WINDOW) start with them off too:
 * portGraphicsCaptureDefaults. */
#if defined(__EMSCRIPTEN__) || defined(GE_XBOX)
#define PORT_GRAPHICS_ON_DEFAULT 0
#else
#define PORT_GRAPHICS_ON_DEFAULT 1
#endif
#define PORT_TONE_MAPPING_DEFAULT PORT_GRAPHICS_ON_DEFAULT
#define PORT_VIBRANCE_DEFAULT 80
/* the Vibrant Look's values */
#define PORT_BRIGHTNESS_DEFAULT 150
#define PORT_CONTRAST_DEFAULT 90
#define PORT_INTENSITY_DEFAULT 50
#define PORT_CLARITY_DEFAULT 60
#define PORT_SHARPEN_DEFAULT 50
#define PORT_GLOW_DEFAULT 100
#define PORT_SATURATION_DEFAULT 100
#define PORT_AUTO_EXPOSURE_DEFAULT 60 /* Vibrant's */
#define PORT_ADAPT_SPEED_DEFAULT 100
#define PORT_LOOK_DEFAULT PORT_LOOK_VIBRANT

/* Looks (port_graphics_looks.c): named sets of the grading sliders */
enum {
    PORT_LOOK_CUSTOM,
    PORT_LOOK_NATURAL,
    PORT_LOOK_VIBRANT,
    PORT_LOOK_CINEMATIC,
    PORT_LOOK_NOIR,
    PORT_LOOK_MUTED,
    PORT_LOOK_COLD_WAR,
    PORT_LOOK_RETRO,
    PORT_LOOK_DARK,
    PORT_LOOK_COUNT
};
#define PORT_HDR_OUTPUT_DEFAULT 0
#define PORT_HDR_PEAK_DEFAULT 1000
#define PORT_HDR_PAPER_DEFAULT 200
#define PORT_BAKED_LIGHTING_DEFAULT PORT_GRAPHICS_ON_DEFAULT
#define PORT_BAKED_STRENGTH_DEFAULT 100
#define PORT_GRAPHICS_VIEWMODEL_DEFAULT 1

/* the hotkeys, in joy_port.c action order (PORTACT_TOGGLE_TONE_MAPPING + n) */
enum {
    PORT_GFX_KEY_TONE_MAPPING,
    PORT_GFX_KEY_BAKED_LIGHTING,
    PORT_GFX_KEY_HDR_OUTPUT,
    PORT_GFX_KEY_VIEWMODEL,
    PORT_GFX_KEY_COMPARE,
    PORT_GFX_KEY_SLIDER_NEXT, /* pick the slider the next two keys move */
    PORT_GFX_KEY_SLIDER_PREV,
    PORT_GFX_KEY_SLIDER_UP,
    PORT_GFX_KEY_SLIDER_DOWN,
    PORT_GFX_KEY_LOOK_NEXT,   /* the next Look (turns Tone Mapping on) */
    PORT_GFX_KEY_COUNT
};

#ifdef __cplusplus
extern "C" {
#endif

s32 portGraphicsOptionGet(const char *key, s32 *value);
/* video.c, before midas.ini is read: capture runs (GE_WINDOW: audits, the
 * review gallery, icon export) start with Tone Mapping and Baked Lighting off,
 * so their images stay comparable with stock; their own ini can still set them */
void portGraphicsCaptureDefaults(void);
s32 portGraphicsOptionSet(const char *key, s32 value);
/* a hotkey press (joy_port.c portStateInput, every frame) */
void portGraphicsHotkey(s32 which);
/* once per frame: reports HDR Output becoming unavailable */
void portGraphicsTick(void);
/* bondview2.c maybe_mp_interface, around the first-person guns: the grade tag
 * (G_NOOP 0x66) goes after them with Gun and Hands on, before them otherwise;
 * the viewmodel tag (G_NOOP 0x67) carries the baked-light probe for them */
s32 portGraphicsGradesViewmodel(void);
Gfx *portGraphicsGradeTag(Gfx *gdl);
Gfx *portGraphicsViewmodelTag(Gfx *gdl);

/* Looks (port_graphics_looks.c) */
const char *portGraphicsLookName(s32 look);
s32 portGraphicsLook(void);
void portGraphicsLookSet(s32 look);     /* a preset: apply it; Custom: keep the values */
void portGraphicsLookRecheck(void);     /* a grading slider changed */
s32 portGraphicsLookNext(void);

/* Baked Lighting (port_baked_light.c) */
s32 portBakedLightingOn(void);
s32 portBakedStrength(void);
void portBakedLightingChanged(void);
void portBakeLevelBegin(void);
void portBakeCaptureRoom(s32 room);
void portBakeLevelLoaded(void);
void portBakeApplyRoom(s32 room);
void portBakeLevelUnload(void);
void portBakeTick(void);
/* the probe at the current player's eye: rgb multipliers, 1 = none */
void portBakeViewmodelProbe(f32 out_rgb[3]);
/* "Baking 42%" while running, NULL when idle (the hotkey message) */
const char *portBakeStatusText(void);

#ifdef __cplusplus
}
#endif

#endif
