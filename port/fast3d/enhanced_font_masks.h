#ifndef PORT_ENHANCED_FONT_MASKS_H
#define PORT_ENHANCED_FONT_MASKS_H
/* Enhanced font masks (FORK-052, font styles plan FONT-2): the pure steps that
 * turn a player-installed GEPD font image into the 8x mask drawn in place of
 * a ROM glyph. They follow the removed FORK-021 generator
 * (378bee0^:scripts/port/generate_enhanced_fonts.py) so the masks match what
 * it produced: authored X coordinates kept, at most one native pixel of ink
 * past the glyph's width, oversize images fitted with Pillow's Lanczos filter,
 * and fringe that would touch a tightly kerned neighbour trimmed. */
#include <cstddef>
#include <cstdint>
#include <vector>

namespace enhanced_font_masks {

constexpr int kScale = 8;  /* mask texels per ROM texel */

/* A ROM glyph's metrics, as textrelated.c registers them. */
struct GlyphInfo {
    int width = 0, height = 0, baseline = 0, kerningindex = 0;
    int stride() const { return (width + 7) & ~7; }
};

/* An 8x mask over the ROM bitmap (stride * 8 by height * 8 texels). ink_width
 * is one past its rightmost inked column, in mask texels. */
struct Mask {
    int width = 0, height = 0, ink_width = 0;
    std::vector<uint8_t> alpha;
};

/* The pinned list's row for a pack file (enhanced_fonts_list.inc). */
struct PackFile {
    int font;
    char ch;
    const char *path;   /* in the pack, under "GoldenEye Texture Pack/" */
    uint32_t size;
    const char *sha256; /* lowercase hex */
};

const PackFile *pack_files(size_t &count);
/* The file's name in Data/FontPacks/gepd/: the last part of its path. */
const char *file_name(const PackFile &file);
/* Whether data is exactly the pinned file. */
bool verify(const uint8_t *data, size_t size, const PackFile &file);

/* Pillow's Image.resize(..., LANCZOS) for one 8-bit channel. */
std::vector<uint8_t> resize_lanczos(const std::vector<uint8_t> &src, int sw, int sh, int dw, int dh);

/* The mask from an RGBA image (its red channel) for a glyph, before the
 * kerning trim; false for an empty image or glyph. */
bool prepare(const std::vector<uint8_t> &rgba, int width, int height, const GlyphInfo &glyph, Mask &out);

/* A font's glyphs, prepared, for trim_overhangs. */
struct Prepared {
    GlyphInfo glyph;
    Mask *mask;
};
/* Trims each mask's ink past its glyph's width where it would touch any
 * glyph that may follow it (kerning: the font's 13 x 13 table), comparing the
 * whole font before trimming so the result does not depend on order. */
void trim_overhangs(const std::vector<Prepared> &font, const int32_t *kerning);

}  // namespace enhanced_font_masks
#endif
