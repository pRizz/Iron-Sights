// ROM-free checks for the open-font glyph path (FORK-050): the vendored atlases
// decode, a registered glyph places on the ROM baseline and its field image
// is inside where the glyph has ink and outside past it.
#include <cassert>
#include <cstdio>
#include <vector>

#include "../../port/fast3d/open_fonts.h"

static int median(const std::vector<uint8_t> &rgba, uint32_t width, uint32_t x, uint32_t y) {
    const uint8_t *p = &rgba[(size_t(y) * width + x) * 4];
    const int r = p[0], g = p[1], b = p[2];
    return std::max(std::min(r, g), std::min(std::max(r, g), b));
}

int main() {
    // Arrange: a ROM-like font with an 8-texel capital H whose bitmap starts 2
    // rows below the line top, and an I and a comma of its own sizes.
    // Its ink, like the ROM's, fills rows 1..6 of 8 at about 85% (stride 8).
    static uint8_t pixels[94][16 * 16];
    uint8_t *h_ink = pixels['H' - 0x21];
    for (int y = 1; y <= 6; y++) h_ink[y * 8 + 0] = h_ink[y * 8 + 6] = 220;
    for (int x = 0; x < 7; x++) h_ink[3 * 8 + x] = 220;
    for (int ch = 0x21; ch <= 0x7e; ch++) portOpenFontsGlyph(0, ch, pixels[ch - 0x21], 7, 8, 2);
    portOpenFontsGlyph(0, 'I', pixels['I' - 0x21], 2, 8, 2);
    portOpenFontsGlyph(0, ',', pixels[',' - 0x21], 2, 4, 7);
    // Zurich Bold (font 1) draws with the body font, from its own H.
    static uint8_t body[94][16 * 16];
    for (int y = 1; y <= 9; y++) body['H' - 0x21][y * 8 + 0] = body['H' - 0x21][y * 8 + 6] = 220;
    for (int ch = 0x21; ch <= 0x7e; ch++) portOpenFontsGlyph(1, ch, body[ch - 0x21], 7, 11, 1);

    // Act
    const open_fonts::Glyph *h = open_fonts::find(pixels['H' - 0x21]);
    const open_fonts::Glyph *i = open_fonts::find(pixels['I' - 0x21]);
    const open_fonts::Glyph *comma = open_fonts::find(pixels[',' - 0x21]);

    // Assert: registration and the atlas.
    assert(h && i && comma);
    assert(open_fonts::usable(*h) && open_fonts::usable(*i) && open_fonts::usable(*comma));
    const open_fonts::Glyph *body_a = open_fonts::find(body['a' - 0x21]);
    assert(body_a && open_fonts::usable(*body_a));
    std::vector<uint8_t> body_rgba;
    uint32_t bw = 0, bh = 0;
    assert(open_fonts::image(*body_a, body_rgba, bw, bh) && bw > 0 && bh > 0);

    // The box covers the ROM bitmap; a comma's box reaches below its 4 rows.
    const open_fonts::Box hb = open_fonts::box(*h);
    assert(hb.x0 <= 0 && hb.y0 <= 0 && hb.x1 >= 7 && hb.y1 >= 8);
    const open_fonts::Box cb = open_fonts::box(*comma);
    assert(cb.y1 > 4.0f);

    // The field: inside at the I's stem centre, outside at the box's corner.
    std::vector<uint8_t> rgba;
    uint32_t w = 0, ht = 0;
    assert(open_fonts::image(*i, rgba, w, ht));
    assert(w > 0 && ht > 0 && rgba.size() == size_t(w) * ht * 4);
    const open_fonts::Box ib = open_fonts::box(*i);
    const float px = w / (ib.x1 - ib.x0), py = ht / (ib.y1 - ib.y0);
    const uint32_t cx = uint32_t((1.0f - ib.x0) * px), cy = uint32_t((4.0f - ib.y0) * py);  // stem centre
    assert(median(rgba, w, cx, cy) > 160);
    assert(median(rgba, w, 0, 0) < 96);
    // Our capitals match the ROM capital's ink: its foot at texel row 7, its
    // top at row 1 (both rows 1..6 of the H, the I sharing its baseline).
    const uint32_t above = uint32_t((6.6f - ib.y0) * py), below = uint32_t((7.6f - ib.y0) * py);
    const uint32_t top_in = uint32_t((1.4f - ib.y0) * py), top_out = uint32_t((0.6f - ib.y0) * py);
    assert(median(rgba, w, cx, above) > 128);
    assert(median(rgba, w, cx, below) < 128);
    assert(median(rgba, w, cx, top_in) > 128);
    assert(median(rgba, w, cx, top_out) < 128);

    std::printf("open fonts: ok\n");
    return 0;
}
