/* ROM-free lifecycle and live binding checks for desktop onboarding. */
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../../port/src/port_mission_hints.c"

static struct player player;
struct player *g_CurrentPlayer = &player;
f32 g_GlobalTimerDelta = 1;
static s32 live = 1, players = 1, owner, n64, lines, seat;
static LEVELID stage = LEVELID_DAM;
static const char *gadget = "Q", *ads = "Mouse Right", *dedicatedAds = "";
static s32 aimMode = 1, aimToggle;
static char shown[2][160];
static u32 textAlpha;
static f32 healthSlide = 1;
static s32 persistedEnabled, persistedShown, writes;
s32 portIniWriteStrs(const char *section, const char *const *keys, const char *const *values, s32 count)
{
    assert(!strcmp(section, "[display]") && count == 2);
    assert(!strcmp(keys[0], "mission_hints") && !strcmp(keys[1], "mission_hints_shown"));
    persistedEnabled = atoi(values[0]); persistedShown = atoi(values[1]); writes++;
    return 1;
}
f32 portHealthSlide(void) { return healthSlide; }
void osSyncPrintf(const char *fmt, ...)
{
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args); abort();
}
s32 portBondControlsLive(void) { return live; }
s32 getPlayerCount(void) { return players; }
s32 get_cur_playernum(void) { return seat; }
LEVELID bossGetStageNum(void) { return stage; }
u32 cur_player_get_aim_control(void) { return aimToggle; }
s32 portJoyPortOptionGet(const char *key, s32 *value) { assert(!strcmp(key, "aim_mode")); *value = aimMode; return 1; }
s32 portInputP1Device(void) { return owner; }
s32 portInputSlotDevice(s32 slot) { assert(slot == 0); return 1; }
s32 portInputDeviceModeIsN64(s32 device) { assert(device == 1); return n64; }
s32 portVideoIniGetKey(const char *action, char *buf, s32 size)
{
    snprintf(buf, size, "%s", !strcmp(action, "next_inventory") ? gadget : !strcmp(action, "r") ? ads : dedicatedAds);
    return 1;
}
s32 portInputDeviceBindText(s32 device, const char *action, char *buf, s32 size)
{
    assert(device == 1);
    return portVideoIniGetKey(action, buf, size);
}
s16 viGetViewLeft(void) { return 0; }
s16 viGetViewTop(void) { return 0; }
s16 viGetViewWidth(void) { return 320; }
s16 viGetViewHeight(void) { return 240; }
Gfx *portHudPanel(Gfx *g, f32 x, f32 y, f32 w, f32 h, u32 color)
{ assert(!"Mission hints must not draw a panel"); return g; }
Gfx *portHudLabel(Gfx *g, const char *text, s32 x, s32 y, f32 scale, u32 color, s32 width)
{
    assert(x == 56 && y >= 110 && y <= 122 && x + width <= 320 && scale == .65f);
    if (color >> 8) { assert(lines < 2); snprintf(shown[lines++], sizeof(shown[0]), "%s", text); textAlpha = color & 255; }
    return g;
}
Gfx *combiner_bayer_lod_perspective(Gfx *g) { return g; }

static s32 draw(void)
{
    Gfx list[32];
    lines = 0; portMissionHintsDraw(list);
    if (lines) {
        assert((list[0].words.w1 & 0x30) == PORT_HUD_ANCHOR_LEFT);
        assert(((list[0].words.w1 >> 16) & 255) == (u32)(healthSlide * 255.0f + 0.5f));
    }
    return lines;
}

static void testControlTime(void)
{
    /* Arrange: opening camera/control lock lasts longer than the hint window. */
    s32 i;
    portMissionHintsReset(); live = 0; g_GlobalTimerDelta = 600;
    /* Act / assert: locked time is excluded; pauses do not restart the window. */
    portMissionHintsTick(); assert(draw() == 0);
    live = 1; assert(draw() == 2 && textAlpha == 255);
    g_GlobalTimerDelta = 360; portMissionHintsTick(); assert(draw() == 2 && textAlpha == 255);
    live = 0; portMissionHintsTick(); live = 1;
    g_GlobalTimerDelta = 60; portMissionHintsTick(); assert(draw() == 2 && textAlpha >= 127 && textAlpha <= 128);
    portMissionHintsTick(); assert(draw() == 0);
    live = 0; portMissionHintsTick(); live = 1; assert(draw() == 0);
    /* Eight controllable seconds at each supported simulation step. */
    for (i = 1; i <= 8; i *= 2) {
        s32 ticks;
        portMissionHintsOptionSet("mission_hints_shown", 0);
        portMissionHintsOptionSet("mission_hints", 1);
        g_GlobalTimerDelta = 2.0f / i; portMissionHintsReset();
        for (ticks = 0; ticks < 240 * i - 1; ticks++) portMissionHintsTick();
        assert(draw() == 2);
        portMissionHintsTick(); assert(draw() == 0);
    }
}

static void testLiveBindings(void)
{
    /* Arrange: custom alternatives and a mouse button, then change them live. */
    portMissionHintsReset(); gadget = "G,Q"; ads = "Mouse X2";
    /* Act / assert: labels come from loaded bindings, not default strings. */
    assert(draw() == 2 && !strcmp(shown[0], "tap g to change gadgets"));
    assert(!strcmp(shown[1], "hold mouse button 5 to aim down sights"));
    gadget = "Comma"; assert(draw() == 2 && !strcmp(shown[0], "tap comma to change gadgets"));
    gadget = "(unbound)"; assert(draw() == 1 && !strcmp(shown[0], "hold mouse button 5 to aim down sights"));
    ads = ""; assert(draw() == 0);
    owner = 1; gadget = "Right Shoulder"; ads = "Left Trigger";
    assert(draw() == 2 && !strcmp(shown[0], "tap right shoulder to change gadgets"));
    assert(!strcmp(shown[1], "hold left trigger to aim down sights"));
    n64 = 1; assert(draw() == 0); n64 = 0;
    owner = 0; gadget = "Q"; ads = "Mouse Right";
    aimToggle = 1; assert(draw() == 2 && !strcmp(shown[1], "tap right mouse button to aim down sights")); aimToggle = 0;
    aimMode = 0; assert(draw() == 1); dedicatedAds = "Mouse X1";
    assert(draw() == 2 && !strcmp(shown[1], "hold mouse button 4 to aim down sights"));
    dedicatedAds = ""; aimMode = 1;
    healthSlide = 0; assert(draw() == 2);
    healthSlide = .5f; assert(draw() == 2); healthSlide = 1;
}

static void testLifecycle(void)
{
    /* Arrange: a fresh mission, with UI and player visibility changes. */
    portMissionHintsReset(); g_GlobalTimerDelta = 600;
    /* Act / assert: hidden HUD, death, frontend and multiplayer never consume hints. */
    player.hudmessoff = 1; portMissionHintsTick(); assert(draw() == 0); player.hudmessoff = 0;
    player.mpmenuon = 1; portMissionHintsTick(); assert(draw() == 0); player.mpmenuon = 0;
    player.bonddead = 1; portMissionHintsTick(); assert(draw() == 0); player.bonddead = 0;
    stage = LEVELID_TITLE; portMissionHintsTick(); assert(draw() == 0); stage = LEVELID_DAM;
    players = 2; portMissionHintsTick(); assert(draw() == 0); players = 1;
    seat = 1; portMissionHintsTick(); assert(draw() == 0); seat = 0;
    assert(draw() == 2 && textAlpha == 255);
    /* Restoring a suspend point dismisses onboarding; only a mission reset rearms it. */
    portMissionHintsDismiss(); assert(draw() == 0);
    portMissionHintsTick(); assert(draw() == 0);
    portMissionHintsReset(); assert(draw() == 2);
}

static void testQuota(void)
{
    /* Arrange: reset onboarding, including its persisted quota. */
    s32 i, value;
    portMissionHintsOptionSet("mission_hints_shown", 0);
    portMissionHintsOptionSet("mission_hints", 1);
    writes = 0; g_GlobalTimerDelta = 480;
    /* Act / assert: only complete, actually rendered windows consume the quota. */
    portMissionHintsReset(); portMissionHintsTick(); assert(writes == 0);
    portMissionHintsReset(); assert(draw() == 2); portMissionHintsDismiss();
    portMissionHintsTick(); assert(writes == 0);
    for (i = 1; i <= 3; i++) {
        portMissionHintsReset(); assert(draw() == 2);
        portMissionHintsTick(); assert(draw() == 0);
        assert(writes == i && persistedShown == i && persistedEnabled == (i < 3));
    }
    portMissionHintsReset(); assert(draw() == 0);
    assert(portMissionHintsOptionGet("mission_hints", &value) && value == 0);
    portMissionHintsOptionSet("mission_hints", 1);
    assert(persistedShown == 0 && persistedEnabled == 1);
    assert(draw() == 0); /* An explicit re-enable applies to the next mission. */
    portMissionHintsReset(); assert(draw() == 2);
    portMissionHintsOptionSet("mission_hints", 0); assert(draw() == 0);
    portMissionHintsOptionSet("mission_hints_shown", 99);
    assert(portMissionHintsOptionGet("mission_hints_shown", &value) && value == 3);
    portMissionHintsOptionSet("mission_hints_shown", -1);
    assert(portMissionHintsOptionGet("mission_hints_shown", &value) && value == 0);
    assert(!portMissionHintsOptionGet("unknown", &value));
    assert(!portMissionHintsOptionSet("unknown", 1));
}

int main(void)
{
    testControlTime(); testLiveBindings(); testLifecycle(); testQuota();
    puts("Mission hints passed: unlocked-time duration/fade, live remaps, mouse/pad labels, unbound actions, stage/menu/death/multiplayer/restore gates.");
    return 0;
}
