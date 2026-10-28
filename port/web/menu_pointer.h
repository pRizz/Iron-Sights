#ifndef PORT_WEB_MENU_POINTER_H
#define PORT_WEB_MENU_POINTER_H

/* Browsers cannot warp an unlocked OS pointer. Keep physical samples separate
 * from a virtual menu cursor that keyboard/controller navigation may reseat. */
typedef struct {
    int x, y, valid, motion;
    unsigned buttons;
} PortWebMenuPointer;

static inline void portWebMenuPointerReset(PortWebMenuPointer *state)
{
    state->x = state->y = state->valid = state->motion = 0;
    state->buttons = 0;
}

static inline void portWebMenuPointerMotion(PortWebMenuPointer *state)
{
    state->motion = 1;
}

static inline int portWebMenuPointerRead(PortWebMenuPointer *state,
        int x, int y, unsigned buttons, int *virtualX, int *virtualY)
{
    const int moved = state->motion || (state->valid && (x != state->x || y != state->y))
        || (buttons & ~state->buttons) != 0;
    state->x = x;
    state->y = y;
    state->buttons = buttons;
    state->valid = 1;
    state->motion = 0;
    if (moved) {
        *virtualX = x;
        *virtualY = y;
    }
    return moved;
}

#endif
