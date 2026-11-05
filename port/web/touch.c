/* Touch is a separate input source; it never synthesizes keyboard bindings. */
#include <math.h>
#include <str.h>
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <emscripten.h>
#include "touch.h"

extern s32 get_cur_playernum(void);
extern s32 portBondControlsLive(void);
extern s32 portConIsOpen(void);
extern s32 portJoyPortBindCount(void);
extern const char *portJoyPortBindName(s32 index);
extern void portJoyPortPadDown(s32 pad, s32 action);
extern s32 g_portMouseDirect;
extern f32 g_portMouseYawDeg, g_portMousePitchDeg;
extern s32 g_PlayerIsInTank;
extern s32 portStageRunning(void);
extern s32 portWebControlsLive(void);
extern s32 portWatchMenuUp(void);
extern s32 bondinvCountTotalItemsInInv(void);
extern s32 bondinvGetTextbyInvIndex(s32 index);
extern s32 portJoyPortInvScrollItem(s32 item);
extern s32 portSheetValueEditOpen(void);
extern s32 portSheetPickListOpen(void);
extern s32 portBindCaptureActive(void);
extern s32 portInputAssignActive(void);

/* Presentation follows real engine permission, never an intro-duration timer.
 * Keep the watch distinct so its native lists can still be closed/navigated. */
EMSCRIPTEN_KEEPALIVE s32 portWebTouchContext(void)
{
    if (!portStageRunning()) {
        /* Each Options folder state that a menu tap cannot answer gets its own controls. */
        if (portBindCaptureActive()) return WEB_TOUCH_BIND_CAPTURE;
        if (portInputAssignActive()) return WEB_TOUCH_ASSIGN;
        if (portSheetPickListOpen()) return WEB_TOUCH_PICK_LIST;
        return portSheetValueEditOpen() ? WEB_TOUCH_VALUE_EDIT : WEB_TOUCH_NAVIGATION;
    }
    if (portWatchMenuUp()) return WEB_TOUCH_WATCH;
    return portWebControlsLive() ? WEB_TOUCH_GAME : WEB_TOUCH_CINEMATIC;
}

/* Match next-inventory's usable-item whitelist, not every watch inventory row. */
EMSCRIPTEN_KEEPALIVE s32 portWebTouchHasGadgets(void)
{
    s32 i, count;
    if (portWebTouchContext() != WEB_TOUCH_GAME || get_cur_playernum() != 0 || g_PlayerIsInTank) return 0;
    count = bondinvCountTotalItemsInInv();
    for (i = 0; i < count; i++) {
        if (portJoyPortInvScrollItem(bondinvGetTextbyInvIndex(i))) return 1;
    }
    return 0;
}

enum { TOUCH_FIRE, TOUCH_AIM, TOUCH_USE, TOUCH_RELOAD, TOUCH_WATCH, TOUCH_ACCEPT, TOUCH_BACK, TOUCH_CROUCH, TOUCH_PREV_WEAPON,
    TOUCH_NEXT_WEAPON, TOUCH_PREV_GADGET, TOUCH_NEXT_GADGET, TOUCH_DETONATE, TOUCH_ZOOM_IN, TOUCH_ZOOM_OUT,
    TOUCH_UP, TOUCH_DOWN, TOUCH_LEFT, TOUCH_RIGHT, TOUCH_SPRINT, TOUCH_COUNT };
static s32 enabled;
static f32 moveX, moveY, yaw, pitch;
static u32 held, pressed;
static s32 steps;
static s32 unbind, back;
/* A watch Left/Right held this many polls (about 0.3 s) moves a volume bar like a held D-pad. */
#define TOUCH_REPEAT_POLLS 18
static s32 sidePolls;
static const char *const actions[TOUCH_COUNT] = {
    [TOUCH_AIM] = "aim_down_sights", [TOUCH_USE] = "use", [TOUCH_RELOAD] = "reload",
    [TOUCH_CROUCH] = "crouch_toggle", [TOUCH_PREV_WEAPON] = "prev_weapon",
    [TOUCH_NEXT_WEAPON] = "next_weapon", [TOUCH_PREV_GADGET] = "prev_inventory",
    [TOUCH_NEXT_GADGET] = "next_inventory", [TOUCH_DETONATE] = "detonate",
    [TOUCH_ZOOM_IN] = "zoom_in", [TOUCH_ZOOM_OUT] = "zoom_out", [TOUCH_SPRINT] = "walk"
};

EMSCRIPTEN_KEEPALIVE void portWebTouchReset(void)
{
    held = pressed = 0;
    steps = unbind = back = sidePolls = 0;
    moveX = moveY = yaw = pitch = 0;
}

EMSCRIPTEN_KEEPALIVE void portWebTouchEnable(s32 on)
{
    on = on != 0;
    if (on != enabled) portWebTouchReset();
    enabled = on;
}

static f32 bounded(f32 value, f32 limit)
{
    if (!__builtin_isfinite(value)) return 0;
    return value < -limit ? -limit : value > limit ? limit : value;
}

EMSCRIPTEN_KEEPALIVE void portWebTouchMove(f32 x, f32 y)
{
    if (!enabled) return;
    moveX = bounded(x, 1);
    moveY = bounded(y, 1);
}

EMSCRIPTEN_KEEPALIVE void portWebTouchLook(f32 x, f32 y)
{
    if (!enabled) return;
    /* Bound pending motion after a delayed frame; never bank a long swipe. */
    yaw = bounded(yaw + bounded(x, 30), 45);
    pitch = bounded(pitch + bounded(y, 30), 45);
}

EMSCRIPTEN_KEEPALIVE void portWebTouchButton(s32 action, s32 down)
{
    u32 bit;
    if (!enabled || action < 0 || action >= TOUCH_COUNT) return;
    bit = 1u << action;
    if (down) {
        if (!(held & bit)) {
            pressed |= bit;
            if (action == TOUCH_BACK) back = 1;
        }
        held |= bit;
    }
    else held &= ~bit;
}

/* Slider value steps (negative = lower) from the -/+ buttons or a sideways drag.
 * Bounded so a long swipe cannot bank a run of steps after the finger stops. */
EMSCRIPTEN_KEEPALIVE void portWebTouchStep(s32 count)
{
    if (!enabled) return;
    steps = (s32)bounded((f32)steps + (f32)count, 40);
}

/* The Options folder's open slider edit takes the queued steps each tick
 * (src/game/front.c) and clamps them at the scale's ends; the D-pad wraps.
 * An open pick list takes them as rows to scroll instead. */
s32 portWebTouchTakeSteps(void)
{
    const s32 taken = steps;
    steps = 0;
    return taken;
}

/* +1 / -1 while the watch's Right / Left has been held past a tap, else 0. Only
 * the volume bars read it (src/game/options.c): elsewhere a held arrow stays one
 * step, as watch lists and pages need. */
s32 portWebTouchHeldSide(void)
{
    if (!enabled || sidePolls <= TOUCH_REPEAT_POLLS) return 0;
    if (held & (1u << TOUCH_RIGHT)) return 1;
    return (held & (1u << TOUCH_LEFT)) ? -1 : 0;
}

/* The Unbind button: the Options folder reads it as its Delete key. */
EMSCRIPTEN_KEEPALIVE void portWebTouchUnbind(void)
{
    if (enabled) unbind = 1;
}

s32 portWebTouchTakeUnbind(void)
{
    const s32 taken = unbind;
    unbind = 0;
    return taken;
}

/* Back pressed since the last take; Input Device's wait cancels on it because
 * it listens to real devices only and never sees the touch B. */
s32 portWebTouchTakeBack(void)
{
    const s32 taken = back;
    back = 0;
    return taken;
}

void portWebTouchPoll(OSContPad *pad)
{
    static const u16 buttons[TOUCH_COUNT] = {
        [TOUCH_FIRE] = Z_TRIG, [TOUCH_WATCH] = START_BUTTON, [TOUCH_ACCEPT] = A_BUTTON,
        [TOUCH_BACK] = B_BUTTON, [TOUCH_UP] = U_JPAD, [TOUCH_DOWN] = D_JPAD,
        [TOUCH_LEFT] = L_JPAD, [TOUCH_RIGHT] = R_JPAD
    };
    /* Watch arrows are one digital step per contact, never an analog hold. */
    const u32 directions = (1u << TOUCH_UP) | (1u << TOUCH_DOWN)
        | (1u << TOUCH_LEFT) | (1u << TOUCH_RIGHT);
    u32 down = (held & ~directions) | pressed;
    s32 i, action;
    sidePolls = (held & ((1u << TOUCH_LEFT) | (1u << TOUCH_RIGHT))) ? sidePolls + 1 : 0;
    pressed = 0;
    if (!enabled || portConIsOpen()) return;
    /* Steps and requests belong to the screen they were made on; never carry them on. */
    if (!portSheetValueEditOpen() && !portSheetPickListOpen()) steps = 0;
    if (!portBindCaptureActive()) unbind = 0;
    if (!portInputAssignActive()) back = 0;
    for (i = 0; i < TOUCH_COUNT; i++) {
        if (!(down & (1u << i))) continue;
        pad->button |= buttons[i];
        if (!actions[i]) continue;
        for (action = 0; action < portJoyPortBindCount(); action++) {
            if (strcmp(actions[i], portJoyPortBindName(action)) == 0) {
                portJoyPortPadDown(0, action);
                break;
            }
        }
    }
    if (!portBondControlsLive() || g_PlayerIsInTank) {
        /* Native menus include both cursor-driven folders and D-pad watch lists. */
        f32 x = moveX, y = moveY;
        if (!portWatchMenuUp()) {
            if (down & (1u << TOUCH_LEFT)) x = -1;
            if (down & (1u << TOUCH_RIGHT)) x = 1;
            if (down & (1u << TOUCH_UP)) y = 1;
            if (down & (1u << TOUCH_DOWN)) y = -1;
        }
        if (x != 0) pad->stick_x = (s8)(x * 80);
        if (y != 0) pad->stick_y = (s8)(y * 80);
    }
}

void portWebTouchApplyMove(struct MoveData *move)
{
    f32 dx, dy;
    if (get_cur_playernum() != 0) return;
    dx = yaw; dy = pitch;
    yaw = pitch = 0;
    if (!enabled || !portBondControlsLive() || portConIsOpen()) return;
    if (!g_PlayerIsInTank && (moveX != 0 || moveY != 0)) {
        move->analogWalk = moveY * 70;
        move->analogStrafe = moveX * 70;
        move->analogTurn = move->analogPitch = 0;
        move->digitalStepForward = move->digitalStepBack = 0;
        move->digitalStepLeft = move->digitalStepRight = 0;
        move->canLookAhead = move->canTurnTank = 1;
        move->rLeanLeft = move->rLeanRight = 0;
        move->crouchUp = move->crouchDown = 0;
    }
    if (dx != 0 || dy != 0) {
        g_portMouseDirect = 1;
        g_portMouseYawDeg += dx;
        /* Same screen-Y convention and Upright/Reverse policy as mouse look. */
        g_portMousePitchDeg += move->invertPitch ? dy : -dy;
    }
}
