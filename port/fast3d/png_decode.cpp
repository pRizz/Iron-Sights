/* PNG decoding for the font images (FORK-051, FORK-052). See png_decode.h. */
#include "png_decode.h"

#include <cstdlib>
#include <cstring>

extern "C" {
#include "third_party/puff/puff.h"
}

namespace png_decode {
namespace {

uint32_t read_be32(const uint8_t *p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

int paeth(int a, int b, int c) {
    const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

/* Undo the PNG row filters of raw ((stride + 1) bytes per row) into pixels. */
bool unfilter(const std::vector<uint8_t> &raw, size_t stride, int height, int bpp, std::vector<uint8_t> &pixels) {
    pixels.assign(stride * size_t(height), 0);
    for (int y = 0; y < height; y++) {
        const uint8_t filter = raw[y * (stride + 1)];
        const uint8_t *src = &raw[y * (stride + 1) + 1];
        uint8_t *dst = &pixels[y * stride];
        const uint8_t *up = y ? &pixels[(y - 1) * stride] : nullptr;
        for (size_t x = 0; x < stride; x++) {
            const size_t back = size_t(bpp);
            const int a = x >= back ? dst[x - back] : 0, b = up ? up[x] : 0, c = (up && x >= back) ? up[x - back] : 0;
            int v = src[x];
            switch (filter) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: v += paeth(a, b, c); break;
                default: return false;
            }
            dst[x] = uint8_t(v);
        }
    }
    return true;
}

}  // namespace

bool rgba(const uint8_t *data, size_t size, std::vector<uint8_t> &out, int &width, int &height) {
    static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (data == nullptr || size < 8 || std::memcmp(data, signature, 8) != 0) return false;
    std::vector<uint8_t> idat;
    size_t pos = 8;
    int bpp = 0;
    width = height = 0;
    while (pos + 12 <= size) {
        const uint32_t length = read_be32(data + pos);
        const uint8_t *type = data + pos + 4, *body = data + pos + 8;
        if (length > size - pos - 12) return false;
        if (std::memcmp(type, "IHDR", 4) == 0) {
            if (length < 13) return false;
            width = int(read_be32(body));
            height = int(read_be32(body + 4));
            if (body[8] != 8 || (body[9] != 2 && body[9] != 6) || body[12] != 0) return false;  /* 8-bit RGB(A), no interlace */
            bpp = body[9] == 6 ? 4 : 3;
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), body, body + length);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + length;
    }
    if (bpp == 0 || width <= 0 || height <= 0 || width > 16384 || height > 16384 || idat.size() < 6) return false;
    const size_t stride = size_t(width) * bpp;
    std::vector<uint8_t> raw((stride + 1) * size_t(height));
    unsigned long out_len = raw.size(), in_len = idat.size() - 6;  /* skip the zlib header and Adler-32 */
    if (puff(raw.data(), &out_len, idat.data() + 2, &in_len) != 0 || out_len != raw.size()) return false;
    std::vector<uint8_t> pixels;
    if (!unfilter(raw, stride, height, bpp, pixels)) return false;
    if (bpp == 4) {
        out.swap(pixels);
        return true;
    }
    out.resize(size_t(width) * height * 4);
    for (size_t i = 0, n = size_t(width) * height; i < n; i++) {
        out[i * 4] = pixels[i * 3];
        out[i * 4 + 1] = pixels[i * 3 + 1];
        out[i * 4 + 2] = pixels[i * 3 + 2];
        out[i * 4 + 3] = 255;
    }
    return true;
}

}  // namespace png_decode
