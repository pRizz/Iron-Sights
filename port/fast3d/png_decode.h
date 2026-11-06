#ifndef PORT_PNG_DECODE_H
#define PORT_PNG_DECODE_H
/* PNG decoding for the font images (FORK-051, FORK-052): 8-bit RGB or RGBA,
 * non-interlaced, inflated with the vendored puff. Other PNGs are refused. */
#include <cstddef>
#include <cstdint>
#include <vector>

namespace png_decode {
/* The image as RGBA (opaque for RGB); false for a PNG it cannot read. */
bool rgba(const uint8_t *data, size_t size, std::vector<uint8_t> &out, int &width, int &height);
}
#endif
