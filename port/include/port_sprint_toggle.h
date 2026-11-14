/* Sprint (Toggle) (FORK-006): the click-to-sprint latch behind the
 * sprint_toggle bind, which the shipped controller layouts put on the left
 * stick click. joy_port.c owns the per-pad state and calls the step once per
 * player tick from portJoyPortMoveScale; this header is only the rule.
 *
 * A press while not sprinting arms the latch. Moving at or above
 * PORT_SPRINT_TOGGLE_MOVING (the length of the player's forward / sideways
 * speed, before any scaling) turns an armed latch into a sprint. An armed latch
 * that sees no movement within PORT_SPRINT_TOGGLE_ARM_WINDOW (60 Hz frames)
 * lapses, so a click made standing still does not wait forever. A sprint ends
 * when movement drops under the threshold, on a second press, or whenever
 * sprinting is blocked (Sprint off, controls not live, noclip, aiming, ADS or
 * crouching). A press while blocked does nothing. */
#ifndef PORT_SPRINT_TOGGLE_H
#define PORT_SPRINT_TOGGLE_H

#include <ultra64.h>

#define PORT_SPRINT_TOGGLE_MOVING 0.3f
#define PORT_SPRINT_TOGGLE_ARM_WINDOW 30.0f

enum {
    PORT_SPRINT_TOGGLE_OFF,
    PORT_SPRINT_TOGGLE_ARMED,
    PORT_SPRINT_TOGGLE_ON
};

typedef struct {
    s32 state;    /* PORT_SPRINT_TOGGLE_* */
    f32 armedFor; /* 60 Hz frames spent armed */
} PortSprintToggle;

/** One player tick. `forwards` / `sideways` are the voluntary move speeds
 * (-1..1), `dt` the tick length in 60 Hz frames. Returns 1 while the latch
 * sprints. */
static inline s32 portSprintToggleStep(PortSprintToggle *t, s32 pressed, f32 forwards, f32 sideways,
                                       s32 blocked, f32 dt)
{
    const s32 moving = forwards * forwards + sideways * sideways
                       >= PORT_SPRINT_TOGGLE_MOVING * PORT_SPRINT_TOGGLE_MOVING;

    if (blocked) {
        t->state = PORT_SPRINT_TOGGLE_OFF;
        return 0;
    }
    if (pressed) {
        t->state = (t->state == PORT_SPRINT_TOGGLE_OFF) ? PORT_SPRINT_TOGGLE_ARMED : PORT_SPRINT_TOGGLE_OFF;
        t->armedFor = 0.0f;
    }
    if (t->state == PORT_SPRINT_TOGGLE_ARMED) {
        if (moving) {
            t->state = PORT_SPRINT_TOGGLE_ON;
        } else {
            t->armedFor += dt;
            if (t->armedFor >= PORT_SPRINT_TOGGLE_ARM_WINDOW) t->state = PORT_SPRINT_TOGGLE_OFF;
        }
    } else if (t->state == PORT_SPRINT_TOGGLE_ON && !moving) {
        t->state = PORT_SPRINT_TOGGLE_OFF;
    }
    return t->state == PORT_SPRINT_TOGGLE_ON;
}

#endif
