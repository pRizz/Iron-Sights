/* Points and pixels (MAC-002 Task 6). On a HiDPI ("Retina") display SDL
 * measures the window, the mouse and the pointer warp in points, and the GL
 * drawable in pixels, more than one a point. The rule: sizes saved to
 * midas.ini and positions given to SDL are points; sizes given to the
 * renderer and compared with the framebuffer are pixels. These convert
 * between the two for one axis. On a build whose window is one pixel a point
 * (every build but macOS's, and a macOS run with GE_WINDOW set) the two sizes
 * are equal and each returns its value unchanged.
 * scripts/port/test_hidpi.sh tests them. No game header: the renderer's C++
 * includes this too. int32_t is the game's s32. */
#ifndef PORT_HIDPI_H
#define PORT_HIDPI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* VALUE, a coordinate or length in points on an axis POINTS long that is
 * drawn on PIXELS pixels, in pixels: VALUE * PIXELS / POINTS rounded to the
 * nearest. VALUE itself when the two sizes are equal or either is not
 * positive (no window yet). */
int32_t portHidpiToPixels(int32_t value, int32_t points, int32_t pixels);

/* The inverse: VALUE in pixels, as points. VALUE itself when the two sizes
 * are equal or either is not positive. */
int32_t portHidpiToPoints(int32_t value, int32_t points, int32_t pixels);

#ifdef __cplusplus
}
#endif

/* A screenshot's rows, repacked in place for a 24-bit BMP file. PIXELS holds
 * HEIGHT rows of WIDTH packed RGB pixels, as glReadPixels returns them with a
 * pack alignment of 1, in a buffer of HEIGHT rows of the BMP's row size,
 * WIDTH * 3 rounded up to a multiple of 4. Afterwards each row is at its BMP
 * offset, blue first, with its padding bytes zero. Any width: the repacking
 * it replaces (port/fast3d/gfx_sdl2.cpp) copied each row through a fixed
 * 4,096-pixel stack buffer and overran it on a wider drawable. Nothing is
 * done for a width or height that is not positive. Defined here, not in
 * port_hidpi.c, because every build's screenshot uses it and only macOS
 * compiles that file. */
static inline void portBmpPackRows(uint8_t *pixels, int width, int height)
{
    size_t stride;
    size_t padded;
    int x;
    int y;

    if (pixels == NULL || width <= 0 || height <= 0)
    {
        return;
    }
    stride = (size_t) width * 3;
    padded = (stride + 3) & ~(size_t) 3;
    /* Last row first, and each row from its last pixel: a row's BMP place is
     * at or after its packed place and before the next row's, so nothing is
     * written over a byte still to be read, and no row needs a copy. */
    for (y = height - 1; y >= 0; y--)
    {
        const uint8_t *source = pixels + (size_t) y * stride;
        uint8_t *row = pixels + (size_t) y * padded;
        size_t pad;

        for (x = width - 1; x >= 0; x--)
        {
            uint8_t red = source[(size_t) x * 3 + 0];
            uint8_t green = source[(size_t) x * 3 + 1];
            uint8_t blue = source[(size_t) x * 3 + 2];

            row[(size_t) x * 3 + 0] = blue;
            row[(size_t) x * 3 + 1] = green;
            row[(size_t) x * 3 + 2] = red;
        }
        for (pad = stride; pad < padded; pad++)
        {
            row[pad] = 0;
        }
    }
}

#endif
