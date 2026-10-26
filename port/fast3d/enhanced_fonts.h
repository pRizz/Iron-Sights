#ifndef PORT_ENHANCED_FONTS_H
#define PORT_ENHANCED_FONTS_H
#include <cstddef>
#include <cstdint>
#include <vector>

namespace enhanced_fonts {
struct FontRecord {
    uint64_t fingerprint;
    uint16_t width, height;
    uint16_t glyph_width, ink_width; /* native advance box; authored ink in 8x texels */
    uint32_t offset, size;
};
const FontRecord *find(const uint8_t *pixels, size_t size, unsigned width, unsigned height);
bool decode(const uint8_t *encoded, size_t size, unsigned width, unsigned height, std::vector<uint8_t>& rgba);
bool load(const FontRecord& record, std::vector<uint8_t>& rgba);
/* Extend only a complete glyph's drawing, never its native cursor advance.
 * Extent is in quarter pixels; step is S5.10 and origin is S10.5. */
int32_t drawExtent(const FontRecord& record, int32_t extent, int32_t step, int32_t origin);
}
#endif
