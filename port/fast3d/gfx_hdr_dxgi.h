/* FORK-082: HDR Output's DXGI presenter (gfx_hdr_dxgi.cpp); stubs off Windows. */
#ifndef GFX_HDR_DXGI_H
#define GFX_HDR_DXGI_H

#include <stdint.h>
#include <stdbool.h>

struct SDL_Window;

#ifdef __cplusplus
extern "C" {
#endif

/* create or resize the scRGB swap chain for the window and share a w x h float
 * texture with GL texture name gl_texture; false = unavailable */
bool gfx_hdr_dxgi_target(struct SDL_Window* window, uint32_t w, uint32_t h, unsigned gl_texture);
/* bracket GL drawing into the shared texture; present copies it to the back buffer */
bool gfx_hdr_dxgi_lock(void);
bool gfx_hdr_dxgi_present(int vsync);
/* release the swap chain (GL presents the window again) */
void gfx_hdr_dxgi_shutdown(void);
/* a hard failure this session (or no presenter on this platform) */
bool gfx_hdr_dxgi_failed(void);
/* whether the window's monitor is in HDR mode; its peak and Windows' SDR content
 * brightness in nits (0 when unknown) */
bool gfx_hdr_dxgi_display(struct SDL_Window* window, float* peak_nits, float* white_nits);

#ifdef __cplusplus
}
#endif

#endif
