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

bool parse_scope(const std::string &sources, const std::vector<std::string> &aliases, Scope &scope) {
    picojson::value doc;
    if (!picojson::parse(doc, sources).empty() || !doc.is<picojson::object>()) return false;
    const auto &textures = doc.get<picojson::object>().find("textures");
    if (textures == doc.get<picojson::object>().end() || !textures->second.is<picojson::array>()) return false;
    for (const auto &name : textures->second.get<picojson::array>()) {
        Key key;
        if (name.is<std::string>() && parse_name(name.get<std::string>(), key)) scope.keys.insert(key);
    }
    for (const auto &text : aliases) {
        picojson::value list;
        if (!picojson::parse(list, text).empty() || !list.is<picojson::object>()) return false;
        const auto &items = list.get<picojson::object>().find("aliases");
        if (items == list.get<picojson::object>().end() || !items->second.is<picojson::array>()) return false;
        for (const auto &item : items->second.get<picojson::array>()) {
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
    if (!scope.keys.count(key) && !scope.keys.count(any)) return;
    entries_.emplace(key, entry);  /* the first copy of a key wins */
    const auto range = scope.aliases.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
        entries_.emplace(Key{key.texture, it->second, key.format, key.size}, entry);
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
    const uint32_t count = le16(&end[eocd + 10]), cd_size = le32(&end[eocd + 12]), cd_offset = le32(&end[eocd + 16]);
    if (cd_offset == 0xFFFFFFFFu || uint64_t(cd_offset) + cd_size > length) return;  /* ZIP64: not a pack's size */
    std::vector<uint8_t> cd(cd_size);
    if (!read_at(in, cd_offset, cd.data(), cd_size)) return;
    size_t at = 0;
    for (uint32_t n = 0; n < count && at + 46 <= cd.size(); n++) {
        const uint8_t *e = &cd[at];
        if (le32(e) != 0x02014b50u) return;
        const uint32_t flags = le16(e + 8), method = le16(e + 10), packed = le32(e + 20), size = le32(e + 24);
        const uint32_t name_len = le16(e + 28), extra_len = le16(e + 30), comment_len = le16(e + 32);
        const uint32_t header = le32(e + 42);
        if (at + 46 + name_len > cd.size()) return;
        const std::string name(reinterpret_cast<const char *>(e + 46), name_len);
        at += 46 + name_len + extra_len + comment_len;
        Key key;
        if ((flags & 1u) || (method != 0 && method != 8) || size > kMaxBytes || packed > kMaxBytes
            || !parse_name(name, key)) {
            continue;  /* encrypted, unsupported compression, oversized or not a texture */
        }
        Entry entry;
        entry.file = zip;
        entry.zipped = true;
        entry.header = header;
        entry.method = method;
        entry.packed = packed;
        entry.size = size;
        add(key, entry, scope);
    }
}

size_t Pack::index(const std::filesystem::path &root, const Scope &scope) {
    entries_.clear();
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) return 0;
    std::vector<std::filesystem::path> zips, pngs;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (auto it = std::filesystem::recursive_directory_iterator(root, options, error);
         !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
        if (!it->is_regular_file(error)) continue;
        const std::string name = it->path().filename().string();
        if (lower_ends_with(name, ".zip")) zips.push_back(it->path());
        else if (lower_ends_with(name, ".png")) pngs.push_back(it->path());
    }
    /* Sorted so the same files always give the same choice between copies:
     * extracted PNGs first, then ZIPs. */
    std::sort(pngs.begin(), pngs.end());
    std::sort(zips.begin(), zips.end());
    for (const auto &png : pngs) {
        Key key;
        std::error_code size_error;
        const auto bytes = std::filesystem::file_size(png, size_error);
        if (size_error || bytes == 0 || bytes > kMaxBytes || !parse_name(png.filename().string(), key)) continue;
        Entry entry;
        entry.file = png;
        entry.size = uint32_t(bytes);
        add(key, entry, scope);
    }
    for (const auto &zip : zips) index_zip(zip, scope);
    return entries_.size();
}

const Entry *Pack::find(const Key &key) const {
    auto it = entries_.find(key);
    if (it == entries_.end() && key.palette) it = entries_.find(Key{key.texture, 0, key.format, key.size});
    return it == entries_.end() ? nullptr : &it->second;
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
    return true;
}
}  // namespace hd_textures
