// ROM-free checks for the Enhanced fonts (FORK-052, font styles plan FONT-2):
// the pinned list, file verification, PNG decoding, mask preparation, the
// kerning trim, quad growth and the folder loader's refusal of bad files.
// No pack art is needed or used.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "../../port/fast3d/enhanced_fonts.h"
#include "../../port/fast3d/png_decode.h"

using namespace enhanced_font_masks;

namespace {

void be32(std::vector<uint8_t> &out, uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) out.push_back(uint8_t(v >> s));
}

void chunk(std::vector<uint8_t> &png, const char *type, const std::vector<uint8_t> &body) {
    be32(png, uint32_t(body.size()));
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), body.begin(), body.end());
    be32(png, 0);  // the decoder does not check CRCs
}

// A w x h RGB PNG from raw scanlines (each with its filter byte), stored uncompressed.
std::vector<uint8_t> rgb_png(int w, int h, const std::vector<uint8_t> &scanlines) {
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'}, ihdr, idat = {0x78, 0x01, 0x01};
    be32(ihdr, uint32_t(w));
    be32(ihdr, uint32_t(h));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    const uint16_t len = uint16_t(scanlines.size());
    idat.insert(idat.end(), {uint8_t(len), uint8_t(len >> 8), uint8_t(~len), uint8_t(~len >> 8)});
    idat.insert(idat.end(), scanlines.begin(), scanlines.end());
    be32(idat, 0);  // Adler-32, skipped
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", idat);
    chunk(png, "IEND", {});
    return png;
}

Mask mask_with_ink(int width, int height, int x0, int x1) {
    Mask m;
    m.width = width;
    m.height = height;
    m.alpha.assign(size_t(width) * height, 0);
    for (int y = 0; y < height; y++) {
        for (int x = x0; x < x1; x++) m.alpha[size_t(y) * width + x] = 255;
    }
    m.ink_width = x1;
    return m;
}

void test_pinned_list() {
    size_t count = 0;
    const PackFile *files = pack_files(count);
    assert(count == 186);
    std::set<std::string> names;
    int per_font[2] = {0, 0};
    for (size_t i = 0; i < count; i++) {
        assert(files[i].font == 0 || files[i].font == 1);
        assert(files[i].ch >= 0x21 && files[i].ch < 0x7e);  // no tilde in the pack
        assert(files[i].size > 0 && std::strlen(files[i].sha256) == 64);
        names.insert(file_name(files[i]));
        per_font[files[i].font]++;
    }
    assert(names.size() == 186 && per_font[0] == 93 && per_font[1] == 93);
    assert(std::string(file_name(files[0])).find('/') == std::string::npos);
}

void test_verify() {
    const uint8_t abc[] = {'a', 'b', 'c'};
    const PackFile file{0, 'A', "x/abc.png", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"};
    assert(verify(abc, 3, file));
    const uint8_t abd[] = {'a', 'b', 'd'};
    assert(!verify(abd, 3, file));
    assert(!verify(abc, 2, file));
}

void test_png() {
    // 2x2 RGB: row 0 unfiltered, row 1 with the Sub filter.
    const std::vector<uint8_t> lines = {0, 10, 20, 30, 40, 50, 60, 1, 5, 6, 7, 1, 1, 1};
    const std::vector<uint8_t> png = rgb_png(2, 2, lines);
    std::vector<uint8_t> rgba;
    int w = 0, h = 0;
    assert(png_decode::rgba(png.data(), png.size(), rgba, w, h) && w == 2 && h == 2);
    const std::vector<uint8_t> want = {10, 20, 30, 255, 40, 50, 60, 255, 5, 6, 7, 255, 6, 7, 8, 255};
    assert(rgba == want);
    std::vector<uint8_t> bad = png;
    bad[1] = 'X';
    assert(!png_decode::rgba(bad.data(), bad.size(), rgba, w, h));
}

void test_prepare() {
    // A glyph 5 texels wide in an 8-texel stride: a 64 x 64 mask allowing
    // ink up to (5 + 1) * 8 = 48 columns.
    const GlyphInfo glyph{5, 8, 2, 0};
    std::vector<uint8_t> rgba(64 * 64 * 4, 0);
    for (int y = 4; y < 60; y++) {
        for (int x = 2; x < 40; x++) rgba[(y * 64 + x) * 4] = 200;  // red channel only
    }
    Mask m;
    assert(prepare(rgba, 64, 64, glyph, m));
    assert(m.width == 64 && m.height == 64 && m.ink_width == 40);
    assert(m.alpha[10 * 64 + 2] == 200 && m.alpha[10 * 64 + 1] == 0);

    // Ink past 48 columns is fitted into them, not cut.
    for (int y = 4; y < 60; y++) {
        for (int x = 40; x < 60; x++) rgba[(y * 64 + x) * 4] = 200;
    }
    assert(prepare(rgba, 64, 64, glyph, m));
    assert(m.ink_width <= 48 && m.ink_width >= 46);

    std::vector<uint8_t> empty(64 * 64 * 4, 0);
    assert(!prepare(empty, 64, 64, glyph, m));
}

void test_resize() {
    const std::vector<uint8_t> flat(40 * 30, 180);
    const std::vector<uint8_t> same = resize_lanczos(flat, 40, 30, 40, 30);
    assert(same == flat);
    const std::vector<uint8_t> small = resize_lanczos(flat, 40, 30, 25, 17);
    assert(small.size() == 25u * 17u);
    for (uint8_t v : small) assert(v == 180);
}

void test_trim() {
    // A is 2 texels wide with ink to column 28 (past its 16); B inks columns 0..7.
    Mask a = mask_with_ink(64, 64, 0, 28), b = mask_with_ink(64, 64, 0, 8);
    std::vector<int32_t> kerning(169, 0);
    kerning[1 * 13 + 2] = 1;  // A (kerning index 1) before B (2) sits one texel closer
    std::vector<Prepared> font = {{GlyphInfo{2, 8, 2, 1}, &a}, {GlyphInfo{6, 8, 2, 2}, &b}};
    trim_overhangs(font, kerning.data());
    assert(a.ink_width == 16);  // B starts at A's column (2 + 1 - 1) * 8 = 16
    assert(a.alpha[20 * 64 + 15] == 255 && a.alpha[20 * 64 + 16] == 0);
    assert(b.ink_width == 8);

    a = mask_with_ink(64, 64, 0, 28);
    kerning[1 * 13 + 2] = 0;  // B now starts at column 24
    trim_overhangs(font, kerning.data());
    assert(a.ink_width == 24);
}

void test_draw_extent() {
    const Mask wide = mask_with_ink(64, 64, 0, 20);
    const enhanced_fonts::Drawn glyph{&wide, 2, 8, 8};
    assert(enhanced_fonts::draw_extent(glyph, 8, 1024, 0) == 10);  // a whole glyph grows to its ink
    assert(enhanced_fonts::draw_extent(glyph, 5, 1024, 0) == 5);   // a partial span does not
    assert(enhanced_fonts::draw_extent(glyph, 8, 1024, 32) == 8);  // nor one starting past texel 0
    const Mask narrow = mask_with_ink(64, 64, 0, 12);
    const enhanced_fonts::Drawn inside{&narrow, 2, 8, 8};
    assert(enhanced_fonts::draw_extent(inside, 8, 1024, 0) == 8);
}

void test_loader_refuses_wrong_files() {
    size_t count = 0;
    const PackFile *files = pack_files(count);
    const auto dir = std::filesystem::temp_directory_path() / "enhanced_fonts_loader_test";
    std::filesystem::create_directories(dir);
    FILE *f = std::fopen((dir / file_name(files[0])).string().c_str(), "wb");
    assert(f != nullptr);
    std::fputs("not the pinned file", f);
    std::fclose(f);
    static unsigned char pixels[64];
    std::vector<int> kerning(169, 0);
    portEnhancedFontsGlyph(files[0].font, files[0].ch, pixels, 2, 8, 2, 0, kerning.data());
    enhanced_fonts::set_directory(dir.string());
    assert(enhanced_fonts::installed_count() == 0);
    enhanced_fonts::Drawn d{};
    assert(!enhanced_fonts::find(pixels, d));
    std::filesystem::remove_all(dir);
}

}  // namespace

int main() {
    test_pinned_list();
    test_verify();
    test_png();
    test_prepare();
    test_resize();
    test_trim();
    test_draw_extent();
    test_loader_refuses_wrong_files();
    std::printf("enhanced fonts: ok\n");
    return 0;
}
