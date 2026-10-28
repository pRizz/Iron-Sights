#include <assert.h>
#include <stdio.h>
#include "../../port/web/menu_pointer.h"

static void absoluteHandoff(void)
{
    /* Arrange: the native menu snapped to an item, but the OS pointer did not. */
    PortWebMenuPointer state = {0};
    int x = 600, y = 400;
    assert(!portWebMenuPointerRead(&state, 100, 100, 0, &x, &y));
    assert(x == 600 && y == 400); /* stationary mouse leaves the controller alone */

    /* Act: one small physical motion, with no traversal to a window edge. */
    assert(portWebMenuPointerRead(&state, 105, 104, 0, &x, &y));

    /* Assert: physical coordinates replace the snap instead of adding deltas. */
    assert(x == 105 && y == 104);
    for (int i = 0; i < 50; i++) {
        x = 700; y = 300; /* repeated menu changes or controller snaps */
        assert(portWebMenuPointerRead(&state, 106 + i, 105 + i, 0, &x, &y));
        assert(x == 106 + i && y == 105 + i);
    }
}

static void stationaryClick(void)
{
    /* Arrange: keyboard/controller moved the native cursor under a still mouse. */
    PortWebMenuPointer state = {0};
    int x = 200, y = 100;
    portWebMenuPointerRead(&state, 40, 80, 0, &x, &y);

    /* Act/Assert: a fresh click selects the physical target on that first click. */
    assert(portWebMenuPointerRead(&state, 40, 80, 1, &x, &y));
    assert(x == 40 && y == 80);
    x = 300; y = 200;
    assert(!portWebMenuPointerRead(&state, 40, 80, 1, &x, &y));
    assert(x == 300 && y == 200); /* holding is not another ownership handoff */
    assert(!portWebMenuPointerRead(&state, 40, 80, 0, &x, &y));
    assert(portWebMenuPointerRead(&state, 40, 80, 4, &x, &y));
    assert(x == 40 && y == 80);
}

static void focusAndBounds(void)
{
    /* Arrange: focus/capture changes invalidate the old physical sample. */
    PortWebMenuPointer state = {0};
    int x = 100, y = 100;
    portWebMenuPointerRead(&state, 100, 100, 0, &x, &y);
    portWebMenuPointerReset(&state);

    /* Act/Assert: reseed without moving a controller cursor; next move is exact. */
    x = 800; y = 600;
    assert(!portWebMenuPointerRead(&state, 100, 100, 0, &x, &y));
    assert(x == 800 && y == 600);
    assert(portWebMenuPointerRead(&state, -5, 900, 0, &x, &y));
    assert(x == -5 && y == 900); /* viewport/menu clamping is the caller's job */
    assert(portWebMenuPointerRead(&state, 500, 300, 0, &x, &y));
    assert(x == 500 && y == 300); /* reentry does not keep a clamp offset */

    /* First actual motion may arrive before the first poll after focus reset. */
    portWebMenuPointerReset(&state);
    portWebMenuPointerMotion(&state);
    assert(portWebMenuPointerRead(&state, 200, 150, 0, &x, &y));
    assert(x == 200 && y == 150);
    x = 700; y = 400;
    portWebMenuPointerMotion(&state);
    assert(portWebMenuPointerRead(&state, 200, 150, 0, &x, &y));
    assert(x == 200 && y == 150);
}

int main(void)
{
    absoluteHandoff();
    stationaryClick();
    focusAndBounds();
    puts("Web menu pointer: absolute handoff, repeated snaps, stationary clicks, focus reset and reentry passed.");
    return 0;
}
