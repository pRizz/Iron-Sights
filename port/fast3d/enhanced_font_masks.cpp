/* Enhanced font masks (FORK-052, FONT-2). See enhanced_font_masks.h. */
#include "enhanced_font_masks.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "third_party/picosha2/picosha2.h"

namespace enhanced_font_masks {
namespace {

const PackFile kFiles[] = {
#include "enhanced_fonts_list.inc"
};

/* Pillow's Resample.c: Lanczos-3, 8-bit fixed point with 22 fraction bits. */
constexpr int kPrecision = 22;
constexpr double kPi = 3.14159265358979323846;

double sinc(double x) {
    if (x == 0.0) return 1.0;
    x *= kPi;
    return std::sin(x) / x;
}

double lanczos(double x) {
    return (x >= -3.0 && x < 3.0) ? sinc(x) * sinc(x / 3.0) : 0.0;
}

uint8_t clip8(int32_t v) {
    if (v >= (1 << kPrecision << 8)) return 255;
    if (v <= 0) return 0;
    return uint8_t(v >> kPrecision);
}

/* One axis's taps: for each output sample, its first input and fixed-point
 * weights, as Pillow's precompute_coeffs and normalize_coeffs_8bpc. */
struct Taps {
    std::vector<int> first, count;
    std::vector<int32_t> weights;  /* ksize per output sample */
    int ksize = 0;
};

Taps taps(int in_size, int out_size) {
    Taps t;
    const double scale = double(in_size) / out_size;
    const double filterscale = std::max(scale, 1.0);
    const double support = 3.0 * filterscale;
    t.ksize = int(std::ceil(support)) * 2 + 1;
    t.first.resize(out_size);
    t.count.resize(out_size);
    t.weights.assign(size_t(out_size) * t.ksize, 0);
    std::vector<double> k(t.ksize);
    for (int xx = 0; xx < out_size; xx++) {
        const double center = (xx + 0.5) * scale;
        const int xmin = std::max(int(center - support + 0.5), 0);
        const int xmax = std::min(int(center + support + 0.5), in_size) - xmin;
        double ww = 0.0;
        for (int x = 0; x < xmax; x++) {
            k[x] = lanczos((x + xmin - center + 0.5) / filterscale);
            ww += k[x];
        }
        for (int x = 0; x < xmax; x++) {
            const double w = ww != 0.0 ? k[x] / ww : k[x];
            t.weights[size_t(xx) * t.ksize + x] = int32_t(w < 0 ? -0.5 + w * (1 << kPrecision) : 0.5 + w * (1 << kPrecision));
        }
        t.first[xx] = xmin;
        t.count[xx] = xmax;
    }
    return t;
}

/* The bounding box of non-zero samples, as Pillow's getbbox; false when empty. */
bool bbox(const std::vector<uint8_t> &a, int w, int h, int &right, int &bottom) {
    right = bottom = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (a[size_t(y) * w + x] != 0) {
                right = std::max(right, x + 1);
                bottom = y + 1;
            }
        }
    }
    return right > 0;
}

/* The smallest column at or past native where a's ink (at least half) meets
 * b's drawn advance columns later on the same baseline-aligned row, or limit. */
int overhang_limit(const Prepared &a, const Prepared &b, int advance, int limit) {
    const int native = a.glyph.width * kScale;
    const int a_base = a.glyph.baseline * kScale, b_base = b.glyph.baseline * kScale;
    for (int y = 0; y < a.mask->height; y++) {
        const int yb = y + a_base - b_base;
        if (yb < 0 || yb >= b.mask->height) continue;
        const uint8_t *ra = &a.mask->alpha[size_t(y) * a.mask->width];
        const uint8_t *rb = &b.mask->alpha[size_t(yb) * b.mask->width];
        for (int x = native; x < std::min(limit, a.mask->width); x++) {
            const int xb = x - advance;
            if (ra[x] >= 128 && xb >= 0 && xb < b.mask->width && rb[xb] >= 128) {
                limit = x;
                break;
            }
        }
    }
    return limit;
}

}  // namespace

const PackFile *pack_files(size_t &count) {
    count = sizeof(kFiles) / sizeof(kFiles[0]);
    return kFiles;
}

const char *file_name(const PackFile &file) {
    const char *slash = std::strrchr(file.path, '/');
    return slash ? slash + 1 : file.path;
}

bool verify(const uint8_t *data, size_t size, const PackFile &file) {
    if (data == nullptr || size != file.size) return false;
    return picosha2::hash256_hex_string(data, data + size) == file.sha256;
}

std::vector<uint8_t> resize_lanczos(const std::vector<uint8_t> &src, int sw, int sh, int dw, int dh) {
    std::vector<uint8_t> cur = src;
    int w = sw;
    if (dw != sw) {  /* Pillow resamples horizontally first */
        const Taps t = taps(sw, dw);
        std::vector<uint8_t> out(size_t(dw) * sh);
        for (int y = 0; y < sh; y++) {
            for (int x = 0; x < dw; x++) {
                int32_t ss = 1 << (kPrecision - 1);
                for (int k = 0; k < t.count[x]; k++) ss += cur[size_t(y) * w + t.first[x] + k] * t.weights[size_t(x) * t.ksize + k];
                out[size_t(y) * dw + x] = clip8(ss);
            }
        }
        cur.swap(out);
        w = dw;
    }
    if (dh != sh) {
        const Taps t = taps(sh, dh);
        std::vector<uint8_t> out(size_t(w) * dh);
        for (int y = 0; y < dh; y++) {
            for (int x = 0; x < w; x++) {
                int32_t ss = 1 << (kPrecision - 1);
                for (int k = 0; k < t.count[y]; k++) ss += cur[size_t(t.first[y] + k) * w + x] * t.weights[size_t(y) * t.ksize + k];
                out[size_t(y) * w + x] = clip8(ss);
            }
        }
        cur.swap(out);
    }
    return cur;
}

bool prepare(const std::vector<uint8_t> &rgba, int width, int height, const GlyphInfo &glyph, Mask &out) {
    if (width <= 0 || height <= 0 || glyph.width <= 0 || glyph.height <= 0 || rgba.size() != size_t(width) * height * 4) {
        return false;
    }
    std::vector<uint8_t> red(size_t(width) * height);
    for (size_t i = 0; i < red.size(); i++) red[i] = rgba[i * 4];
    int box_right = 0, box_bottom = 0;
    bbox(red, width, height, box_right, box_bottom);
    out.width = glyph.stride() * kScale;
    out.height = glyph.height * kScale;
    const int limit = std::min(out.width, (glyph.width + 1) * kScale);
    const int crop_w = std::min(width, std::max(limit, box_right));
    const int crop_h = std::min(height, std::max(out.height, box_bottom));
    const int fit_w = std::min(crop_w, limit), fit_h = std::min(crop_h, out.height);
    std::vector<uint8_t> crop(size_t(crop_w) * crop_h);
    for (int y = 0; y < crop_h; y++) std::memcpy(&crop[size_t(y) * crop_w], &red[size_t(y) * width], size_t(crop_w));
    if (fit_w != crop_w || fit_h != crop_h) crop = resize_lanczos(crop, crop_w, crop_h, fit_w, fit_h);
    out.alpha.assign(size_t(out.width) * out.height, 0);
    for (int y = 0; y < fit_h; y++) std::memcpy(&out.alpha[size_t(y) * out.width], &crop[size_t(y) * fit_w], size_t(fit_w));
    int bottom = 0;
    return bbox(out.alpha, out.width, out.height, out.ink_width, bottom);
}

void trim_overhangs(const std::vector<Prepared> &font, const int32_t *kerning) {
    std::vector<int> limits;
    for (const Prepared &a : font) {
        int limit = a.mask->ink_width;
        for (const Prepared &b : font) {
            const int32_t kern = kerning[a.glyph.kerningindex * 13 + b.glyph.kerningindex];
            limit = overhang_limit(a, b, (a.glyph.width + 1 - kern) * kScale, limit);
        }
        limits.push_back(limit);
    }
    for (size_t i = 0; i < font.size(); i++) {
        Mask &mask = *font[i].mask;
        if (limits[i] >= mask.ink_width) continue;
        for (int y = 0; y < mask.height; y++) {
            std::fill(mask.alpha.begin() + size_t(y) * mask.width + limits[i], mask.alpha.begin() + size_t(y + 1) * mask.width, 0);
        }
        int bottom = 0;
        bbox(mask.alpha, mask.width, mask.height, mask.ink_width, bottom);
    }
}

}  // namespace enhanced_font_masks
