/* Included by test_ads.c: the Graphics page (FORK-082) - its settings and
 * their clamps, the hotkeys (toggle, console message, save; Compare is not
 * saved), and the display-list tags the renderer acts on. The bake itself is
 * tested apart (test_baked_trace.c); its level hooks are stubbed here. */
#include "../../port/src/port_graphics.c"
#include "../../port/src/port_graphics_looks.c"

static s32 bakeChanges;
static s32 iconExport;
s32 portWeaponExportEnabled(void) { return iconExport; }
static f32 bakeProbe[3] = { 1.0f, 1.0f, 1.0f };
void portBakedLightingChanged(void) { bakeChanges++; }
const char *portBakeStatusText(void) { return NULL; }
void portBakeTick(void) {}
void portBakeViewmodelProbe(f32 out_rgb[3])
{
    out_rgb[0] = bakeProbe[0];
    out_rgb[1] = bakeProbe[1];
    out_rgb[2] = bakeProbe[2];
}

static void testGraphicsDefaults(void)
{
    static const struct { const char *key; s32 value; } defaults[] = {
        { "tone_mapping", PORT_TONE_MAPPING_DEFAULT }, { "vibrance", PORT_VIBRANCE_DEFAULT },
        { "hdr_output", PORT_HDR_OUTPUT_DEFAULT }, { "hdr_peak", PORT_HDR_PEAK_DEFAULT },
        { "hdr_paper_white", PORT_HDR_PAPER_DEFAULT }, { "baked_lighting", PORT_BAKED_LIGHTING_DEFAULT },
        { "baked_lighting_strength", PORT_BAKED_STRENGTH_DEFAULT },
        { "graphics_viewmodel", PORT_GRAPHICS_VIEWMODEL_DEFAULT },
        { "tone_brightness", PORT_BRIGHTNESS_DEFAULT }, { "tone_contrast", PORT_CONTRAST_DEFAULT },
        { "tone_intensity", PORT_INTENSITY_DEFAULT }, { "tone_clarity", PORT_CLARITY_DEFAULT },
        { "tone_sharpen", PORT_SHARPEN_DEFAULT }, { "tone_glow", PORT_GLOW_DEFAULT },
        { "tone_saturation", PORT_SATURATION_DEFAULT }, { "tone_temperature", 0 }, { "tone_split", 0 },
        { "tone_split_colours", 0 }, { "tone_fade", 0 }, { "tone_vignette", 0 }, { "tone_grain", 0 },
        { "tone_auto_exposure", PORT_AUTO_EXPOSURE_DEFAULT }, { "tone_adapt_speed", PORT_ADAPT_SPEED_DEFAULT },
        { "graphics_look", PORT_LOOK_DEFAULT },
    };
    s32 i, v;
    for (i = 0; i < (s32) (sizeof(defaults) / sizeof(defaults[0])); i++) {
        v = -1;
        assert(portJoyPortOptionGet(defaults[i].key, &v) && v == defaults[i].value);
    }
    /* on by default on the desktop builds, with the Vibrant Look; HDR Output stays opt-in */
    assert(PORT_TONE_MAPPING_DEFAULT == 1 && PORT_BAKED_LIGHTING_DEFAULT == 1 && PORT_HDR_OUTPUT_DEFAULT == 0);
    assert(portGraphicsLook() == PORT_LOOK_VIBRANT);
    assert(portOptIsToggle(portOptFind("tone_mapping")) && portOptIsToggle(portOptFind("baked_lighting")));
    assert(portOptIsRanged(portOptFind("vibrance")) && portOptIsRanged(portOptFind("baked_lighting_strength")));
    assert(portOptIsRanged(portOptFind("tone_brightness")) && portOptIsRanged(portOptFind("tone_glow")));
    /* capture runs (GE_WINDOW) start with Tone Mapping and Baked Lighting off */
    {
        extern int setenv(const char *, const char *, int);
        extern int unsetenv(const char *);
        setenv("GE_WINDOW", "960x540", 1);
        portGraphicsCaptureDefaults();
        assert(!g_PortGraphicsRender.tone_mapping && !portBakedLightingOn());
        unsetenv("GE_WINDOW");
        assert(portJoyPortOptionSet("tone_mapping", PORT_TONE_MAPPING_DEFAULT));
        assert(portJoyPortOptionSet("baked_lighting", PORT_BAKED_LIGHTING_DEFAULT));
        portGraphicsCaptureDefaults(); /* no GE_WINDOW: nothing changes */
        assert(g_PortGraphicsRender.tone_mapping && portBakedLightingOn());
    }
}

static void testGraphicsClamps(void)
{
    s32 v;
    assert(portJoyPortOptionSet("vibrance", 150) && portJoyPortOptionGet("vibrance", &v) && v == 100);
    assert(portJoyPortOptionSet("hdr_peak", 100) && portJoyPortOptionGet("hdr_peak", &v) && v == 400);
    assert(portJoyPortOptionSet("hdr_paper_white", 1000) && portJoyPortOptionGet("hdr_paper_white", &v) && v == 480);
    bakeChanges = 0;
    assert(portJoyPortOptionSet("baked_lighting_strength", 5));
    assert(portJoyPortOptionGet("baked_lighting_strength", &v) && v == 25 && bakeChanges == 1);
    assert(portJoyPortSetOption("vibrance", "30") && g_PortGraphicsRender.vibrance == 30); /* midas.ini */
    assert(portJoyPortOptionSet("tone_brightness", 10) && portJoyPortOptionGet("tone_brightness", &v) && v == 50);
    assert(portJoyPortOptionSet("tone_contrast", 900) && g_PortGraphicsRender.contrast == 200);
    assert(portJoyPortOptionSet("tone_glow", -5) && g_PortGraphicsRender.glow == 0);
    assert(portJoyPortSetOption("tone_sharpen", "60") && g_PortGraphicsRender.sharpen == 60);
    assert(portJoyPortOptionSet("vibrance", PORT_VIBRANCE_DEFAULT));
    assert(portJoyPortOptionSet("tone_brightness", PORT_BRIGHTNESS_DEFAULT));
    assert(portJoyPortOptionSet("tone_contrast", PORT_CONTRAST_DEFAULT));
    assert(portJoyPortOptionSet("tone_glow", PORT_GLOW_DEFAULT));
    assert(portJoyPortOptionSet("tone_sharpen", PORT_SHARPEN_DEFAULT));
    assert(portJoyPortOptionSet("hdr_peak", PORT_HDR_PEAK_DEFAULT));
    assert(portJoyPortOptionSet("hdr_paper_white", PORT_HDR_PAPER_DEFAULT));
    assert(portJoyPortOptionSet("baked_lighting_strength", PORT_BAKED_STRENGTH_DEFAULT));
}

static void testGraphicsHotkeys(void)
{
    s32 v;
    /* joy_port.c hands the five actions over in PORT_GFX_KEY order */
    assert(PORTACT_TOGGLE_TONE_MAPPING + PORT_GFX_KEY_BAKED_LIGHTING == PORTACT_TOGGLE_BAKED_LIGHTING);
    assert(PORTACT_TOGGLE_TONE_MAPPING + PORT_GFX_KEY_HDR_OUTPUT == PORTACT_TOGGLE_HDR_OUTPUT);
    assert(PORTACT_TOGGLE_TONE_MAPPING + PORT_GFX_KEY_VIEWMODEL == PORTACT_TOGGLE_GRAPHICS_VIEWMODEL);
    assert(PORTACT_TOGGLE_TONE_MAPPING + PORT_GFX_KEY_COMPARE == PORTACT_GRAPHICS_COMPARE);
    assert(PORTACT_TOGGLE_TONE_MAPPING + PORT_GFX_KEY_SLIDER_DOWN == PORTACT_GRAPHICS_SLIDER_DOWN);
    assert(strcmp(portJoyPortBindDefault(PORTACT_GRAPHICS_SLIDER_NEXT), "Home") == 0);
    assert(strcmp(portJoyPortBindDefault(PORTACT_GRAPHICS_SLIDER_UP), "PageUp") == 0);
    assert(strcmp(portJoyPortBindDefault(PORTACT_TOGGLE_TONE_MAPPING), "F3") == 0);
    assert(strcmp(portJoyPortBindDefault(PORTACT_TOGGLE_BAKED_LIGHTING), "F4") == 0);
    assert(strcmp(portJoyPortBindDefault(PORTACT_TOGGLE_HDR_OUTPUT), "F2") == 0);
    assert(strcmp(portJoyPortBindDefault(PORTACT_TOGGLE_GRAPHICS_VIEWMODEL), "F7") == 0);
    assert(strcmp(portJoyPortBindDefault(PORTACT_GRAPHICS_COMPARE), "F11") == 0);

    noclipPopups = 0;
    portGraphicsHotkey(PORT_GFX_KEY_TONE_MAPPING); /* on by default: off, then on */
    assert(portJoyPortOptionGet("tone_mapping", &v) && v == 0 && noclipPopups == 1);
    portGraphicsHotkey(PORT_GFX_KEY_TONE_MAPPING);
    assert(portJoyPortOptionGet("tone_mapping", &v) && v == 1 && noclipPopups == 2);
    bakeChanges = 0;
    portGraphicsHotkey(PORT_GFX_KEY_BAKED_LIGHTING);
    assert(!portBakedLightingOn() && bakeChanges == 1);
    portGraphicsHotkey(PORT_GFX_KEY_BAKED_LIGHTING);
    assert(portBakedLightingOn() && bakeChanges == 2);
    portGraphicsHotkey(PORT_GFX_KEY_VIEWMODEL);
    assert(!portGraphicsGradesViewmodel());
    portGraphicsHotkey(PORT_GFX_KEY_VIEWMODEL);
    assert(portGraphicsGradesViewmodel());
    portGraphicsHotkey(PORT_GFX_KEY_COMPARE);
    assert(g_PortGraphicsRender.compare == 1);
    portGraphicsHotkey(PORT_GFX_KEY_COMPARE);
    assert(g_PortGraphicsRender.compare == 0 && noclipPopups == 8);

    /* HDR Output's state from the renderer: reported once when it cannot run */
    portGraphicsHotkey(PORT_GFX_KEY_HDR_OUTPUT);
    g_PortGraphicsRender.hdr_state = PORT_HDR_SDR_DISPLAY;
    noclipPopups = 0;
    portGraphicsTick();
    portGraphicsTick();
    assert(noclipPopups == 1);
    portGraphicsHotkey(PORT_GFX_KEY_HDR_OUTPUT);
    g_PortGraphicsRender.hdr_state = PORT_HDR_OFF;
    portGraphicsTick();
}

/* Home / End pick a slider, Page Up / Page Down step it, clamped, not wrapped */
static void testGraphicsSliderKeys(void)
{
    s32 v, i;
    noclipPopups = 0;
    portGraphicsHotkey(PORT_GFX_KEY_SLIDER_UP); /* starts on Brightness, 150 by default */
    assert(portJoyPortOptionGet("tone_brightness", &v) && v == 155 && noclipPopups == 1);
    for (i = 0; i < 40; i++) portGraphicsHotkey(PORT_GFX_KEY_SLIDER_UP);
    assert(portJoyPortOptionGet("tone_brightness", &v) && v == 200); /* the limit holds */
    portGraphicsHotkey(PORT_GFX_KEY_SLIDER_NEXT);                    /* Contrast */
    portGraphicsHotkey(PORT_GFX_KEY_SLIDER_DOWN);
    assert(portJoyPortOptionGet("tone_contrast", &v) && v == 85);
    portGraphicsHotkey(PORT_GFX_KEY_SLIDER_PREV);                    /* back to Brightness */
    portGraphicsHotkey(PORT_GFX_KEY_SLIDER_PREV);                    /* wraps to the last slider */
    portGraphicsHotkey(PORT_GFX_KEY_SLIDER_NEXT);                    /* and to the first again */
    portGraphicsHotkey(PORT_GFX_KEY_SLIDER_DOWN);
    assert(portJoyPortOptionGet("tone_brightness", &v) && v == 195);
    for (i = 0; i < 16; i++) portGraphicsHotkey(PORT_GFX_KEY_SLIDER_NEXT); /* Lighting Strength */
    bakeChanges = 0;
    portGraphicsHotkey(PORT_GFX_KEY_SLIDER_DOWN);
    assert(portJoyPortOptionGet("baked_lighting_strength", &v) && v == 75 && bakeChanges == 1);
    assert(portJoyPortOptionSet("tone_brightness", PORT_BRIGHTNESS_DEFAULT));
    assert(portJoyPortOptionSet("tone_contrast", PORT_CONTRAST_DEFAULT));
    assert(portJoyPortOptionSet("baked_lighting_strength", PORT_BAKED_STRENGTH_DEFAULT));
    for (i = 0; i < 2; i++) portGraphicsHotkey(PORT_GFX_KEY_SLIDER_NEXT); /* leave the cursor anywhere */
}

/* Looks: a preset sets every grading slider; touching one makes it Custom */
static void testGraphicsLooks(void)
{
    s32 v;
    assert(portGraphicsLook() == PORT_LOOK_VIBRANT); /* the defaults are Vibrant */
    assert(strcmp(portGraphicsLookName(PORT_LOOK_COLD_WAR), "Cold War") == 0);
    assert(portJoyPortOptionSet("graphics_look", PORT_LOOK_NOIR));
    assert(portJoyPortOptionGet("tone_saturation", &v) && v == 0);
    assert(portJoyPortOptionGet("tone_contrast", &v) && v == 130);
    assert(portJoyPortOptionGet("graphics_look", &v) && v == PORT_LOOK_NOIR);
    assert(portJoyPortOptionSet("tone_mapping", 0) && portJoyPortOptionSet("graphics_look", PORT_LOOK_MUTED));
    assert(!g_PortGraphicsRender.tone_mapping); /* the row and midas.ini leave Tone Mapping alone */
    assert(portJoyPortOptionSet("graphics_look", PORT_LOOK_NOIR));
    assert(portJoyPortOptionSet("tone_grain", 40));
    assert(portJoyPortOptionGet("graphics_look", &v) && v == PORT_LOOK_CUSTOM);
    assert(portJoyPortOptionGet("tone_saturation", &v) && v == 0); /* Custom keeps the values */
    assert(portJoyPortOptionSet("tone_grain", 35));                /* back to Noir's values */
    assert(portGraphicsLook() == PORT_LOOK_NOIR);
    /* the owner's own settings read as Vibrant, whatever order they load in */
    assert(portJoyPortOptionSet("graphics_look", PORT_LOOK_CUSTOM));
    assert(portJoyPortSetOption("tone_glow", "100") && portJoyPortSetOption("vibrance", "80"));
    assert(portJoyPortSetOption("tone_brightness", "150") && portJoyPortSetOption("tone_contrast", "90"));
    assert(portJoyPortSetOption("tone_clarity", "60") && portJoyPortSetOption("tone_sharpen", "50"));
    assert(portJoyPortSetOption("tone_intensity", "50") && portJoyPortSetOption("tone_saturation", "100"));
    assert(portJoyPortSetOption("tone_fade", "0") && portJoyPortSetOption("tone_vignette", "0"));
    assert(portJoyPortSetOption("tone_grain", "0") && portJoyPortSetOption("tone_adapt_speed", "100"));
    assert(portGraphicsLook() == PORT_LOOK_CUSTOM); /* Noir's Auto Exposure (40) still differs */
    assert(portJoyPortSetOption("tone_auto_exposure", "60"));
    assert(portGraphicsLook() == PORT_LOOK_VIBRANT);
    assert(portJoyPortOptionSet("tone_adapt_speed", 400) && g_PortGraphicsRender.adapt_speed == 400);
    assert(portJoyPortOptionSet("tone_adapt_speed", 1) && g_PortGraphicsRender.adapt_speed == 25); /* clamped */
    assert(portJoyPortOptionSet("tone_adapt_speed", PORT_ADAPT_SPEED_DEFAULT) && portGraphicsLook() == PORT_LOOK_VIBRANT);
    /* Insert: the next Look, Tone Mapping on, saved */
    noclipPopups = 0;
    portGraphicsHotkey(PORT_GFX_KEY_LOOK_NEXT);
    assert(portGraphicsLook() == PORT_LOOK_CINEMATIC && g_PortGraphicsRender.tone_mapping && noclipPopups == 1);
    assert(g_PortGraphicsRender.split == 40 && g_PortGraphicsRender.vignette == 30);
    assert(portJoyPortOptionSet("graphics_look", PORT_LOOK_DARK));
    portGraphicsHotkey(PORT_GFX_KEY_LOOK_NEXT); /* wraps past Custom to the first preset */
    assert(portGraphicsLook() == PORT_LOOK_NATURAL);
    assert(portJoyPortOptionSet("tone_mapping", PORT_TONE_MAPPING_DEFAULT));
}

static void testGraphicsTags(void)
{
    Gfx list[4];
    Gfx *end;

    memset(list, 0, sizeof(list));
    assert(portJoyPortOptionSet("tone_mapping", 0) && portJoyPortOptionSet("baked_lighting", 0));
    end = portGraphicsGradeTag(list);
    assert(end == list); /* Tone Mapping off: no tag */
    assert(portJoyPortOptionSet("tone_mapping", 1));
    end = portGraphicsGradeTag(list);
    assert(end == list + 1 && (list[0].words.w0 >> 24) == (u8) G_NOOP && list[0].words.w1 == 0x66000000u);

    end = portGraphicsViewmodelTag(list);
    assert(end == list); /* Baked Lighting off: the viewmodel keeps its colours */
    assert(portJoyPortOptionSet("baked_lighting", 1));
    bakeProbe[0] = 0.5f;
    bakeProbe[1] = 1.0f;
    bakeProbe[2] = 2.5f; /* clamps at 255 = 1.99x */
    end = portGraphicsViewmodelTag(list);
    assert(end == list + 1 && list[0].words.w1 == 0x674080FFu);
    iconExport = 1; /* the weapon icon export captures stock pixels */
    assert(portGraphicsViewmodelTag(list) == list && portGraphicsGradeTag(list) == list);
    iconExport = 0;
    assert(portJoyPortOptionSet("graphics_viewmodel", 0));
    assert(portGraphicsViewmodelTag(list) == list);

    assert(portJoyPortOptionSet("graphics_viewmodel", PORT_GRAPHICS_VIEWMODEL_DEFAULT));
    assert(portJoyPortOptionSet("baked_lighting", PORT_BAKED_LIGHTING_DEFAULT));
    assert(portJoyPortOptionSet("tone_mapping", PORT_TONE_MAPPING_DEFAULT));
}

static void testGraphics(void)
{
    testGraphicsDefaults();
    testGraphicsClamps();
    testGraphicsHotkeys();
    testGraphicsSliderKeys();
    testGraphicsLooks();
    testGraphicsTags();
}
