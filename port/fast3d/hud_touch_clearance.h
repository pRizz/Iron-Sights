#ifndef PORT_HUD_TOUCH_CLEARANCE_H
#define PORT_HUD_TOUCH_CLEARANCE_H

/* G_NOOP tag: brackets a bottom-corner HUD element (gunfire.c via
 * portHudTouchClearTag). While browser touch controls cover that corner the
 * renderer draws it a little lower and reports where it drew, so the controls
 * can rise above it (port/web/touch.c). Slot 0 ends the bracket. */
#define PORT_HUD_CLEAR_TAG 0x5f000000u
enum {
    PORT_HUD_CLEAR_NONE,
    PORT_HUD_CLEAR_RIGHT_AMMO, /* right-hand ammo icon and counts */
    PORT_HUD_CLEAR_LEFT_AMMO,  /* dual-wield left-hand ammo */
    PORT_HUD_CLEAR_GADGET,     /* gadget name in the left-hand ammo slot */
    PORT_HUD_CLEAR_SLOTS
};
enum { PORT_HUD_CLEAR_LEFT_SIDE, PORT_HUD_CLEAR_RIGHT_SIDE, PORT_HUD_CLEAR_SIDES };
/* Native pixels the bracketed elements drop under touch controls (half that at
 * HUD Scale): the row's lowest icon then ends about 5 pixels above the bottom,
 * so the controls rise less. */
#define PORT_HUD_CLEAR_DROP 6.0f

/* An axis-aligned box in normalized device coordinates, y up. */
typedef struct { float x0, x1, y0, y1; int valid; } PortHudClearBox;

static inline int portHudClearSide(int slot)
{
    return slot == PORT_HUD_CLEAR_RIGHT_AMMO ? PORT_HUD_CLEAR_RIGHT_SIDE : PORT_HUD_CLEAR_LEFT_SIDE;
}

static inline PortHudClearBox portHudClearUnion(PortHudClearBox box, float x0, float x1, float y0, float y1)
{
    if (!box.valid) {
        const PortHudClearBox first = {x0, x1, y0, y1, 1};
        return first;
    }
    box.x0 = x0 < box.x0 ? x0 : box.x0;
    box.x1 = x1 > box.x1 ? x1 : box.x1;
    box.y0 = y0 < box.y0 ? y0 : box.y0;
    box.y1 = y1 > box.y1 ? y1 : box.y1;
    return box;
}

/* The box as canvas fractions: out = left, right (from the canvas's left edge),
 * top, bottom (from its top). A pillarbox narrows the drawn frame to viewWidth
 * pixels, viewX into a canvas realWidth wide. 0 when there is no box. */
static inline int portHudClearToCanvas(PortHudClearBox box, float realWidth, float viewX, float viewWidth,
                                       float out[4])
{
    if (!box.valid || !(realWidth > 0)) return 0;
    out[0] = ((box.x0 + 1.0f) * 0.5f * viewWidth + viewX) / realWidth;
    out[1] = ((box.x1 + 1.0f) * 0.5f * viewWidth + viewX) / realWidth;
    out[2] = (1.0f - box.y1) * 0.5f;
    out[3] = (1.0f - box.y0) * 0.5f;
    return 1;
}
#endif
