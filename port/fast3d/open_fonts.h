#ifndef PORT_OPEN_FONTS_H
#define PORT_OPEN_FONTS_H
/* Open fonts (FORK-050): the vendored MIT title font drawn in place of the
 * ROM's Bank Gothic glyphs as multi-channel signed distance fields.
 *
 * The game keeps its own text layout. Each ROM glyph's texture is replaced by
 * a field image resampled from the open font's atlas over a box in the ROM
 * glyph's texel space (origin at the bitmap's top-left, y down): the union of
 * the ROM glyph and our padded glyph, so our ink may reach past the ROM
 * bitmap. The texture-rectangle path grows the quad to that box. */
#include <cstdint>
#include <vector>

namespace open_fonts {
struct Glyph;

struct Box {
    float x0, y0, x1, y1; /* ROM texels */
};

/* The registered ROM glyph whose pixels start at addr, or nullptr. */
const Glyph *find(const uint8_t *addr);
/* Whether the open font can draw this glyph (atlas loaded, glyph present). */
bool usable(const Glyph &glyph);
/* The ROM bitmap's size in texels. */
void rom_size(const Glyph &glyph, int &width, int &height);
/* The box the field image covers. */
Box box(const Glyph &glyph);
/* The RGBA field image over box(); false when the open font is unavailable. */
bool image(const Glyph &glyph, std::vector<uint8_t> &rgba, uint32_t &width, uint32_t &height);
}

extern "C" {
/* textrelated.c: a ROM font's glyph at chars[ch - 0x21]. font 0 is Bank Gothic
 * (drawn with the open title font); other fonts are not replaced yet. */
void portOpenFontsGlyph(int font, int ch, const unsigned char *pixels, int width, int height, int baseline);
}
#endif
