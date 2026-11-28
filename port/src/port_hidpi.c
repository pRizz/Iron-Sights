/* Points and pixels (MAC-002 Task 6). See port/include/port_hidpi.h for the
 * contracts and scripts/port/test_hidpi.sh for their tests. CMake compiles
 * this file only when APPLE is true: no other build has a window whose
 * points and pixels differ, and none calls these. */
#include "port_hidpi.h"

/* VALUE * TO / FROM rounded to the nearest, halves away from zero. */
static int32_t hidpiScale(int32_t value, int32_t from, int32_t to)
{
    int64_t scaled;

    if (from <= 0 || to <= 0 || from == to)
    {
        return value;
    }
    scaled = (int64_t) value * to;
    if (scaled >= 0)
    {
        return (int32_t) ((scaled + from / 2) / from);
    }
    return (int32_t) -((-scaled + from / 2) / from);
}

int32_t portHidpiToPixels(int32_t value, int32_t points, int32_t pixels)
{
    return hidpiScale(value, points, pixels);
}

int32_t portHidpiToPoints(int32_t value, int32_t points, int32_t pixels)
{
    return hidpiScale(value, pixels, points);
}
