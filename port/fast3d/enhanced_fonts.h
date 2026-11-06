#ifndef PORT_ENHANCED_FONTS_H
#define PORT_ENHANCED_FONTS_H
/* Enhanced fonts (FORK-052, font styles plan FONT-2): Carnivorous's CC0 GEPD
 * font images, installed by the player in Data/FontPacks/gepd/ and never
 * bundled, drawn as 8x masks in place of the ROM's Bank Gothic and Zurich Bold
 * glyphs when Fonts is Enhanced. Each file must match its pinned size and
 * SHA-256 (enhanced_fonts_list.inc); a glyph without one keeps its ROM bitmap.
 * Advances, kerning and line spacing stay the ROM's. */
#include <cstdint>
#include <string>
#include <vector>

#include "enhanced_font_masks.h"

namespace enhanced_fonts {

/* A registered ROM glyph with an installed mask. */
struct Drawn {
    const enhanced_font_masks::Mask *mask;
    int width;   /* the ROM glyph's width (its advance box), texels */
    int stride;  /* the ROM bitmap's row length, texels */
    int height;
};

/* The glyph whose ROM pixels start at addr, when its mask is installed. The
 * first call reads the folder. */
bool find(const uint8_t *addr, Drawn &out);
/* The mask as an RGBA texture (every channel the mask), like an I8 upload. */
void rgba(const enhanced_font_masks::Mask &mask, std::vector<uint8_t> &out);
/* A texture rectangle's width (extent, in the rect's units) grown to show the
 * mask's ink past the glyph's width. Only a whole glyph drawn from its first
 * texel grows; step is S5.10 and origin S10.5, as gfx_dp_texture_rectangle. */
int32_t draw_extent(const Drawn &glyph, int32_t extent, int32_t step, int32_t origin);
/* How many pinned files are installed and valid (reads the folder). */
int installed_count();
/* Forget what was read so the next find() reads the folder again. */
void reload();
/* The folder to read (default Data/FontPacks/gepd); for tests. */
void set_directory(const std::string &directory);
/* The folder the files are read from and installed into. */
std::string directory();

}  // namespace enhanced_fonts

extern "C" {
/* textrelated.c: a ROM font's glyph at chars[ch - 0x21], with the font's
 * 13 x 13 kerning table. font 0 is Bank Gothic, 1 Zurich Bold. */
void portEnhancedFontsGlyph(int font, int ch, const unsigned char *pixels, int width, int height, int baseline,
                            int kerningindex, const int *kerning);
}
#endif
