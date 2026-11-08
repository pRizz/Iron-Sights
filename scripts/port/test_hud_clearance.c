/* ROM-free checks for reporting corner ammo to browser touch controls (FORK-028). */
#include <assert.h>
#include <stdio.h>
#include "port/fast3d/hud_touch_clearance.h"

static float absf(float value) { return value < 0 ? -value : value; }

int main(void)
{
    PortHudClearBox ammo = {0};
    float out[4];
    /* Icon and counts make one box. */
    ammo = portHudClearUnion(ammo, .55f, .62f, -.92f, -.82f);
    ammo = portHudClearUnion(ammo, .50f, .70f, -.90f, -.84f);
    assert(ammo.valid && ammo.x0 == .50f && ammo.x1 == .70f && ammo.y0 == -.92f && ammo.y1 == -.82f);
    /* Canvas fractions: x from the left, y from the top. */
    assert(portHudClearToCanvas(ammo, 1024.f, 0.f, 1024.f, out));
    assert(absf(out[0] - .75f) < 1e-6f && absf(out[1] - .85f) < 1e-6f);
    assert(absf(out[2] - .91f) < 1e-6f && absf(out[3] - .96f) < 1e-6f);
    {   /* a pillarboxed frame sits viewX into a wider canvas */
        const PortHudClearBox right = {1.f, 1.f, -1.f, 1.f, 1};
        assert(portHudClearToCanvas(right, 1600.f, 160.f, 1280.f, out));
        assert(absf(out[0] - 1440.f / 1600.f) < 1e-6f && out[2] == 0.f && out[3] == 1.f);
    }
    {   /* nothing drawn, or no canvas */
        const PortHudClearBox none = {0};
        assert(!portHudClearToCanvas(none, 1024.f, 0.f, 1024.f, out));
        assert(!portHudClearToCanvas(ammo, 0.f, 0.f, 1024.f, out));
    }
    /* the drop keeps the row's lowest icon (about 11 native pixels up) on screen */
    assert(PORT_HUD_CLEAR_DROP > 0.f && PORT_HUD_CLEAR_DROP < 11.f);
    assert(portHudClearSide(PORT_HUD_CLEAR_RIGHT_AMMO) == PORT_HUD_CLEAR_RIGHT_SIDE);
    assert(portHudClearSide(PORT_HUD_CLEAR_LEFT_AMMO) == PORT_HUD_CLEAR_LEFT_SIDE);
    assert(portHudClearSide(PORT_HUD_CLEAR_GADGET) == PORT_HUD_CLEAR_LEFT_SIDE);
    puts("HUD touch clearance checks passed: element box union, canvas fractions, pillarbox, empty box, drop range.");
    return 0;
}
