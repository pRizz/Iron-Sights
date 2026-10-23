#include <cassert>
#include <vector>
#include "enhanced_fonts.cpp"

int main()
{
    using namespace enhanced_fonts;
    std::vector<uint8_t> pixels;
    size_t total = 0;
    for (const auto& record : font_records) {
        assert(load(record, pixels));
        assert(pixels.size() == size_t(record.width)*record.height*64*4);
        bool ink = false, empty = false, antialiased = false;
        for (size_t i = 0; i < pixels.size(); i += 4) {
            assert(pixels[i] == pixels[i+1] && pixels[i] == pixels[i+2] && pixels[i] == pixels[i+3]);
            ink |= pixels[i] != 0;
            empty |= pixels[i] == 0;
            antialiased |= pixels[i] != 0 && pixels[i] != 255;
        }
        assert(ink && empty && antialiased);
        ++total;
    }
    assert(total == 186);
    const uint8_t valid[] = {2, 128, 2, 0};
    assert(decode(valid, sizeof(valid), 2, 2, pixels));
    assert(pixels[0] == 128 && pixels[8] == 0);
    const uint8_t overflow[] = {5, 255}, zero[] = {0, 255};
    assert(!decode(overflow, sizeof(overflow), 2, 2, pixels) && pixels.empty());
    assert(!decode(zero, sizeof(zero), 2, 2, pixels));
    assert(!decode(valid, 3, 2, 2, pixels));
    assert(!decode(valid, 2, 2, 2, pixels));
    assert(!decode(valid, 4, 999, 2, pixels));
    assert(!find(valid, sizeof(valid), 2, 2));
    assert(!find(valid, sizeof(valid), 2, 3));
    FontRecord bad = {0, 8, 8, uint32_t(sizeof(font_masks)), 2};
    assert(!load(bad, pixels));
}
