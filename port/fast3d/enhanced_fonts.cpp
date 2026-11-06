/* Enhanced fonts (FORK-052, FONT-2): the registry and folder loading around
 * enhanced_font_masks. See enhanced_fonts.h. */
#include "enhanced_fonts.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <mutex>
#include <unordered_map>

#include "png_decode.h"

namespace enhanced_fonts {
namespace {

using enhanced_font_masks::GlyphInfo;
using enhanced_font_masks::Mask;

constexpr int kFonts = 2, kGlyphs = 94;

struct Slot {
    const uint8_t *pixels = nullptr;
    GlyphInfo info;
    Mask mask;
    bool has_mask = false;
};

struct State {
    std::array<std::array<Slot, kGlyphs>, kFonts> fonts;
    std::array<std::array<int32_t, 13 * 13>, kFonts> kerning{};
    std::array<bool, kFonts> has_kerning{};
    std::unordered_map<const uint8_t *, std::pair<int, int>> by_address;
    std::string directory = "Data/FontPacks/gepd";
    bool loaded = false;
    int installed = 0;
};

std::mutex g_lock;
State g_state;

bool read_file(const std::string &path, std::vector<uint8_t> &out) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    out.clear();
    uint8_t buffer[4096];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0 && out.size() < (1u << 20)) out.insert(out.end(), buffer, buffer + n);
    std::fclose(f);
    return true;
}

/* Reads, verifies and prepares every pinned file whose glyph is registered,
 * then trims each font's overhangs. Called with g_lock held. */
void load_locked(State &s) {
    s.loaded = true;
    s.installed = 0;
    for (auto &font : s.fonts) {
        for (Slot &slot : font) {
            slot.has_mask = false;
            slot.mask = Mask{};
        }
    }
    size_t count = 0;
    const enhanced_font_masks::PackFile *files = enhanced_font_masks::pack_files(count);
    std::vector<uint8_t> data, rgba;
    for (size_t i = 0; i < count; i++) {
        const auto &file = files[i];
        if (!read_file(s.directory + "/" + enhanced_font_masks::file_name(file), data)) continue;
        if (!enhanced_font_masks::verify(data.data(), data.size(), file)) {
            std::fprintf(stderr, "ge: enhanced fonts: %s does not match its pinned checksum; skipped\n",
                         enhanced_font_masks::file_name(file));
            continue;
        }
        s.installed++;
        if (file.font < 0 || file.font >= kFonts || file.ch < 0x21 || file.ch > 0x7e) continue;
        Slot &slot = s.fonts[file.font][file.ch - 0x21];
        int w = 0, h = 0;
        if (slot.pixels == nullptr || !png_decode::rgba(data.data(), data.size(), rgba, w, h)) continue;
        slot.has_mask = enhanced_font_masks::prepare(rgba, w, h, slot.info, slot.mask);
    }
    for (int f = 0; f < kFonts; f++) {
        if (!s.has_kerning[f]) continue;
        std::vector<enhanced_font_masks::Prepared> prepared;
        for (Slot &slot : s.fonts[f]) {
            if (slot.has_mask) prepared.push_back({slot.info, &slot.mask});
        }
        enhanced_font_masks::trim_overhangs(prepared, s.kerning[f].data());
    }
    if (s.installed > 0) std::fprintf(stderr, "ge: enhanced fonts: %d of %zu files installed\n", s.installed, count);
}

}  // namespace

bool find(const uint8_t *addr, Drawn &out) {
    std::lock_guard<std::mutex> guard(g_lock);
    auto it = g_state.by_address.find(addr);
    if (it == g_state.by_address.end()) return false;
    if (!g_state.loaded) load_locked(g_state);
    const Slot &slot = g_state.fonts[it->second.first][it->second.second];
    if (!slot.has_mask) return false;
    out = Drawn{&slot.mask, slot.info.width, slot.info.stride(), slot.info.height};
    return true;
}

void rgba(const Mask &mask, std::vector<uint8_t> &out) {
    out.resize(mask.alpha.size() * 4);
    for (size_t i = 0; i < mask.alpha.size(); i++) std::fill_n(&out[i * 4], 4, mask.alpha[i]);
}

int32_t draw_extent(const Drawn &glyph, int32_t extent, int32_t step, int32_t origin) {
    using enhanced_font_masks::kScale;
    const int ink = glyph.mask->ink_width;
    if (extent <= 0 || step <= 0 || origin != 0 || ink <= glyph.width * kScale || ink > glyph.stride * kScale) return extent;
    const int32_t full = glyph.width * 4096 / step;
    /* Scaled native rectangles and reciprocal steps each truncate once.
     * Partial or repeated texture spans must not grow like whole glyphs. */
    if (extent < full - 1 || extent > full + 1) return extent;
    return std::max(extent, (ink * 512 + step - 1) / step);
}

int installed_count() {
    std::lock_guard<std::mutex> guard(g_lock);
    if (!g_state.loaded) load_locked(g_state);
    return g_state.installed;
}

void reload() {
    std::lock_guard<std::mutex> guard(g_lock);
    g_state.loaded = false;
}

void set_directory(const std::string &directory) {
    std::lock_guard<std::mutex> guard(g_lock);
    g_state.directory = directory;
    g_state.loaded = false;
}

std::string directory() {
    std::lock_guard<std::mutex> guard(g_lock);
    return g_state.directory;
}

}  // namespace enhanced_fonts

extern "C" void portEnhancedFontsGlyph(int font, int ch, const unsigned char *pixels, int width, int height, int baseline,
                                       int kerningindex, const int *kerning) {
    using namespace enhanced_fonts;
    if (font < 0 || font >= kFonts || ch < 0x21 || ch > 0x7e || pixels == nullptr || kerningindex < 0 || kerningindex >= 13) {
        return;
    }
    std::lock_guard<std::mutex> guard(g_lock);
    State &s = g_state;
    Slot &slot = s.fonts[font][ch - 0x21];
    const GlyphInfo info{width, height, baseline, kerningindex};
    if (slot.pixels) s.by_address.erase(slot.pixels);  /* the font reloaded at a new address */
    if (slot.info.width != width || slot.info.height != height || slot.info.baseline != baseline
        || slot.info.kerningindex != kerningindex) {
        s.loaded = false;  /* new metrics: prepare the masks again */
    }
    slot.pixels = pixels;
    slot.info = info;
    if (kerning != nullptr) {
        std::copy(kerning, kerning + 13 * 13, s.kerning[font].begin());
        s.has_kerning[font] = true;
    }
    s.by_address[pixels] = {font, ch - 0x21};
}
