#ifndef MIDAS_GFX_MEMORY_H
#define MIDAS_GFX_MEMORY_H
#include <cstddef>
#include <cstring>
#include "system.h"

static inline bool gfx_span_fits(size_t offset, size_t count, size_t capacity)
{
    return offset <= capacity && count <= capacity - offset;
}

/* Ambient colours have eight bytes; their Light_t-sized command encoding
 * does not authorize reading the following object's direction/padding. */
template<class Light>
static void gfx_copy_light(Light &destination, const void *source, bool ambient)
{
    std::memset(&destination, 0, sizeof(destination));
    std::memcpy(&destination, source, ambient ? 8 : sizeof(destination));
}
#endif
