/* Graphics (FORK-082): the settings, hotkeys and display-list tags. The rules
 * are in port_graphics.h. */
#include <stdio.h>
#include <string.h>
#include <PR/gbi.h>
#include "port_graphics.h"
#include "port_console.h"

struct PortGraphicsRender g_PortGraphicsRender = {
    .tone_mapping = PORT_TONE_MAPPING_DEFAULT, .vibrance = PORT_VIBRANCE_DEFAULT,
    .hdr_output = PORT_HDR_OUTPUT_DEFAULT, .hdr_peak = PORT_HDR_PEAK_DEFAULT, .hdr_paper = PORT_HDR_PAPER_DEFAULT,
    .compare = 0, .hdr_state = PORT_HDR_OFF, .hdr_display_peak = 0,
    .brightness = PORT_BRIGHTNESS_DEFAULT, .contrast = PORT_CONTRAST_DEFAULT, .intensity = PORT_INTENSITY_DEFAULT,
    .clarity = PORT_CLARITY_DEFAULT, .sharpen = PORT_SHARPEN_DEFAULT, .glow = PORT_GLOW_DEFAULT,
    .saturation = PORT_SATURATION_DEFAULT, .temperature = 0, .split = 0, .split_colours = 0, .fade = 0,
    .vignette = 0, .grain = 0, .auto_exposure = PORT_AUTO_EXPOSURE_DEFAULT, .adapt_speed = PORT_ADAPT_SPEED_DEFAULT,
};

/* the Tone Mapping sliders: key, field, range */
static const struct { const char *key; s32 *value; s32 lo, hi; } sToneSliders[] = {
    { "vibrance",        &g_PortGraphicsRender.vibrance,   0, 100 },
    { "tone_brightness", &g_PortGraphicsRender.brightness, 50, 200 },
    { "tone_contrast",   &g_PortGraphicsRender.contrast,   50, 200 },
    { "tone_intensity",  &g_PortGraphicsRender.intensity,  0, 100 },
    { "tone_clarity",    &g_PortGraphicsRender.clarity,    0, 100 },
    { "tone_sharpen",    &g_PortGraphicsRender.sharpen,    0, 100 },
    { "tone_glow",       &g_PortGraphicsRender.glow,       0, 100 },
    { "tone_saturation", &g_PortGraphicsRender.saturation, 0, 200 },
    { "tone_temperature", &g_PortGraphicsRender.temperature, -100, 100 },
    { "tone_split",      &g_PortGraphicsRender.split,      0, 100 },
    { "tone_split_colours", &g_PortGraphicsRender.split_colours, 0, 3 },
    { "tone_fade",       &g_PortGraphicsRender.fade,       0, 100 },
    { "tone_vignette",   &g_PortGraphicsRender.vignette,   0, 100 },
    { "tone_grain",      &g_PortGraphicsRender.grain,      0, 100 },
    { "tone_auto_exposure", &g_PortGraphicsRender.auto_exposure, 0, 100 },
    { "tone_adapt_speed", &g_PortGraphicsRender.adapt_speed, 25, 400 },
};
#define TONE_SLIDERS ((s32) (sizeof(sToneSliders) / sizeof(sToneSliders[0])))

static s32 bakedOn = PORT_BAKED_LIGHTING_DEFAULT;
static s32 bakedStrength = PORT_BAKED_STRENGTH_DEFAULT;
static s32 viewmodelOn = PORT_GRAPHICS_VIEWMODEL_DEFAULT;

extern void portOptSave(void);

static s32 clampi(s32 v, s32 lo, s32 hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

s32 portGraphicsOptionGet(const char *key, s32 *value)
{
    s32 i;
    for (i = 0; i < TONE_SLIDERS; i++) {
        if (strcmp(key, sToneSliders[i].key) == 0) {
            *value = *sToneSliders[i].value;
            return TRUE;
        }
    }
    if (strcmp(key, "tone_mapping") == 0) *value = g_PortGraphicsRender.tone_mapping;
    else if (strcmp(key, "graphics_look") == 0) *value = portGraphicsLook();
    else if (strcmp(key, "hdr_output") == 0) *value = g_PortGraphicsRender.hdr_output;
    else if (strcmp(key, "hdr_peak") == 0) *value = g_PortGraphicsRender.hdr_peak;
    else if (strcmp(key, "hdr_paper_white") == 0) *value = g_PortGraphicsRender.hdr_paper;
    else if (strcmp(key, "baked_lighting") == 0) *value = bakedOn;
    else if (strcmp(key, "baked_lighting_strength") == 0) *value = bakedStrength;
    else if (strcmp(key, "graphics_viewmodel") == 0) *value = viewmodelOn;
    else return FALSE;
    return TRUE;
}

s32 portGraphicsOptionSet(const char *key, s32 value)
{
    s32 i;
    for (i = 0; i < TONE_SLIDERS; i++) {
        if (strcmp(key, sToneSliders[i].key) == 0) {
            *sToneSliders[i].value = clampi(value, sToneSliders[i].lo, sToneSliders[i].hi);
            portGraphicsLookRecheck(); /* a Look whose values no longer match reads Custom */
            return TRUE;
        }
    }
    if (strcmp(key, "graphics_look") == 0) {
        portGraphicsLookSet(value);
        return TRUE;
    }
    if (strcmp(key, "tone_mapping") == 0) g_PortGraphicsRender.tone_mapping = value != 0;
    else if (strcmp(key, "hdr_output") == 0) g_PortGraphicsRender.hdr_output = value != 0;
    else if (strcmp(key, "hdr_peak") == 0) g_PortGraphicsRender.hdr_peak = clampi(value, 400, 2000);
    else if (strcmp(key, "hdr_paper_white") == 0) g_PortGraphicsRender.hdr_paper = clampi(value, 80, 480);
    else if (strcmp(key, "baked_lighting") == 0) {
        bakedOn = value != 0;
        portBakedLightingChanged();
    } else if (strcmp(key, "baked_lighting_strength") == 0) {
        bakedStrength = clampi(value, 25, 200);
        portBakedLightingChanged();
    } else if (strcmp(key, "graphics_viewmodel") == 0) viewmodelOn = value != 0;
    else return FALSE;
    return TRUE;
}

void portGraphicsCaptureDefaults(void)
{
    extern char *getenv(const char *);
    if (getenv("GE_WINDOW") == NULL) return;
    g_PortGraphicsRender.tone_mapping = 0;
    bakedOn = 0;
}

s32 portBakedLightingOn(void)
{
    return bakedOn;
}

s32 portBakedStrength(void)
{
    return bakedStrength;
}

static const char *onOff(s32 v)
{
    return v ? "On" : "Off";
}

static const char *hdrNote(void)
{
    switch (g_PortGraphicsRender.hdr_state) {
    case PORT_HDR_UNSUPPORTED: return " (unavailable here)";
    case PORT_HDR_SDR_DISPLAY: return " (turn on HDR in Windows display settings)";
    default: return "";
    }
}

/* the sliders the Home / End / Page Up / Page Down hotkeys reach, in menu
 * order, with their menu step and the feature they need */
enum { NEEDS_TONE, NEEDS_BAKE, NEEDS_HDR };
static const struct { const char *key; const char *label; s32 step; const char *unit; s32 needs; } sHotSliders[] = {
    { "tone_brightness", "Brightness", 5, "%", NEEDS_TONE },
    { "tone_contrast", "Contrast", 5, "%", NEEDS_TONE },
    { "tone_intensity", "Intensity", 10, "%", NEEDS_TONE },
    { "vibrance", "Vibrance", 10, "%", NEEDS_TONE },
    { "tone_clarity", "Clarity", 5, "%", NEEDS_TONE },
    { "tone_sharpen", "Sharpen", 5, "%", NEEDS_TONE },
    { "tone_glow", "Glow", 5, "%", NEEDS_TONE },
    { "tone_saturation", "Saturation", 10, "%", NEEDS_TONE },
    { "tone_temperature", "Temperature", 10, "", NEEDS_TONE },
    { "tone_split", "Split Tone", 10, "%", NEEDS_TONE },
    { "tone_split_colours", "Split Colours", 1, "", NEEDS_TONE },
    { "tone_fade", "Fade", 5, "%", NEEDS_TONE },
    { "tone_vignette", "Vignette", 5, "%", NEEDS_TONE },
    { "tone_grain", "Film Grain", 5, "%", NEEDS_TONE },
    { "tone_auto_exposure", "Auto Exposure", 10, "%", NEEDS_TONE },
    { "tone_adapt_speed", "Adaptation Speed", 25, "%", NEEDS_TONE },
    { "baked_lighting_strength", "Lighting Strength", 25, "%", NEEDS_BAKE },
#if defined(_WIN32) && !defined(GE_XBOX)
    { "hdr_peak", "HDR Peak Brightness", 100, " nits", NEEDS_HDR },
    { "hdr_paper_white", "HDR Paper White", 20, " nits", NEEDS_HDR },
#endif
};
#define HOT_SLIDERS ((s32) (sizeof(sHotSliders) / sizeof(sHotSliders[0])))
static s32 sHotSlider;

/* a slider's value as the message shows it */
static void sliderValue(s32 i, s32 v, char *buf, s32 len)
{
    static const char *const colours[] = { "Teal / Orange", "Green / Gold", "Blue / Amber", "Sepia" };
    if (strcmp(sHotSliders[i].key, "tone_split_colours") == 0 && v >= 0 && v <= 3) {
        snprintf(buf, len, "%s", colours[v]);
    } else {
        snprintf(buf, len, "%d%s", v, sHotSliders[i].unit);
    }
}

/* why a slider has no visible effect right now, or "" */
static const char *sliderNote(s32 i)
{
    switch (sHotSliders[i].needs) {
    case NEEDS_TONE: return g_PortGraphicsRender.tone_mapping ? "" : " (Tone Mapping is off)";
    case NEEDS_BAKE: return bakedOn ? "" : " (Baked Lighting is off)";
    default: return g_PortGraphicsRender.hdr_output ? "" : " (HDR Output is off)";
    }
}

/* Home / End pick, Page Up / Page Down step; returns whether a value changed */
static s32 sliderHotkey(s32 which)
{
    s32 v = 0, nv;
    char text[24];
    if (which == PORT_GFX_KEY_SLIDER_NEXT || which == PORT_GFX_KEY_SLIDER_PREV) {
        sHotSlider = (sHotSlider + (which == PORT_GFX_KEY_SLIDER_NEXT ? 1 : HOT_SLIDERS - 1)) % HOT_SLIDERS;
        portGraphicsOptionGet(sHotSliders[sHotSlider].key, &v);
        sliderValue(sHotSlider, v, text, sizeof(text));
        portConPrintf(1, "Graphics slider: %s %s%s\n", sHotSliders[sHotSlider].label, text,
                      sliderNote(sHotSlider));
        return FALSE;
    }
    portGraphicsOptionGet(sHotSliders[sHotSlider].key, &v);
    portGraphicsOptionSet(sHotSliders[sHotSlider].key,
                          v + (which == PORT_GFX_KEY_SLIDER_UP ? 1 : -1) * sHotSliders[sHotSlider].step);
    portGraphicsOptionGet(sHotSliders[sHotSlider].key, &nv);
    sliderValue(sHotSlider, nv, text, sizeof(text));
    portConPrintf(1, "%s: %s%s\n", sHotSliders[sHotSlider].label, text,
                  nv == v ? " (limit)" : sliderNote(sHotSlider));
    return nv != v;
}

void portGraphicsHotkey(s32 which)
{
    if (which >= PORT_GFX_KEY_SLIDER_NEXT && which <= PORT_GFX_KEY_SLIDER_DOWN) {
        if (sliderHotkey(which)) portOptSave();
        return;
    }
    if (which == PORT_GFX_KEY_LOOK_NEXT) {
        g_PortGraphicsRender.tone_mapping = 1; /* a Look shows only through Tone Mapping */
        portConPrintf(1, "Look: %s\n", portGraphicsLookName(portGraphicsLookNext()));
        portOptSave();
        return;
    }
    switch (which) {
    case PORT_GFX_KEY_TONE_MAPPING:
        g_PortGraphicsRender.tone_mapping = !g_PortGraphicsRender.tone_mapping;
        portConPrintf(1, "Tone Mapping: %s\n", onOff(g_PortGraphicsRender.tone_mapping));
        break;
    case PORT_GFX_KEY_BAKED_LIGHTING: {
        const char *status;
        bakedOn = !bakedOn;
        portBakedLightingChanged();
        status = portBakeStatusText();
        portConPrintf(1, "Baked Lighting: %s%s%s\n", onOff(bakedOn), status && bakedOn ? " - " : "",
                      status && bakedOn ? status : "");
        break;
    }
    case PORT_GFX_KEY_HDR_OUTPUT:
        g_PortGraphicsRender.hdr_output = !g_PortGraphicsRender.hdr_output;
        portConPrintf(1, "HDR Output: %s%s\n", onOff(g_PortGraphicsRender.hdr_output),
                      g_PortGraphicsRender.hdr_output ? hdrNote() : "");
        break;
    case PORT_GFX_KEY_VIEWMODEL:
        viewmodelOn = !viewmodelOn;
        portConPrintf(1, "Graphics on Gun and Hands: %s\n", onOff(viewmodelOn));
        break;
    case PORT_GFX_KEY_COMPARE: /* a review aid: not saved */
        g_PortGraphicsRender.compare = !g_PortGraphicsRender.compare;
        portConPrintf(1, "Compare (left stock, right graded): %s\n", onOff(g_PortGraphicsRender.compare));
        return;
    default:
        return;
    }
    portOptSave();
}

void portGraphicsTick(void)
{
    static s32 reported = PORT_HDR_OFF;
    const s32 state = g_PortGraphicsRender.hdr_output ? g_PortGraphicsRender.hdr_state : PORT_HDR_OFF;
    if (state != reported && (state == PORT_HDR_UNSUPPORTED || state == PORT_HDR_SDR_DISPLAY)) {
        portConPrintf(1, "HDR Output%s\n", hdrNote());
    }
    reported = state;
}

s32 portGraphicsGradesViewmodel(void)
{
    return viewmodelOn;
}

Gfx *portGraphicsGradeTag(Gfx *gdl)
{
    if (g_PortGraphicsRender.tone_mapping) {
        gDPNoOpTag(gdl++, 0x66000000u);
    }
    return gdl;
}

static u32 probeByte(f32 v)
{
    s32 b = (s32) (v * 128.0f + 0.5f); /* 128 = 1.0, so up to ~2x */
    return (u32) clampi(b, 0, 255);
}

Gfx *portGraphicsViewmodelTag(Gfx *gdl)
{
    f32 rgb[3];
    if (!viewmodelOn || !bakedOn) {
        return gdl;
    }
    portBakeViewmodelProbe(rgb);
    gDPNoOpTag(gdl++, 0x67000000u | (probeByte(rgb[0]) << 16) | (probeByte(rgb[1]) << 8) | probeByte(rgb[2]));
    return gdl;
}
