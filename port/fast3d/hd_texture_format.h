#ifndef MIDAS_HD_TEXTURE_FORMAT_H
#define MIDAS_HD_TEXTURE_FORMAT_H
/* Player-installed HD texture packs (FORK-053): a native texture's identity in
 * the Rice naming used by GLideN64 PNG packs. */
#include <compare>
#include <cstddef>
#include <cstdint>
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
/* The tile's Rice texture (and, for CI, palette) fingerprint. */
bool identify(const Tile& tile, Key& key);
}
#endif
