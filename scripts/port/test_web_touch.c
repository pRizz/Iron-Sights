/* Synthetic browser bridge regression: no ROM or renderer. */
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../../port/web/touch.c"

s32 g_portMouseDirect, g_PlayerIsInTank;
f32 g_portMouseYawDeg, g_portMousePitchDeg;
static s32 live, consoleOpen, playerNum;
static s32 stageRunning, watchOpen;
static u32 actionBits;
static const char *const bindings[] = {
    "reload", "use", "aim_down_sights", "prev_weapon", "next_weapon",
    "crouch_toggle", "zoom_in", "zoom_out", "detonate", "walk",
    "prev_inventory", "next_inventory"
};
s32 get_cur_playernum(void) { return playerNum; }
s32 portBondControlsLive(void) { return live; }
s32 portStageRunning(void) { return stageRunning; }
s32 portWatchMenuUp(void) { return watchOpen; }
s32 portWebControlsLive(void) { return live; }
s32 portConIsOpen(void) { return consoleOpen; }
s32 portJoyPortBindCount(void) { return sizeof(bindings) / sizeof(bindings[0]); }
const char *portJoyPortBindName(s32 index) { return bindings[index]; }
void portJoyPortPadDown(s32 pad, s32 action) { assert(pad == 0); actionBits |= 1u << action; }
void osSyncPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format); vfprintf(stderr, format, args); va_end(args);
    abort();
}

static void testTouchContext(void)
{
    /* Arrange/act/assert: stale gameplay/watch state cannot show a frontend HUD;
     * cutscenes stay hidden, actual permission reveals it, and watch stays usable. */
    for (stageRunning = 0; stageRunning <= 1; stageRunning++)
        for (watchOpen = 0; watchOpen <= 1; watchOpen++)
            for (live = 0; live <= 1; live++) {
                const s32 expected = !stageRunning ? WEB_TOUCH_NAVIGATION
                    : watchOpen ? WEB_TOUCH_WATCH : live ? WEB_TOUCH_GAME : WEB_TOUCH_CINEMATIC;
                assert(portWebTouchContext() == expected);
            }
    stageRunning = watchOpen = live = 0;
}

static void testWatchDirections(void)
{
    /* Arrange: all four directions, with a finger held across many polls. */
    const s32 directions[] = {TOUCH_UP, TOUCH_DOWN, TOUCH_LEFT, TOUCH_RIGHT};
    const u16 masks[] = {U_JPAD, D_JPAD, L_JPAD, R_JPAD};
    s32 i, tick;
    watchOpen = 1;
    for (i = 0; i < 4; i++) {
        OSContPad pad = {0};
        portWebTouchReset();
        /* Act / Assert: one digital edge, no second analog navigation source. */
        portWebTouchButton(directions[i], 1);
        portWebTouchPoll(&pad);
        assert(pad.button == masks[i] && pad.stick_x == 0 && pad.stick_y == 0);
        for (tick = 0; tick < 120; tick++) {
            pad.button = 0;
            portWebTouchButton(directions[i], 1);
            portWebTouchPoll(&pad);
            assert(pad.button == 0 && pad.stick_x == 0 && pad.stick_y == 0);
        }
        portWebTouchButton(directions[i], 0);
        portWebTouchButton(directions[i], 1);
        portWebTouchButton(directions[i], 0);
        portWebTouchPoll(&pad);
        assert(pad.button == masks[i]);
    }
    watchOpen = 0;
    portWebTouchReset();
}

static void testNativeN64Buttons(void)
{
    /* Arrange: both gameplay and native menus use the same physical buttons. */
    const s32 buttons[] = {TOUCH_ACCEPT, TOUCH_BACK, TOUCH_FIRE, TOUCH_WATCH};
    const u16 masks[] = {A_BUTTON, B_BUTTON, Z_TRIG, START_BUTTON};
    s32 mode, i;
    for (mode = 0; mode <= 1; mode++) {
        live = mode;
        for (i = 0; i < 4; i++) {
            OSContPad pad = {0};
            portWebTouchReset();
            actionBits = 0;
            /* Act: even a tap between native polls is preserved for one tick. */
            portWebTouchButton(buttons[i], 1);
            portWebTouchButton(buttons[i], 0);
            portWebTouchPoll(&pad);
            /* Assert: exact N64 bit, with no semantic binding substitution. */
            assert(pad.button == masks[i] && actionBits == 0);
            pad.button = 0;
            portWebTouchPoll(&pad);
            assert(pad.button == 0);
        }
    }
    live = 0;
    portWebTouchReset();
}

int main(void)
{
    OSContPad pad = {0};
    struct MoveData move = {0};
    testTouchContext();
    portWebTouchEnable(1);
    testNativeN64Buttons();
    testWatchDirections();
    portWebTouchButton(TOUCH_FIRE, 1);
    portWebTouchButton(TOUCH_AIM, 1);
    portWebTouchButton(TOUCH_RELOAD, 1);
    portWebTouchPoll(&pad);
    assert(pad.button & Z_TRIG);
    assert((actionBits & 5) == 5); /* Name mapping, independent of saved numeric IDs. */
    pad.button = 0; actionBits = 0;
    portWebTouchButton(TOUCH_FIRE, 0);
    portWebTouchPoll(&pad);
    assert(!(pad.button & Z_TRIG));
    assert(actionBits & 4); /* Releasing fire does not release ADS. */
    portWebTouchReset();
    pad.button = 0; actionBits = 0;
    portWebTouchPoll(&pad);
    assert(!pad.button && !actionBits);

    portWebTouchMove(0.5f, 1.0f);
    portWebTouchPoll(&pad);
    assert(pad.stick_x == 40 && pad.stick_y == 80);
    portWebTouchLook(10, -8);
    portWebTouchApplyMove(&move); /* Intro/watch discard pending look. */
    assert(g_portMouseYawDeg == 0 && yaw == 0);
    live = 1;
    portWebTouchLook(10, -8);
    portWebTouchApplyMove(&move);
    assert(move.analogWalk == 70 && move.analogStrafe == 35);
    assert(move.canLookAhead && move.canTurnTank);
    assert(g_portMouseYawDeg == 10 && g_portMousePitchDeg == 8 && g_portMouseDirect);
    portWebTouchApplyMove(&move);
    assert(g_portMouseYawDeg == 10); /* Once per simulation tick, no repeated motion. */
    move.invertPitch = 1;
    portWebTouchLook(0, -8);
    portWebTouchApplyMove(&move);
    assert(g_portMousePitchDeg == 0); /* Reverse flips the same swipe. */

    portWebTouchMove(__builtin_nanf(""), 999);
    assert(moveX == 0 && moveY == 1);
    portWebTouchLook(999, -999);
    portWebTouchLook(999, -999);
    assert(yaw == 45 && pitch == -45);
    portWebTouchEnable(0);
    assert(held == 0 && yaw == 0 && moveY == 0);
    portWebTouchButton(TOUCH_FIRE, 1);
    assert(held == 0);
    portWebTouchEnable(1);
    consoleOpen = 1;
    portWebTouchButton(TOUCH_FIRE, 1);
    pad.button = 0;
    portWebTouchPoll(&pad);
    assert(pad.button == 0);
    puts("Touch bridge: semantic buttons, independent holds, analog movement, menu sticks, reset, bounds and locked-look discard passed.");
    return 0;
}
