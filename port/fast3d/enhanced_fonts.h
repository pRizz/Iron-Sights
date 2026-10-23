#ifndef PORT_ENHANCED_FONTS_H
#define PORT_ENHANCED_FONTS_H
#include <cstddef>
#include <cstdint>
#include <vector>

namespace enhanced_fonts {
struct FontRecord {
    uint64_t fingerprint;
    uint16_t width, height;
    uint32_t offset, size;
};
const FontRecord *find(const uint8_t *pixels, size_t size, unsigned width, unsigned height);
bool decode(const uint8_t *encoded, size_t size, unsigned width, unsigned height, std::vector<uint8_t>& rgba);
bool load(const FontRecord& record, std::vector<uint8_t>& rgba);
}
#endif
