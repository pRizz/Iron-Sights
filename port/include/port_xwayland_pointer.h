/**
 * XWayland pointer reset on focus (FORK-091).
 *
 * The menus warp the real pointer to re-seat their cursor (input_pc.c,
 * mouseWarpFree) while the system cursor is hidden. XWayland emulates a warp
 * under a hidden cursor with a Wayland pointer lock. Under WSLg, after focus
 * leaves the window and returns, that emulation stays on without receiving
 * any motion, so the window gets clicks and keys but no pointer motion until
 * the cursor is shown again. Showing the cursor ends the emulation; hiding it
 * again restores the game's look.
 *
 * Linux native builds only: elsewhere the module compiles to nothing and the
 * game makes no call.
 */
#ifndef PORT_XWAYLAND_POINTER_H
#define PORT_XWAYLAND_POINTER_H

/** 1 when regaining focus should reset the cursor: SDL's X11 driver, a
 *  Wayland display (so X11 is XWayland) and the system cursor hidden. */
int portXwaylandPointerNeedsReset(const char *videoDriver, const char *waylandDisplay, int cursorShown);

/** Called on SDL_WINDOWEVENT_FOCUS_GAINED: shows, then hides, the system
 *  cursor when portXwaylandPointerNeedsReset says so. */
void portXwaylandPointerFocusGained(void);

/** The same reset, called after the game leaves relative mode (FORK-092):
 *  SDL then warps the hidden cursor back, which XWayland turns into a lock
 *  that left the menus without pointer motion after a mission under WSLg. */
void portXwaylandPointerReleased(void);

#endif
