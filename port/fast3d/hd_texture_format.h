#ifndef MIDAS_HD_TEXTURE_FORMAT_H
#define MIDAS_HD_TEXTURE_FORMAT_H
#include <cstdint>
#include <cstddef>
#include <vector>
#include <compare>
namespace hd_textures {
struct Tile {
    const uint8_t *pixels;
    size_t bytes;
    unsigned width, height, stride, format, size;
    const uint16_t *palette;
    unsigned paletteCount;
    bool restoreSwizzle = true;
};
struct Key {
    uint32_t texture, palette, format, size;
    auto operator<=>(const Key&) const = default;
};
bool identify(const Tile& tile, Key& key);
uint32_t pixelHash(const uint8_t *bytes, size_t size);
bool unpack(const uint8_t *bytes, size_t size, unsigned width, unsigned height,
            uint32_t hash, std::vector<uint8_t>& rgba);
}
#endif
