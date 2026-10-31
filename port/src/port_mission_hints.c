/* Short desktop onboarding, using the same live bindings as the watch UI. */
#include "port_mission_hints.h"
#include <str.h>
static s32 hintsEnabled = 1, hintsShown, hintWasShown;
static f32 hintRemaining;
static void hintPersist(void);

s32 portMissionHintsOptionGet(const char *key, s32 *value)
{
    if (!strcmp(key, "mission_hints")) { *value = hintsEnabled; return 1; }
    if (!strcmp(key, "mission_hints_shown")) { *value = hintsShown; return 1; }
    return 0;
}

s32 portMissionHintsOptionSet(const char *key, s32 value)
{
    if (!strcmp(key, "mission_hints")) {
        const s32 reenabled = value && !hintsEnabled;
        hintsEnabled = value != 0;
        if (!hintsEnabled) hintRemaining = 0;
        if (reenabled) { hintsShown = 0; hintPersist(); }
        return 1;
    }
    if (!strcmp(key, "mission_hints_shown")) {
        hintsShown = value < 0 ? 0 : value > 3 ? 3 : value;
        if (hintsShown == 3) { hintsEnabled = 0; hintRemaining = 0; }
        return 1;
    }
    return 0;
}

#if defined(GE_VIDEO) && !defined(__EMSCRIPTEN__) && !defined(GE_XBOX)
#include <stdio.h>
#include <string.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <fr.h>
#include "boss.h"
#include "game/lv.h"
#include "game/player.h"
#include "game/options.h"
#include "game/textrelated.h"
#include "port_hud.h"
#include "port/fast3d/hud_text_offset.h"

extern s32 portBondControlsLive(void);
extern s32 portVideoIniGetKey(const char *key, char *buf, s32 size);
extern s32 portInputP1Device(void);
extern s32 portInputSlotDevice(s32 slot);
extern s32 portInputDeviceModeIsN64(s32 device);
extern s32 portInputDeviceBindText(s32 device, const char *name, char *buf, s32 size);
extern s32 portJoyPortOptionGet(const char *key, s32 *value);

/* Presentation/counts stay outside suspend images and are never rolled back. */
static void hintPersist(void)
{
    extern s32 portIniWriteStrs(const char *, const char *const *, const char *const *, s32);
    const char *keys[] = { "mission_hints", "mission_hints_shown" };
    char count[8];
    const char *values[] = { hintsEnabled ? "1" : "0", count };
    snprintf(count, sizeof(count), "%d", hintsShown);
    portIniWriteStrs("[display]", keys, values, 2);
}

static s32 hintsPermitted(void)
{
    return g_CurrentPlayer && getPlayerCount() == 1 && get_cur_playernum() == 0
        && bossGetStageNum() != LEVELID_TITLE && portBondControlsLive()
        && !g_CurrentPlayer->bonddead && !g_CurrentPlayer->hudmessoff
        && !g_CurrentPlayer->mpmenuon;
}

void portMissionHintsReset(void)
{
    hintRemaining = hintsEnabled && hintsShown < 3 ? 480.0f : 0;
    hintWasShown = 0;
}
void portMissionHintsDismiss(void) { hintRemaining = 0; }

void portMissionHintsTick(void)
{
    if (!hintsEnabled || hintRemaining <= 0 || !hintsPermitted() || g_GlobalTimerDelta <= 0) return;
    hintRemaining = hintRemaining > g_GlobalTimerDelta ? hintRemaining - g_GlobalTimerDelta : 0;
    if (hintRemaining == 0 && hintWasShown) {
        hintsShown++;
        if (hintsShown >= 3) hintsEnabled = 0;
        hintPersist();
    }
}

static s32 maybeHintBinding(const char *action, char *text, s32 size)
{
    const s32 owner = portInputP1Device();
    const s32 maybeDevice = owner > 0 ? portInputSlotDevice(owner - 1) : -1;
    text[0] = '\0';
    if (maybeDevice > 0) {
        /* N64-style controls do not consume these direct port actions. */
        if (portInputDeviceModeIsN64(maybeDevice)
            || !portInputDeviceBindText(maybeDevice, action, text, size)) return 0;
    } else {
        s32 i;
        if (!portVideoIniGetKey(action, text, size)) return 0;
        /* Alternatives are comma-separated; show one real key to keep it short.
         * Literal comma keys use the bridge's unambiguous "Comma" label. */
        for (i = 0; text[i]; i++) if (text[i] == ',') { text[i] = '\0'; break; }
    }
    return text[0] && strcmp(text, "(unbound)") && strcmp(text, "unbound");
}

static const char *hintBindingName(const char *binding)
{
    static const struct { const char *stored, *display; } names[] = {
        { "Mouse Left", "Left Mouse Button" },
        { "Mouse Right", "Right Mouse Button" },
        { "Mouse Middle", "Middle Mouse Button" },
        { "Mouse X1", "Mouse Button 4" },
        { "Mouse X2", "Mouse Button 5" },
    };
    s32 i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (!strcmp(binding, names[i].stored)) return names[i].display;
    return binding;
}

static void hintSmallCaps(char *text)
{
    s32 i;
    for (i = 0; text[i]; i++) {
        if ((u8)text[i] >= 0x80) { if (text[i + 1]) i++; }
        else if (text[i] >= 'A' && text[i] <= 'Z') text[i] += 'a' - 'A';
    }
}

Gfx *portMissionHintsDraw(Gfx *gdl)
{
    char rows[2][160], binding[96];
    s32 i, count = 0, width, x, y, aimMode = 0;
    f32 fade;
    u32 alpha;
    if (!hintsEnabled || hintRemaining <= 0 || !hintsPermitted()) return gdl;
    if (maybeHintBinding("next_inventory", binding, sizeof(binding)))
        snprintf(rows[count++], sizeof(rows[0]), "Tap %s to change gadgets", hintBindingName(binding));
    portJoyPortOptionGet("aim_mode", &aimMode);
    if (aimMode && maybeHintBinding("r", binding, sizeof(binding)))
        snprintf(rows[count++], sizeof(rows[0]), "%s %s to aim down sights",
            cur_player_get_aim_control() ? "Tap" : "Hold", hintBindingName(binding));
    else if (maybeHintBinding("aim_down_sights", binding, sizeof(binding)))
        snprintf(rows[count++], sizeof(rows[0]), "Hold %s to aim down sights", hintBindingName(binding));
    if (!count) return gdl;
    hintWasShown = 1;
    fade = hintRemaining < 120.0f ? hintRemaining / 120.0f : 1.0f;
    fade = fade * fade * (3.0f - 2.0f * fade);
    alpha = (u32)(255.0f * fade + 0.5f);
    width = viGetViewWidth() - 72;
    if (width > 252) width = 252;
    if (width <= 16) return gdl;
    /* At mid-height the health arc's inner edge is near x=50 in its authored
     * 320-wide band. A small inset keeps text inside it. Follow the health
     * slide, independently of the ammo/timer HUD centering preference. */
    x = viGetViewLeft() + 56;
    y = viGetViewTop() + viGetViewHeight() / 2 - (count - 1) * 6 - 4;
    PORT_HUD_ANCHOR_FULL(gdl, PORT_HUD_ANCHOR_LEFT | 0x01000000u
        | ((u32)(portHealthSlide() * 255.0f + 0.5f) << 16));
    for (i = 0; i < count; i++) {
        hintSmallCaps(rows[i]);
        /* One framebuffer-pixel shadow keeps bare text legible without a panel. */
        gDPNoOpTag(gdl++, portHudTextOffsetTag(7, 1));
        gdl = portHudLabel(gdl, rows[i], x, y + i * 12, 0.6f, alpha, width);
        gDPNoOpTag(gdl++, portHudTextOffsetTag(6, 0));
        gdl = portHudLabel(gdl, rows[i], x, y + i * 12, 0.6f, 0xe1e5ea00 | alpha, width);
    }
    gDPNoOpTag(gdl++, portHudTextOffsetTag(0, 0));
    PORT_HUD_ANCHOR_OFF(gdl);
    return combiner_bayer_lod_perspective(gdl);
}
#else
static void hintPersist(void) {}
void portMissionHintsReset(void) {}
void portMissionHintsDismiss(void) {}
void portMissionHintsTick(void) {}
Gfx *portMissionHintsDraw(Gfx *gdl) { return gdl; }
#endif
