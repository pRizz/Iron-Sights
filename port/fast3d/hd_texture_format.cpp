#if !defined(__EMSCRIPTEN__) && !defined(GE_XBOX)
#include "hd_texture_format.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
extern "C" {
#include "third_party/puff/puff.h"
}
namespace hd_textures {
/* Rice reads emulator word-swapped RDRAM as native words. Reconstruct those
 * values from this port's native u16/u32 texels or byte-addressed I/IA/CI data. */
static uint8_t n64byte(const uint8_t *p, unsigned offset, unsigned unit)
{
    if (unit == 2) {
        uint16_t v; std::memcpy(&v, p + (offset & ~1u), 2);
        return uint8_t(v >> ((1 - (offset & 1)) * 8));
    }
    if (unit == 4) {
        uint32_t v; std::memcpy(&v, p + (offset & ~3u), 4);
        return uint8_t(v >> ((3 - (offset & 3)) * 8));
    }
    return p[offset];
}
static uint32_t rice(const uint8_t *p, unsigned rowBytes, unsigned rows,
                     unsigned stride, unsigned unit, unsigned oddSwap = 0)
{
    uint32_t hash = 0;
    for (unsigned y = 0; y < rows; ++y) {
        uint32_t word = 0;
        for (int x = int(rowBytes) - 4; x >= 0; x -= 4) {
            word = 0;
            for (unsigned b = 0; b < 4; ++b) {
                const unsigned offset = unsigned(x)+b;
                const unsigned source = (y & 1) && oddSwap && stride >= oddSwap*2 ? offset ^ oddSwap : offset;
                word = (word << 8) | n64byte(p, source, unit);
            }
            word ^= unsigned(x);
            hash = ((hash << 4) | (hash >> 28)) + word;
        }
        hash += word ^ (rows-1-y);
        p += stride;
    }
    return hash;
}
bool identify(const Tile& t, Key& key)
{
    if (!t.pixels || !t.width || !t.height || t.width > 4096 || t.height > 4096 || t.size > 3 || t.format > 4) return false;
    const unsigned row = (t.width << t.size) >> 1;
    if (row < 4 || t.stride < row || t.stride > 16384 || t.bytes < size_t(t.height)*t.stride) return false;
    if (t.restoreSwizzle && t.stride % (t.size == 3 ? 16 : 8)) return false;
    const unsigned unit = t.format == 0 ? (t.size == 3 ? 4 : t.size == 2 ? 2 : 1)
                         : t.format == 3 && t.size == 2 ? 2 : 1;
    key = {rice(t.pixels, row, t.height, t.stride, unit, t.restoreSwizzle ? (t.size == 3 ? 8 : 4) : 0), 0, t.format, t.size};
    if (t.format != 2) return true;
    if (!t.palette || !t.paletteCount || t.size > 1) return false;
    unsigned maximum = 0;
    for (unsigned y = 0; y < t.height; ++y)
        for (unsigned x = 0; x < t.width; ++x) {
            const uint8_t v = t.pixels[y*t.stride + (t.size ? x : x/2)];
            maximum = std::max(maximum, t.size ? unsigned(v) : unsigned((x & 1) ? v & 15 : v >> 4));
        }
    if (maximum >= t.paletteCount || maximum < 1) return false;
    key.palette = rice(reinterpret_cast<const uint8_t *>(t.palette), (maximum+1)*2, 1, 512, 2);
    return true;
}
uint32_t pixelHash(const uint8_t *bytes, size_t size)
{
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < size;) {
        const size_t end = std::min(size, i+5552);
        for (; i < end; ++i) { a += bytes[i]; b += a; }
        a %= 65521; b %= 65521;
    }
    return (b << 16) | a;
}
static unsigned paeth(int a, int b, int c)
{
    const int p = a+b-c, da = std::abs(p-a), db = std::abs(p-b), dc = std::abs(p-c);
    return da <= db && da <= dc ? a : db <= dc ? b : c;
}
bool unpack(const uint8_t *bytes, size_t size, unsigned width, unsigned height,
            uint32_t hash, std::vector<uint8_t>& rgba)
{
    rgba.clear();
    if (!bytes || !size || size > 80*1024*1024 || !width || !height || width > 4096 || height > 4096) return false;
    const size_t stride = size_t(width)*4;
    unsigned long length = (stride+1)*height, input = size;
    std::vector<uint8_t> filtered(length);
    if (puff(filtered.data(), &length, bytes, &input) || input != size || length != filtered.size()) return false;
    rgba.resize(stride*height);
    for (unsigned y = 0; y < height; ++y) {
        const uint8_t filter = filtered[y*(stride+1)];
        if (filter > 4) { rgba.clear(); return false; }
        for (size_t x = 0; x < stride; ++x) {
            const unsigned a = x >= 4 ? rgba[y*stride+x-4] : 0;
            const unsigned b = y ? rgba[(y-1)*stride+x] : 0;
            const unsigned c = y && x >= 4 ? rgba[(y-1)*stride+x-4] : 0;
            const unsigned predictor = filter == 1 ? a : filter == 2 ? b : filter == 3 ? (a+b)/2 : filter == 4 ? paeth(a,b,c) : 0;
            rgba[y*stride+x] = uint8_t(filtered[y*(stride+1)+1+x] + predictor);
        }
    }
    if (pixelHash(rgba.data(), rgba.size()) != hash) { rgba.clear(); return false; }
    return true;
}
}
#endif
