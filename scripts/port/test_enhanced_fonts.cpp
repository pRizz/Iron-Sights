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
        // Authored ink stays drawable at the text scales used by HUD/menu labels.
        for (const int step : {512, 1024, 1280, 1365, 2048}) {
            const int nativeExtent = record.glyph_width*4096/step;
            const int drawn = drawExtent(record, nativeExtent, step, 0);
            assert(drawn >= nativeExtent);
            if (record.ink_width > record.glyph_width*8)
                assert(drawn*step >= record.ink_width*512);
        }
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
    FontRecord bad = {0, 8, 8, 1, 12, uint32_t(sizeof(font_masks)), 2};
    assert(!load(bad, pixels));
    // Arrange: authored I overhang; native pen/kerning values live outside this API.
    const FontRecord narrow = {0, 8, 8, 1, 12, 0, 0};
    // Act / assert: full, 80%, 75% and double-sized drawing extents keep ink.
    assert(drawExtent(narrow, 4, 1024, 0) == 6);
    assert(drawExtent(narrow, 3, 1280, 0) == 5);
    assert(drawExtent(narrow, 3, 1365, 0) == 5);
    assert(drawExtent(narrow, 8, 512, 0) == 12);
    // Clipped/repeated spans, invalid inputs and ordinary glyphs stay unchanged.
    assert(drawExtent(narrow, 2, 1024, 0) == 2);
    assert(drawExtent(narrow, 8, 1024, 0) == 8);
    assert(drawExtent(narrow, 4, 1024, 32) == 4);
    assert(drawExtent(narrow, 4, 0, 0) == 4);
    assert(drawExtent(narrow, 4, -1024, 0) == 4);
    assert(drawExtent(narrow, 0, 1024, 0) == 0);
    FontRecord ordinary = {0, 8, 8, 7, 53, 0, 0};
    assert(drawExtent(ordinary, 28, 1024, 0) == 28);
    ordinary.ink_width = 65;
    assert(drawExtent(ordinary, 28, 1024, 0) == 28);
    bad = font_records[0]; bad.glyph_width = 0;
    assert(!load(bad, pixels));
    bad = font_records[0]; bad.glyph_width = bad.width+1;
    assert(!load(bad, pixels));
    bad = font_records[0]; bad.ink_width = 0;
    assert(!load(bad, pixels));
    bad = font_records[0]; bad.ink_width = bad.width*8+1;
    assert(!load(bad, pixels));
}
