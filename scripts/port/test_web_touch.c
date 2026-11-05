/* Synthetic browser bridge regression: no ROM or renderer. */
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../../port/web/touch.c"

s32 g_portMouseDirect, g_PlayerIsInTank;
f32 g_portMouseYawDeg, g_portMousePitchDeg;
static s32 live, consoleOpen, playerNum, sprintSetting = 1;
static s32 stageRunning, watchOpen, valueEdit, pickList, bindCapture, assignWait, holdDrag, padOnly, oskOpen;
static char oskName[32];
static s32 oskFinish;
static u32 actionBits;
static s32 inventory[8], inventoryCount, inventoryReads;
static const char *const bindings[] = {
    "reload", "use", "aim_down_sights", "prev_weapon", "next_weapon",
    "crouch_toggle", "zoom_in", "zoom_out", "detonate", "walk",
    "prev_inventory", "next_inventory"
};
s32 get_cur_playernum(void) { return playerNum; }
s32 portBondControlsLive(void) { return live; }
s32 portStageRunning(void) { return stageRunning; }
s32 portWatchMenuUp(void) { return watchOpen; }
s32 portSheetValueEditOpen(void) { return valueEdit; }
s32 portMenuPickListOpen(void) { return pickList; }
s32 portBindCaptureActive(void) { return bindCapture; }
s32 portInputAssignActive(void) { return assignWait; }
s32 portMenuHoldDrag(void) { return holdDrag; }
s32 portMenuPadOnly(void) { return padOnly; }
s32 portOskWebOpen(void) { return oskOpen; }
s32 portOskWebChar(s32 field, s32 index) { return field ? oskName[index] : "SAVE SET AS"[index]; }
void portOskWebClear(void) { oskName[0] = 0; }
void portOskWebAppend(s32 code) { s32 n = 0; while (oskName[n]) n++; oskName[n] = (char) code; oskName[n + 1] = 0; }
void portOskWebFinish(s32 ok) { oskFinish = ok ? 1 : -1; }
s32 portWebControlsLive(void) { return live; }
s32 portConIsOpen(void) { return consoleOpen; }
s32 bondinvCountTotalItemsInInv(void) { inventoryReads++; return inventoryCount; }
s32 bondinvGetTextbyInvIndex(s32 index) { assert(index >= 0 && index < inventoryCount); return inventory[index]; }
s32 portJoyPortInvScrollItem(s32 item) { return item == ITEM_BUG || item == ITEM_CAMERA; }
s32 portJoyPortBindCount(void) { return sizeof(bindings) / sizeof(bindings[0]); }
const char *portJoyPortBindName(s32 index) { return bindings[index]; }
void portJoyPortPadDown(s32 pad, s32 action) { assert(pad == 0); actionBits |= 1u << action; }
s32 portJoyPortOptionGet(const char *key, s32 *out)
{
    if (strcmp(key, "sprint") != 0) return 0;
    *out = sprintSetting;
    return 1;
}
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
    valueEdit = 1;
    assert(portWebTouchContext() == WEB_TOUCH_VALUE_EDIT);
    valueEdit = 0;
    /* A menu set by holding A while the cursor moves reports its own context. */
    holdDrag = 1;
    assert(portWebTouchContext() == WEB_TOUCH_HOLD_DRAG);
    stageRunning = 1;
    assert(portWebTouchContext() == WEB_TOUCH_CINEMATIC);
    stageRunning = holdDrag = 0;
    padOnly = 1;
    assert(portWebTouchContext() == WEB_TOUCH_PAD);
    padOnly = 0;
    oskOpen = 1;
    assert(portWebTouchContext() == WEB_TOUCH_TEXT);
    oskOpen = 0;
    /* Options folder states that a tap cannot answer each report their own context. */
    pickList = 1;
    assert(portWebTouchContext() == WEB_TOUCH_PICK_LIST);
    assignWait = 1;
    assert(portWebTouchContext() == WEB_TOUCH_ASSIGN);
    bindCapture = 1;
    assert(portWebTouchContext() == WEB_TOUCH_BIND_CAPTURE);
    stageRunning = 1;
    assert(portWebTouchContext() == WEB_TOUCH_CINEMATIC); /* only outside a mission */
    stageRunning = pickList = assignWait = bindCapture = 0;
}

static void testValueEditSteps(void)
{
    /* Arrange: a slider edit is open and a fast drag queues three steps up. */
    OSContPad pad = {0};
    portWebTouchReset();
    valueEdit = 1;
    portWebTouchStep(3);
    /* Act / Assert: steps reach the edit directly, never as D-pad or stick input. */
    portWebTouchPoll(&pad);
    assert(pad.button == 0 && pad.stick_x == 0);
    assert(portWebTouchTakeSteps() == 3 && portWebTouchTakeSteps() == 0);
    /* Opposite steps cancel; a huge swipe is bounded. */
    portWebTouchStep(-2);
    portWebTouchStep(1);
    assert(portWebTouchTakeSteps() == -1);
    portWebTouchStep(1000);
    assert(steps == 40);
    /* Closing the edit drops queued steps so they cannot reach the next one. */
    valueEdit = 0;
    portWebTouchPoll(&pad);
    assert(pad.button == 0 && portWebTouchTakeSteps() == 0);
    /* An open pick list takes the same queue as rows to scroll. */
    pickList = 1;
    portWebTouchStep(-3);
    portWebTouchPoll(&pad);
    assert(pad.button == 0 && portWebTouchTakeSteps() == -3);
    pickList = 0;
    portWebTouchReset();
}

static void testPhoneText(void)
{
    /* Arrange: a name keyboard is open and touch is on. */
    oskOpen = 1;
    /* Act / Assert: the title reads a character at a time; the name is rewritten whole. */
    assert(portWebTextChar(0, 0) == 'S' && portWebTextChar(0, 4) == ' ');
    portWebTextClear();
    portWebTextAppend('O');
    portWebTextAppend('K');
    assert(strcmp(oskName, "OK") == 0 && portWebTextChar(1, 1) == 'K');
    portWebTextFinish(1);
    assert(oskFinish == 1);
    /* Nothing reaches a closed keyboard, and closed reads are empty. */
    oskOpen = 0;
    portWebTextAppend('X');
    portWebTextFinish(0);
    assert(strcmp(oskName, "OK") == 0 && oskFinish == 1 && portWebTextChar(0, 0) == 0);
}

static void testCaptureRequests(void)
{
    /* Arrange: a bind row waits for an input. */
    OSContPad pad = {0};
    portWebTouchReset();
    bindCapture = 1;
    /* Act / Assert: Unbind is one Delete for the Options folder, no pad bits. */
    portWebTouchUnbind();
    portWebTouchPoll(&pad);
    assert(pad.button == 0 && portWebTouchTakeUnbind() && !portWebTouchTakeUnbind());
    /* An Unbind left over when the capture ends is dropped, not banked. */
    portWebTouchUnbind();
    bindCapture = 0;
    portWebTouchPoll(&pad);
    assert(!portWebTouchTakeUnbind());
    /* Back still sends B, and Input Device's wait also sees it once. */
    assignWait = 1;
    portWebTouchButton(TOUCH_BACK, 1);
    portWebTouchPoll(&pad);
    assert(pad.button == B_BUTTON && portWebTouchTakeBack() && !portWebTouchTakeBack());
    portWebTouchButton(TOUCH_BACK, 0);
    /* Back pressed with no wait open never cancels a later one. */
    assignWait = 0;
    portWebTouchButton(TOUCH_BACK, 1);
    portWebTouchButton(TOUCH_BACK, 0);
    portWebTouchPoll(&pad);
    assignWait = 1;
    assert(!portWebTouchTakeBack());
    assignWait = 0;
    portWebTouchReset();
}

static void testAvailableGadgets(void)
{
    /* Arrange: ordinary weapons and non-usable mission inventory are not gadgets. */
    inventory[0] = ITEM_UNARMED;
    inventory[1] = ITEM_WPPK;
    inventory[2] = ITEM_KEYCARD;
    inventoryCount = 3;
    stageRunning = live = 1;
    watchOpen = playerNum = g_PlayerIsInTank = 0;
    /* Act / Assert: availability follows current inventory without a cached result. */
    assert(!portWebTouchHasGadgets());
    inventory[3] = ITEM_BUG;
    inventoryCount = 4;
    assert(portWebTouchHasGadgets());
    inventory[3] = ITEM_CAMERA;
    assert(portWebTouchHasGadgets());
    inventoryCount = 3;
    assert(!portWebTouchHasGadgets());
    inventoryCount = 0;
    assert(!portWebTouchHasGadgets());
    inventoryReads = 0;
    stageRunning = 0;
    assert(!portWebTouchHasGadgets() && !inventoryReads);
    stageRunning = 1; watchOpen = 1;
    assert(!portWebTouchHasGadgets() && !inventoryReads);
    watchOpen = 0; live = 0;
    assert(!portWebTouchHasGadgets() && !inventoryReads);
    live = 1; g_PlayerIsInTank = 1;
    assert(!portWebTouchHasGadgets() && !inventoryReads);
    g_PlayerIsInTank = 0; playerNum = -1;
    assert(!portWebTouchHasGadgets() && !inventoryReads);
    stageRunning = live = playerNum = 0;
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
            /* Only a long Left/Right hold reports a side, for the volume bars. */
            assert(portWebTouchHeldSide() == (tick + 2 > TOUCH_REPEAT_POLLS
                ? (directions[i] == TOUCH_RIGHT ? 1 : directions[i] == TOUCH_LEFT ? -1 : 0) : 0));
        }
        portWebTouchButton(directions[i], 0);
        portWebTouchPoll(&pad);
        assert(portWebTouchHeldSide() == 0);
        portWebTouchButton(directions[i], 1);
        portWebTouchButton(directions[i], 0);
        portWebTouchPoll(&pad);
        assert(pad.button == masks[i]);
    }
    watchOpen = 0;
    /* A pad-only menu takes each arrow as one D-pad press with no stick step. */
    padOnly = 1;
    for (i = 0; i < 4; i++) {
        OSContPad pad = {0};
        portWebTouchReset();
        portWebTouchButton(directions[i], 1);
        portWebTouchPoll(&pad);
        assert(pad.button == masks[i] && pad.stick_x == 0 && pad.stick_y == 0);
        portWebTouchButton(directions[i], 0);
    }
    padOnly = 0;
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

static void testStickSprint(void)
{
    /* Arrange: walk is bind 9; the over-dragged stick holds it like the Sprint button. */
    OSContPad pad = {0};
    const u32 walk = 1u << 9;
    portWebTouchStickSprint(1);
    actionBits = 0;
    portWebTouchPoll(&pad);
    assert(actionBits == walk);
    /* With Sprint off, holding Walk would slow the player, so the stick does not. */
    sprintSetting = 0;
    actionBits = 0;
    portWebTouchPoll(&pad);
    assert(actionBits == 0);
    /* The Sprint button still holds the bind it names. */
    portWebTouchButton(TOUCH_SPRINT, 1);
    portWebTouchPoll(&pad);
    assert(actionBits == walk);
    portWebTouchButton(TOUCH_SPRINT, 0);
    sprintSetting = 1;
    portWebTouchStickSprint(0);
    actionBits = 0;
    portWebTouchPoll(&pad);
    assert(actionBits == 0);
    /* Reset releases a stick sprint along with every other hold. */
    portWebTouchStickSprint(1);
    portWebTouchReset();
    portWebTouchPoll(&pad);
    assert(actionBits == 0);
}

int main(void)
{
    OSContPad pad = {0};
    struct MoveData move = {0};
    testTouchContext();
    assert(!portWebTouchEnabled());
    portWebTouchEnable(1);
    assert(portWebTouchEnabled());
    testNativeN64Buttons();
    testAvailableGadgets();
    testWatchDirections();
    testValueEditSteps();
    testCaptureRequests();
    testPhoneText();
    testStickSprint();
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
    puts("Touch bridge: semantic buttons, independent holds, analog movement, stick-overdrag sprint, menu sticks, slider and list steps, hold-drag, capture and text contexts, phone text, held volume arrows, reset, bounds and locked-look discard passed.");
    return 0;
}
