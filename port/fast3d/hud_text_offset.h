#ifndef PORT_HUD_TEXT_OFFSET_H
#define PORT_HUD_TEXT_OFFSET_H
#include <stdint.h>

/* G_NOOP tag: signed framebuffer-pixel offsets for a scoped font-glyph run. */
#define PORT_HUD_TEXT_OFFSET_TAG 0x5a000000u
typedef struct { int x, y; } PortHudTextOffset;

static inline uint32_t portHudTextOffsetTag(int x, int y)
{
    return PORT_HUD_TEXT_OFFSET_TAG | ((uint32_t)(x & 255) << 8) | (uint32_t)(y & 255);
}

static inline PortHudTextOffset portHudTextOffsetDecode(uint32_t word)
{
    PortHudTextOffset offset = {(int)((word >> 8) & 255), (int)(word & 255)};
    if (offset.x >= 128) offset.x -= 256;
    if (offset.y >= 128) offset.y -= 256;
    return offset;
}

static inline float portHudTextOffsetNdc(int pixels, unsigned extent)
{
    return extent ? 2.0f * pixels / extent : 0.0f;
}
#endif
