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

enum { TOUCH_FIRE, TOUCH_AIM, TOUCH_USE, TOUCH_RELOAD, TOUCH_WATCH, TOUCH_ACCEPT, TOUCH_BACK, TOUCH_CROUCH, TOUCH_PREV_WEAPON,
    TOUCH_NEXT_WEAPON, TOUCH_PREV_GADGET, TOUCH_NEXT_GADGET, TOUCH_DETONATE, TOUCH_ZOOM_IN, TOUCH_ZOOM_OUT,
    TOUCH_UP, TOUCH_DOWN, TOUCH_LEFT, TOUCH_RIGHT, TOUCH_SPRINT, TOUCH_COUNT };
static s32 enabled;
static f32 moveX, moveY, yaw, pitch;
static u32 held, pressed;
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
    if (down) { pressed |= bit; held |= bit; }
    else held &= ~bit;
}

void portWebTouchPoll(OSContPad *pad)
{
    static const u16 buttons[TOUCH_COUNT] = {
        [TOUCH_FIRE] = Z_TRIG, [TOUCH_WATCH] = START_BUTTON, [TOUCH_ACCEPT] = A_BUTTON,
        [TOUCH_BACK] = B_BUTTON, [TOUCH_UP] = U_JPAD, [TOUCH_DOWN] = D_JPAD,
        [TOUCH_LEFT] = L_JPAD, [TOUCH_RIGHT] = R_JPAD
    };
    u32 down = held | pressed;
    s32 i, action;
    pressed = 0;
    if (!enabled || portConIsOpen()) return;
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
        if (down & (1u << TOUCH_LEFT)) x = -1;
        if (down & (1u << TOUCH_RIGHT)) x = 1;
        if (down & (1u << TOUCH_UP)) y = 1;
        if (down & (1u << TOUCH_DOWN)) y = -1;
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
        g_portMousePitchDeg += dy;
    }
}
