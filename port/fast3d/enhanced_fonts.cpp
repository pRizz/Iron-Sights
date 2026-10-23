#include <cstdio>
#include "enhanced_fonts.h"
#include <algorithm>
#include <iterator>

namespace enhanced_fonts {
#include "fonts/gepd_font_data.inc"

const FontRecord *find(const uint8_t *pixels, size_t size, unsigned width, unsigned height)
{
    if (!pixels || width == 0 || width > 32 || height == 0 || height > 32 || size != width*height) return nullptr;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ pixels[i]) * 1099511628211ULL;
    auto record = std::lower_bound(std::begin(font_records), std::end(font_records), hash,
        [](const FontRecord& item, uint64_t value) { return item.fingerprint < value; });
    if (record == std::end(font_records) || record->fingerprint != hash || record->width != width || record->height != height) return nullptr;
    return record;
}

bool decode(const uint8_t *encoded, size_t size, unsigned width, unsigned height, std::vector<uint8_t>& rgba)
{
    rgba.clear();
    if (!encoded || !size || (size & 1) || !width || !height || width > 256 || height > 256) return false;
    const size_t count = width*height;
    rgba.reserve(count*4);
    for (size_t i = 0; i < size; i += 2) {
        if (!encoded[i] || encoded[i] > count-rgba.size()/4) { rgba.clear(); return false; }
        rgba.insert(rgba.end(), encoded[i]*4, encoded[i+1]);
    }
    if (rgba.size() != count*4) { rgba.clear(); return false; }
    return true;
}

bool load(const FontRecord& record, std::vector<uint8_t>& rgba)
{
    if (record.offset > sizeof(font_masks) || record.size > sizeof(font_masks)-record.offset) return false;
    return decode(font_masks+record.offset, record.size, record.width*8, record.height*8, rgba);
}
}
