/* Player-installed HD texture packs (FORK-053): pack index and image reads.
 * See hd_texture_pack.h. */
#include "hd_texture_pack.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <system_error>

#include "png_decode.h"
#include "third_party/picojson/picojson.h"

extern "C" {
#include "third_party/puff/puff.h"
}

namespace hd_textures {
namespace {

constexpr uint32_t kMaxBytes = 80u * 1024 * 1024;  /* one texture's file, packed or not */
constexpr uint32_t kMaxSide = 4096;

uint32_t le16(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8; }
uint32_t le32(const uint8_t *p) { return le16(p) | le16(p + 2) << 16; }
uint64_t le64(const uint8_t *p) { return uint64_t(le32(p)) | uint64_t(le32(p + 4)) << 32; }

/* Plain ASCII checks: <cctype>'s toupper would clash with the game's own
 * (src/str.c) at link time on Windows. */
char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; }

bool hex(const std::string &s, size_t at, size_t count, uint32_t &out) {
    if (at + count > s.size()) return false;
    out = 0;
    for (size_t i = at; i < at + count; i++) {
        const char c = lower(s[i]);
        if (c >= '0' && c <= '9') out = out << 4 | uint32_t(c - '0');
        else if (c >= 'a' && c <= 'f') out = out << 4 | uint32_t(c - 'a' + 10);
        else return false;
    }
    return true;
}

bool lower_ends_with(const std::string &s, const char *suffix) {
    const size_t n = std::strlen(suffix);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; i++) {
        if (lower(s[s.size() - n + i]) != suffix[i]) return false;
    }
    return true;
}

bool read_at(std::ifstream &in, uint64_t offset, uint8_t *out, size_t size) {
    in.clear();
    in.seekg(std::streamoff(offset));
    in.read(reinterpret_cast<char *>(out), std::streamsize(size));
    return bool(in) && size_t(in.gcount()) == size;
}

bool key_of(const picojson::value &value, Key &key) {
    if (!value.is<picojson::array>()) return false;
    const auto &a = value.get<picojson::array>();
    if (a.size() != 4) return false;
    uint32_t v[4];
    for (size_t i = 0; i < 4; i++) {
        if (!a[i].is<double>()) return false;
        v[i] = uint32_t(a[i].get<double>());
    }
    key = Key{v[0], v[1], v[2], v[3]};
    return true;
}

}  // namespace

bool parse_name(const std::string &path, Key &key) {
    const size_t slash = path.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    static const char prefix[] = "GOLDENEYE#";
    if (name.size() < 22 || name.compare(0, 10, prefix) != 0) return false;
    uint32_t texture = 0, format = 0, size = 0, palette = 0;
    if (!hex(name, 10, 8, texture) || name[18] != '#' || !hex(name, 19, 1, format) || name[20] != '#'
        || !hex(name, 21, 1, size) || format > 4 || size > 3) {
        return false;
    }
    std::string rest = name.substr(22);
    if (rest.size() >= 2 && rest[0] == '#' && rest[1] == '$') {
        rest = rest.substr(2);
    } else if (rest.size() >= 9 && rest[0] == '#') {
        if (!hex(rest, 1, 8, palette)) return false;
        rest = rest.substr(9);
    }
    if (rest != "_all.png" && rest != "_ciByRGBA.png") return false;
    key = Key{texture, palette, format, size};
    return true;
}

namespace {
/* One axis of resample: for each output texel, its source texels and weights. */
struct Taps {
    std::vector<unsigned> first, count;
    std::vector<float> weight;
};

Taps taps(unsigned from, unsigned to) {
    Taps out;
    const float ratio = float(from) / float(to);
    const float radius = ratio > 1.0f ? ratio : 1.0f;
    for (unsigned i = 0; i < to; i++) {
        const float centre = (float(i) + 0.5f) * ratio - 0.5f;
        int lo = int(centre - radius) + 1, hi = int(centre + radius);
        if (lo < 0) lo = 0;
        if (hi > int(from) - 1) hi = int(from) - 1;
        if (hi < lo) lo = hi = int(centre < 0 ? 0 : (centre > float(from - 1) ? float(from - 1) : centre + 0.5f));
        float sum = 0;
        const size_t at = out.weight.size();
        for (int k = lo; k <= hi; k++) {
            const float d = (float(k) - centre) / radius;
            const float w = 1.0f - (d < 0 ? -d : d);
            out.weight.push_back(w > 0 ? w : 0);
            sum += w > 0 ? w : 0;
        }
        if (sum <= 0) {
            out.weight[at] = sum = 1.0f;
        }
        for (size_t k = at; k < out.weight.size(); k++) out.weight[k] /= sum;
        out.first.push_back(unsigned(lo));
        out.count.push_back(unsigned(hi - lo + 1));
    }
    return out;
}
}  // namespace

Image resample(const Image &src, unsigned width, unsigned height) {
    Image out;
    if (src.width == 0 || src.height == 0 || width == 0 || height == 0
        || src.rgba.size() < size_t(src.width) * src.height * 4) {
        return out;
    }
    const Taps tx = taps(src.width, width), ty = taps(src.height, height);
    /* Horizontal pass, premultiplying as it reads: the only float buffer is
     * output width x source height, so a custom pack's 4096 px master
     * reduced to the HD tier needs 16 MB, not a 128 MB copy of itself. */
    std::vector<float> rows(size_t(width) * src.height * 4, 0.0f);
    for (unsigned y = 0; y < src.height; y++) {
        size_t w = 0;
        for (unsigned x = 0; x < width; x++) {
            float *dst = &rows[(size_t(y) * width + x) * 4];
            for (unsigned k = 0; k < tx.count[x]; k++, w++) {
                const uint8_t *s = &src.rgba[(size_t(y) * src.width + tx.first[x] + k) * 4];
                const float a = s[3] / 255.0f, weight = tx.weight[w] / 255.0f;
                for (int c = 0; c < 3; c++) dst[c] += s[c] * a * weight;
                dst[3] += a * tx.weight[w];
            }
        }
    }
    out.width = width;
    out.height = height;
    out.rgba.assign(size_t(width) * height * 4, 0);
    size_t w = 0;
    for (unsigned y = 0; y < height; y++) {
        const size_t wy = w;
        for (unsigned x = 0; x < width; x++) {
            float px[4] = {0, 0, 0, 0};
            w = wy;
            for (unsigned k = 0; k < ty.count[y]; k++, w++) {
                const float *s = &rows[(size_t(ty.first[y] + k) * width + x) * 4];
                for (int c = 0; c < 4; c++) px[c] += s[c] * ty.weight[w];
            }
            uint8_t *dst = &out.rgba[(size_t(y) * width + x) * 4];
            const float a = px[3];
            for (int c = 0; c < 3; c++) {
                const float v = a > 0 ? px[c] / a : 0.0f;
                dst[c] = uint8_t(v <= 0 ? 0 : v >= 1 ? 255 : v * 255.0f + 0.5f);
            }
            dst[3] = uint8_t(a <= 0 ? 0 : a >= 1 ? 255 : a * 255.0f + 0.5f);
        }
    }
    return out;
}

bool parse_scope(const std::vector<std::string> &sources, const std::vector<std::string> &aliases, Scope &scope) {
    for (const auto &text : sources) {
        picojson::value doc;
        if (!picojson::parse(doc, text).empty() || !doc.is<picojson::object>()) return false;
        const auto &textures = doc.get<picojson::object>().find("textures");
        if (textures == doc.get<picojson::object>().end() || !textures->second.is<picojson::array>()) return false;
        for (const auto &name : textures->second.get<picojson::array>()) {
            Key key;
            if (name.is<std::string>() && parse_name(name.get<std::string>(), key)) scope.keys.insert(key);
        }
    }
    for (const auto &text : aliases) {
        picojson::value list;
        if (!picojson::parse(list, text).empty() || !list.is<picojson::object>()) return false;
        const auto &doc = list.get<picojson::object>();
        const auto items = doc.find("aliases"), remaps = doc.find("remaps");
        const bool has_aliases = items != doc.end() && items->second.is<picojson::array>();
        const bool has_remaps = remaps != doc.end() && remaps->second.is<picojson::array>();
        if (!has_aliases && !has_remaps) return false;
        static const picojson::array none;
        for (const auto &item : has_remaps ? remaps->second.get<picojson::array>() : none) {
            if (!item.is<picojson::object>()) continue;
            const auto &o = item.get<picojson::object>();
            const auto pack = o.find("pack_key"), native = o.find("native_key");
            const auto keep = o.find("keep_rows"), of = o.find("of_rows"), masked = o.find("masked");
            Key key;
            Remap remap;
            if (pack == o.end() || native == o.end() || !key_of(pack->second, key) || !key_of(native->second, remap.native)) continue;
            if (keep != o.end() && of != o.end() && keep->second.is<double>() && of->second.is<double>()) {
                remap.keep_rows = unsigned(keep->second.get<double>());
                remap.of_rows = unsigned(of->second.get<double>());
                if (!remap.keep_rows || remap.keep_rows > remap.of_rows) continue;
            }
            remap.masked = masked != o.end() && masked->second.is<bool>() && masked->second.get<bool>();
            scope.keys.insert(key);
            scope.remaps.emplace(key, remap);
        }
        for (const auto &item : has_aliases ? items->second.get<picojson::array>() : none) {
            if (!item.is<picojson::object>()) continue;
            const auto &o = item.get<picojson::object>();
            const auto pack = o.find("pack_key"), native = o.find("native_palette");
            Key key;
            if (pack == o.end() || native == o.end() || !native->second.is<double>() || !key_of(pack->second, key)) continue;
            scope.aliases.emplace(key, uint32_t(native->second.get<double>()));
        }
    }
    return !scope.keys.empty();
}

void Pack::add(const Key &key, const Entry &entry, const Scope &scope) {
    const Key any{key.texture, 0, key.format, key.size};
    if (!scope.all && !scope.keys.count(key) && !scope.keys.count(any)) return;
    Entry found = entry;
    found.custom = scope.all;
    entries_.emplace(key, found);  /* the first copy of a key wins */
    const auto range = scope.aliases.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
        entries_.emplace(Key{key.texture, it->second, key.format, key.size}, found);
    }
    const auto remaps = scope.remaps.equal_range(key);
    for (auto it = remaps.first; it != remaps.second; ++it) {
        Entry remapped = found;
        remapped.keep_rows = it->second.keep_rows;
        remapped.of_rows = it->second.of_rows;
        remapped.masked = it->second.masked;
        entries_.emplace(it->second.native, remapped);
    }
}

void Pack::index_zip(const std::filesystem::path &zip, const Scope &scope) {
    std::ifstream in(zip, std::ios::binary);
    if (!in) return;
    in.seekg(0, std::ios::end);
    const uint64_t length = uint64_t(in.tellg());
    if (length < 22) return;
    /* The end-of-central-directory record sits in the last 64 KiB + 22 bytes. */
    const size_t tail = size_t(std::min<uint64_t>(length, 65557));
    std::vector<uint8_t> end(tail);
    if (!read_at(in, length - tail, end.data(), tail)) return;
    size_t eocd = std::string::npos;
    for (size_t i = tail - 22 + 1; i-- > 0;) {
        if (le32(&end[i]) == 0x06054b50u) { eocd = i; break; }
    }
    if (eocd == std::string::npos) return;
    uint64_t count = le16(&end[eocd + 10]), cd_size = le32(&end[eocd + 12]), cd_offset = le32(&end[eocd + 16]);
    if (count == 0xFFFFu || cd_size == 0xFFFFFFFFu || cd_offset == 0xFFFFFFFFu) {
        /* ZIP64: the locator just before the record points at the 64-bit one. */
        uint8_t locator[20], record[56];
        const uint64_t at = length - tail + eocd;
        if (at < 20 || !read_at(in, at - 20, locator, 20) || le32(locator) != 0x07064b50u) return;
        if (!read_at(in, le64(locator + 8), record, 56) || le32(record) != 0x06064b50u) return;
        count = le64(record + 32);
        cd_size = le64(record + 40);
        cd_offset = le64(record + 48);
    }
    if (cd_size > (64u << 20) || cd_offset + cd_size > length) return;  /* a pack's directory is far smaller */
    std::vector<uint8_t> cd(static_cast<size_t>(cd_size));
    if (!read_at(in, cd_offset, cd.data(), cd.size())) return;
    size_t at = 0;
    for (uint64_t n = 0; n < count && at + 46 <= cd.size(); n++) {
        const uint8_t *e = &cd[at];
        if (le32(e) != 0x02014b50u) return;
        const uint32_t flags = le16(e + 8), method = le16(e + 10);
        uint64_t packed = le32(e + 20), size = le32(e + 24), header = le32(e + 42);
        const uint32_t name_len = le16(e + 28), extra_len = le16(e + 30), comment_len = le16(e + 32);
        if (at + 46 + name_len + extra_len > cd.size()) return;
        const std::string name(reinterpret_cast<const char *>(e + 46), name_len);
        /* ZIP64 extra field: whichever of size, packed size and header offset
         * are 0xFFFFFFFF follow in that order. */
        for (size_t x = 0; x + 4 <= extra_len;) {
            const uint8_t *field = e + 46 + name_len + x;
            const uint32_t id = le16(field), len = le16(field + 2);
            if (x + 4 + len > extra_len) break;
            if (id == 0x0001) {
                size_t v = 4;
                if (size == 0xFFFFFFFFu && v + 8 <= 4 + len) { size = le64(field + v); v += 8; }
                if (packed == 0xFFFFFFFFu && v + 8 <= 4 + len) { packed = le64(field + v); v += 8; }
                if (header == 0xFFFFFFFFu && v + 8 <= 4 + len) { header = le64(field + v); v += 8; }
            }
            x += 4 + len;
        }
        at += 46 + name_len + extra_len + comment_len;
        Key key;
        if ((flags & 1u) || (method != 0 && method != 8) || size > kMaxBytes || packed > kMaxBytes
            || header >= length || !parse_name(name, key)) {
            continue;  /* encrypted, unsupported compression, oversized or not a texture */
        }
        Entry entry;
        entry.file = zip;
        entry.zipped = true;
        entry.header = header;
        entry.method = method;
        entry.packed = uint32_t(packed);
        entry.size = uint32_t(size);
        add(key, entry, scope);
    }
}

void Pack::index_png(const std::filesystem::path &png, const Scope &scope) {
    Key key;
    std::error_code size_error;
    const auto bytes = std::filesystem::file_size(png, size_error);
    if (size_error || bytes == 0 || bytes > kMaxBytes || !parse_name(png.filename().string(), key)) return;
    Entry entry;
    entry.file = png;
    entry.size = uint32_t(bytes);
    add(key, entry, scope);
}

size_t Pack::index(const std::filesystem::path &root, const Scope &scope,
                   const std::function<bool(const std::filesystem::path &)> &curated) {
    entries_.clear();
    custom_ = 0;
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) return 0;
    std::vector<std::filesystem::path> custom_pngs, custom_zips, curated_pngs, curated_zips;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (auto it = std::filesystem::recursive_directory_iterator(root, options, error);
         !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
        if (!it->is_regular_file(error)) continue;
        const std::string name = it->path().filename().string();
        const bool is_curated = curated && curated(it->path());
        if (lower_ends_with(name, ".zip")) (is_curated ? curated_zips : custom_zips).push_back(it->path());
        else if (lower_ends_with(name, ".png")) (is_curated ? curated_pngs : custom_pngs).push_back(it->path());
    }
    /* Sorted so the same files always give the same choice between copies:
     * custom packs before the curated one, and loose PNGs (a pack being made)
     * before ZIPs. */
    for (auto *list : {&custom_pngs, &custom_zips, &curated_pngs, &curated_zips}) std::sort(list->begin(), list->end());
    Scope everything = scope;
    everything.all = true;
    for (const auto &png : custom_pngs) index_png(png, everything);
    for (const auto &zip : custom_zips) index_zip(zip, everything);
    custom_ = entries_.size();
    for (const auto &png : curated_pngs) index_png(png, scope);
    for (const auto &zip : curated_zips) index_zip(zip, scope);
    return entries_.size();
}

const Entry *Pack::find(const Key &key) const {
    auto it = entries_.find(key);
    if (it == entries_.end() && key.palette) it = entries_.find(Key{key.texture, 0, key.format, key.size});
    return it == entries_.end() ? nullptr : &it->second;
}

const Entry *Pack::find(const Tile &tile, Key &key) const {
    const Entry *entry = find(key);
    if (entry != nullptr || !tile.restoreSwizzle) return entry;
    Tile natural = tile;
    natural.restoreSwizzle = false;
    Key other;
    if (!identify(natural, other) || (entry = find(other)) == nullptr) return nullptr;
    key = other;
    return entry;
}

bool Pack::read(const Entry &entry, Image &image) {
    image = Image{};
    std::ifstream in(entry.file, std::ios::binary);
    if (!in || entry.size == 0 || entry.size > kMaxBytes) return false;
    std::vector<uint8_t> png(entry.size);
    if (!entry.zipped) {
        if (!read_at(in, 0, png.data(), png.size())) return false;
    } else {
        uint8_t local[30];
        if (!read_at(in, entry.header, local, sizeof local) || le32(local) != 0x04034b50u) return false;
        const uint64_t data = entry.header + 30 + le16(local + 26) + le16(local + 28);
        std::vector<uint8_t> packed(entry.packed);
        if (!read_at(in, data, packed.data(), packed.size())) return false;
        if (entry.method == 0) {
            if (entry.packed != entry.size) return false;
            png.swap(packed);
        } else {
            unsigned long out_len = png.size(), in_len = packed.size();
            if (puff(png.data(), &out_len, packed.data(), &in_len) != 0 || out_len != png.size()) return false;
        }
    }
    int width = 0, height = 0;
    if (!png_decode::rgba(png.data(), png.size(), image.rgba, width, height)) return false;
    if (width <= 0 || height <= 0 || unsigned(width) > kMaxSide || unsigned(height) > kMaxSide) {
        image.rgba.clear();
        return false;
    }
    image.width = unsigned(width);
    image.height = unsigned(height);
    if (entry.keep_rows && entry.of_rows && entry.keep_rows < entry.of_rows) {
        image.height = std::max(1u, image.height * entry.keep_rows / entry.of_rows);
        image.rgba.resize(size_t(image.width) * image.height * 4);
    }
    return true;
}
}  // namespace hd_textures
