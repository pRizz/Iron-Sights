/* Browser focus can disappear without SDL receiving the key/button releases. */
#include <SDL.h>
#include <ultra64.h>
#include <emscripten.h>
#include <emscripten/html5.h>

void portWebPointerRelease(void)
{
    EM_ASM({ if (typeof Module['onPointerRelease'] === 'function') Module['onPointerRelease'](); });
}

EMSCRIPTEN_KEEPALIVE f32 portWebFileMenuMetric(s32 metric)
{
    extern f32 frontWebFileMenuMetric(s32 metric);
    return frontWebFileMenuMetric(metric);
}

/* Yield between actual startup work items so the page can paint its progress. */
void portWebStartupProgress(const char *label, unsigned completed, unsigned total)
{
    EM_ASM({
        if (typeof Module['onStartupProgress'] === 'function')
            Module['onStartupProgress'](UTF8ToString($0), $1, $2);
    }, label, completed, total);
    emscripten_sleep(0);
}

/* A browser returns to its room instead of terminating the page's runtime. */
EMSCRIPTEN_KEEPALIVE void portWebReturnToRoom(void)
{
    extern void portWebSetPaused(s32 paused);
    portWebSetPaused(1);
    EM_ASM({ if (typeof Module['onRoomRequested'] === 'function') Module['onRoomRequested'](); });
}

/* The page sizes the framebuffer to its game stage (presentation.ts), so the
 * picture fills the window and follows rotation. The window is not resizable
 * in a browser (gfx_sdl2.cpp), so SDL never replaces this with a CSS size;
 * SDL_SetWindowSize sets the canvas and sends SIZE_CHANGED, which the renderer
 * already handles. Returns 0 when nothing was applied: no window yet, or the
 * first-start icon stage owns the frames; the page asks again later. */
EMSCRIPTEN_KEEPALIVE s32 portWebResizeFramebuffer(s32 width, s32 height)
{
    extern int portIconStageBusy(void);
    SDL_Window *window = SDL_GL_GetCurrentWindow();
    if (window == NULL || portIconStageBusy() || width < 1 || height < 1) return 0;
    SDL_SetWindowSize(window, width, height);
    return 1;
}

/* Internal SDL 2.32.10 contracts from the immutable Emscripten port. */
extern void SDL_ResetKeyboard(void);
extern void SDL_SetKeyboardFocus(SDL_Window *window);
extern void SDL_SetMouseFocus(SDL_Window *window);
extern int SDL_SendMouseButton(SDL_Window *window, Uint32 mouseID, Uint8 state, Uint8 button);
extern void portInputWebFocus(s32 paused);
extern void portAudioWebPause(s32 paused);

/* A page request alone does not update SDL's relative-mode bookkeeping. */
EMSCRIPTEN_KEEPALIVE s32 portWebCaptureMouse(void)
{
    SDL_Window *window = SDL_GL_GetCurrentWindow();
    if (!window) return -1;
    SDL_SetMouseFocus(window);
    return SDL_SetRelativeMouseMode(SDL_TRUE);
}

void portWebApplyFocus(s32 paused)
{
    extern void portWebTouchReset(void);
    SDL_Window *window = SDL_GL_GetCurrentWindow();
    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    Uint8 button;
    portWebTouchReset();
    SDL_ResetKeyboard();
    for (button = 1; button <= 32; button++) {
        if (buttons & (1u << (button-1))) SDL_SendMouseButton(window, 0, SDL_RELEASED, button);
    }
    SDL_GetRelativeMouseState(NULL, NULL);
    if (paused) SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_SetKeyboardFocus(paused ? NULL : window);
    portInputWebFocus(paused);
    portAudioWebPause(paused);
}
