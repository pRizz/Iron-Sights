/* ROM-free tests of the XWayland pointer reset (FORK-091,
 * port/src/port_xwayland_pointer.c): when regaining focus resets the cursor,
 * and that a reset shows, then hides, the system cursor and leaves it hidden.
 * Built by scripts/port/test_xwayland_pointer.sh against a stand-in SDL.h
 * that declares the two calls the module makes; this file defines them and
 * records each call. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "port_xwayland_pointer.h"

static int s_failures;
static int s_cursorShown;
static const char *s_driver;
static int s_calls[8];
static int s_callCount;

static void expect(int condition, const char *what)
{
    if (!condition)
    {
        printf("FAIL: %s\n", what);
        s_failures++;
    }
}

int SDL_ShowCursor(int toggle)
{
    if (toggle == SDL_QUERY)
    {
        return s_cursorShown ? SDL_ENABLE : SDL_DISABLE;
    }
    if (s_callCount < (int) (sizeof(s_calls) / sizeof(s_calls[0])))
    {
        s_calls[s_callCount] = toggle;
    }
    s_callCount++;
    s_cursorShown = (toggle == SDL_ENABLE);
    return toggle;
}

const char *SDL_GetCurrentVideoDriver(void)
{
    return s_driver;
}

typedef struct ResetCase
{
    const char *name;
    const char *driver;
    const char *wayland; /* NULL: WAYLAND_DISPLAY unset */
    int cursorShown;
    int wantReset;
} ResetCase;

static const ResetCase kCases[] = {
    {"XWayland, cursor hidden", "x11", "wayland-0", 0, 1},
    {"XWayland, cursor shown", "x11", "wayland-0", 1, 0},
    {"plain X11 (no Wayland display)", "x11", NULL, 0, 0},
    {"empty Wayland display", "x11", "", 0, 0},
    {"SDL's Wayland driver", "wayland", "wayland-0", 0, 0},
    {"offscreen driver", "offscreen", "wayland-0", 0, 0},
    {"no video driver", NULL, "wayland-0", 0, 0},
};

static void runCase(const ResetCase *c)
{
    char what[160];

    snprintf(what, sizeof(what), "%s: policy", c->name);
    expect(portXwaylandPointerNeedsReset(c->driver, c->wayland, c->cursorShown) == c->wantReset, what);

    s_driver = c->driver;
    s_cursorShown = c->cursorShown;
    s_callCount = 0;
    if (c->wayland != NULL)
    {
        setenv("WAYLAND_DISPLAY", c->wayland, 1);
    }
    else
    {
        unsetenv("WAYLAND_DISPLAY");
    }
    portXwaylandPointerFocusGained();

    if (c->wantReset)
    {
        snprintf(what, sizeof(what), "%s: shows, then hides, the cursor", c->name);
        expect(s_callCount == 2 && s_calls[0] == SDL_ENABLE && s_calls[1] == SDL_DISABLE, what);
    }
    else
    {
        snprintf(what, sizeof(what), "%s: leaves the cursor alone", c->name);
        expect(s_callCount == 0, what);
    }
    snprintf(what, sizeof(what), "%s: cursor ends as it began", c->name);
    expect(s_cursorShown == c->cursorShown, what);
}

int main(void)
{
    size_t i;

    for (i = 0; i < sizeof(kCases) / sizeof(kCases[0]); i++)
    {
        runCase(&kCases[i]);
    }
    if (s_failures != 0)
    {
        printf("test_xwayland_pointer: %d failure(s)\n", s_failures);
        return 1;
    }
    printf("test_xwayland_pointer: %zu cases passed\n", i);
    return 0;
}
