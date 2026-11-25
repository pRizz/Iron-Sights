/* XWayland pointer reset on focus (FORK-091); see port_xwayland_pointer.h. */
#include "port_xwayland_pointer.h"

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

/* SDL_strcmp, not strcmp: the game's include path puts the N64 SDK's string.h
 * first, which declares no strcmp (lp64_diagnostics.py unprototyped). */
int portXwaylandPointerNeedsReset(const char *videoDriver, const char *waylandDisplay, int cursorShown)
{
    if (videoDriver == NULL || SDL_strcmp(videoDriver, "x11") != 0)
    {
        return 0;
    }
    if (waylandDisplay == NULL || waylandDisplay[0] == '\0')
    {
        return 0;
    }
    return !cursorShown;
}

void portXwaylandPointerFocusGained(void)
{
    const int shown = SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE;

    if (!portXwaylandPointerNeedsReset(SDL_GetCurrentVideoDriver(), getenv("WAYLAND_DISPLAY"), shown))
    {
        return;
    }
    SDL_ShowCursor(SDL_ENABLE);
    SDL_ShowCursor(SDL_DISABLE);
    printf("ge: input: XWayland pointer reset on focus\n");
}
#else
typedef int portXwaylandPointerUnused; /* nothing to compile off Linux */
#endif
