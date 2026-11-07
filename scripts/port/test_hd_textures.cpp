// ROM-free checks for player-installed HD texture packs (FORK-053): native
// tile identity, Rice file names, the scope and the pack index over a pack
// fixture built by make_hd_texture_fixture.py (a ZIP with deflated and stored
// entries, an extracted PNG, an out-of-scope name and a corrupt image, plus a
// custom pack: a ZIP64 archive and a loose PNG).
#include <array>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>

#include "port/fast3d/hd_texture_pack.h"

using namespace hd_textures;

static Tile source(std::array<uint8_t, 32> &data) {
    for (unsigned i = 0; i < data.size(); ++i) data[i] = uint8_t(i);
    return {data.data(), data.size(), 8, 4, 8, 4, 1, nullptr, 0, false};
}

static void identity() {
    // Arrange: an independently known Rice I8 fingerprint, including row order.
    std::array<uint8_t, 32> data;
    Tile tile = source(data);
    Key key;
    // Act / Assert
    assert(identify(tile, key) && key.texture == 0x8d8e8f90 && key.palette == 0);
    tile.restoreSwizzle = true;
    std::array<uint8_t, 32> original;
    for (unsigned y = 0; y < 4; ++y)
        for (unsigned x = 0; x < 8; ++x) original[y * 8 + x] = data[y * 8 + ((y & 1) ? x ^ 4 : x)];
    Tile swapped{original.data(), original.size(), 8, 4, 8, 4, 1, nullptr, 0, false};
    Key reference;
    assert(identify(swapped, reference) && identify(tile, key) && key == reference);
    tile.bytes--; assert(!identify(tile, key)); tile.bytes++;
    tile.stride = 7; assert(!identify(tile, key)); tile.stride = 8;
    tile.width = 3; assert(!identify(tile, key));
}

static void native_words() {
    // Arrange: equivalent N64 bytes stored as this port's native u16s and u32s.
    std::array<uint16_t, 16> data;
    for (unsigned i = 0; i < data.size(); ++i) data[i] = uint16_t((i * 2 << 8) | (i * 2 + 1));
    Tile tile{reinterpret_cast<uint8_t *>(data.data()), 32, 4, 4, 8, 0, 2, nullptr, 0, false};
    Key key;
    // Act / Assert
    assert(identify(tile, key) && key.texture == 0x8d8e8f90);
    std::array<uint32_t, 8> words;
    for (unsigned i = 0; i < words.size(); ++i) words[i] = (i * 4 << 24) | ((i * 4 + 1) << 16) | ((i * 4 + 2) << 8) | (i * 4 + 3);
    Tile rgba32{reinterpret_cast<uint8_t *>(words.data()), 32, 2, 4, 8, 0, 3, nullptr, 0, false};
    assert(identify(rgba32, key) && key.texture == 0x8d8e8f90);
}

static void palettes() {
    // Arrange: identical indices and different palette content.
    std::array<uint8_t, 32> data;
    Tile tile = source(data);
    std::array<uint16_t, 256> palette{};
    tile.format = 2; tile.palette = palette.data(); tile.paletteCount = 256;
    Key a, b;
    // Act / Assert
    assert(identify(tile, a)); palette[30] = 0x7fff;
    assert(identify(tile, b) && a.texture == b.texture && a.palette != b.palette);
    tile.paletteCount = 31; assert(!identify(tile, a));
}

static void names() {
    Key key;
    assert(parse_name("GOLDENEYE/Areas/Dam/GOLDENEYE#03274EAB#3#1_all.png", key));
    assert((key == Key{0x03274EAB, 0, 3, 1}));
    assert(parse_name("x\\GOLDENEYE#000f7398#2#1#5A44417D_ciByRGBA.png", key));
    assert((key == Key{0x000F7398, 0x5A44417D, 2, 1}));
    assert(parse_name("GOLDENEYE#000F7398#2#1#$_ciByRGBA.png", key) && key.palette == 0);
    assert(!parse_name("GOLDENEYE#000F7398#5#1_all.png", key));      // no format 5
    assert(!parse_name("GOLDENEYE#000F739#2#1_all.png", key));       // short texture
    assert(!parse_name("GOLDENEYE#000F7398#2#1_all.jpg", key));
    assert(!parse_name("readme.txt", key));
}

static Scope fixture_scope() {
    // The fixture's keys (see make_hd_texture_fixture.py): A, C and G exact, B any palette.
    const std::string sources = R"({"textures": [
        "GOLDENEYE/Areas/A/GOLDENEYE#AAAA0001#3#1_all.png",
        "GOLDENEYE/Areas/B/GOLDENEYE#BBBB0002#2#1#$_ciByRGBA.png",
        "GOLDENEYE/Areas/C/GOLDENEYE#CCCC0003#4#1_all.png",
        "GOLDENEYE/Areas/E/GOLDENEYE#EEEE0005#3#1_all.png",
        "GOLDENEYE/Effects/G/GOLDENEYE#8D8E8F90#4#1_all.png"]})";
    const std::string aliases = R"({"aliases": [
        {"pack_key": [2863267841, 0, 3, 1], "native_palette": 305419896}]})";
    Scope scope;
    assert(parse_scope(sources, {aliases}, scope));
    assert(scope.keys.size() == 5 && scope.aliases.size() == 1);
    return scope;
}

static bool everything_downloaded(const std::filesystem::path &) { return true; }

static bool outside_player(const std::filesystem::path &file) {
    return file.generic_string().find("/player/") == std::string::npos;
}

static void pack(const std::string &root) {
    // Arrange
    const Scope scope = fixture_scope();
    Pack pack;
    // Act: every file is the downloaded pack: A (deflated in pack.zip, which
    // sorts before player/mine.zip's copy), B (stored), C (extracted PNG), E
    // (corrupt), G; D and F are out of scope. A's alias adds a native palette key.
    const size_t found = pack.index(root, scope, everything_downloaded);
    // Assert
    assert(found == 6 && pack.custom() == 0);
    Image image;
    const Entry *a = pack.find(Key{0xAAAA0001, 0, 3, 1});
    assert(a && a->zipped && !a->custom && a->method == 8 && Pack::read(*a, image));
    assert(image.width == 8 && image.height == 4 && image.rgba.size() == 8 * 4 * 4 && image.rgba[0] == 10);
    const Entry *alias = pack.find(Key{0xAAAA0001, 0x12345678, 3, 1});
    assert(alias && alias->header == a->header);
    const Entry *b = pack.find(Key{0xBBBB0002, 0xDEADBEEF, 2, 1});  // any palette
    assert(b && b->zipped && b->method == 0 && Pack::read(*b, image) && image.width == 16 && image.height == 16);
    const Entry *c = pack.find(Key{0xCCCC0003, 0, 4, 1});
    assert(c && !c->zipped && Pack::read(*c, image) && image.width == 4 && image.rgba[3] == 128);
    assert(pack.find(Key{0xDDDD0004, 0, 3, 1}) == nullptr);           // out of scope
    const Entry *e = pack.find(Key{0xEEEE0005, 0, 3, 1});
    assert(e && !Pack::read(*e, image) && image.rgba.empty());       // corrupt PNG
    assert(pack.find(Key{0xAAAA0001, 0x12345678, 3, 0}) == nullptr);  // size is part of the key
    // G's name is the identity() tile's natural row order: a swapped tile finds it second.
    std::array<uint8_t, 32> data;
    Tile tile = source(data);
    tile.restoreSwizzle = true;
    Key key, swapped;
    assert(identify(tile, key) && key.texture != 0x8d8e8f90);
    swapped = key;
    const Entry *g = pack.find(tile, key);
    assert(g && (key == Key{0x8d8e8f90, 0, 4, 1}));
    tile.restoreSwizzle = false;
    key = swapped;
    assert(pack.find(tile, key) == nullptr && key == swapped);         // natural order was the only try
    // A missing folder finds nothing.
    assert(pack.index(root + "/missing", scope, everything_downloaded) == 0 && pack.size() == 0);
}

static void custom_packs(const std::string &root) {
    // Arrange: player/ is a player's own pack; the rest is the download.
    const Scope scope = fixture_scope();
    Pack pack;
    // Act
    const size_t found = pack.index(root, scope, outside_player);
    // Assert: the custom pack's A, A's alias, D (outside the download's scope)
    // and F, then the download's B, C, E and G.
    assert(found == 8 && pack.custom() == 4);
    Image image;
    const Entry *a = pack.find(Key{0xAAAA0001, 0, 3, 1});
    assert(a && a->zipped && a->custom && a->file.filename() == "mine.zip");  // the custom copy wins
    assert(Pack::read(*a, image) && image.width == 8 && image.rgba[0] == 99);
    const Entry *alias = pack.find(Key{0xAAAA0001, 0x12345678, 3, 1});
    assert(alias && alias->file == a->file && alias->header == a->header);
    const Entry *d = pack.find(Key{0xDDDD0004, 0, 3, 1});
    assert(d && d->method == 0 && Pack::read(*d, image) && image.width == 8 && image.height == 8 && image.rgba[0] == 77);
    const Entry *f = pack.find(Key{0xFFFF0006, 0, 3, 1});
    assert(f && !f->zipped && Pack::read(*f, image) && image.width == 4 && image.rgba[3] == 255);
    const Entry *b = pack.find(Key{0xBBBB0002, 0, 2, 1});
    assert(b && !b->custom && b->file.filename() == "pack.zip");
    // Without a download every file is custom, read for every key it names;
    // between two custom copies the first path wins.
    assert(pack.index(root, scope, nullptr) == 8 && pack.custom() == 8);
    assert(pack.find(Key{0xAAAA0001, 0, 3, 1})->file.filename() == "pack.zip");
}

static void resampling() {
    // A transparent texel's colour must not bleed into its opaque neighbour
    // when the HD ammo art is resized (premultiplied alpha).
    Image src;
    src.width = 2;
    src.height = 1;
    src.rgba = {255, 0, 0, 255, 0, 255, 0, 0};  // opaque red, transparent green
    const Image up = resample(src, 4, 1);
    assert(up.width == 4 && up.height == 1 && up.rgba.size() == 16);
    for (unsigned x = 0; x < 4; x++) {
        if (up.rgba[x * 4 + 3] > 0) assert(up.rgba[x * 4 + 1] == 0 && up.rgba[x * 4 + 0] == 255);
    }
    assert(up.rgba[3] == 255 && up.rgba[15] == 0);  // the ends keep their alpha
    // A uniform image stays uniform when reduced to other proportions.
    Image flat;
    flat.width = 20;
    flat.height = 14;
    flat.rgba.assign(20 * 14 * 4, 0);
    for (size_t i = 0; i < flat.rgba.size(); i += 4) { flat.rgba[i] = 10; flat.rgba[i + 1] = 200; flat.rgba[i + 2] = 30; flat.rgba[i + 3] = 255; }
    const Image down = resample(flat, 6, 9);
    assert(down.width == 6 && down.height == 9);
    for (size_t i = 0; i < down.rgba.size(); i += 4) {
        assert(down.rgba[i] == 10 && down.rgba[i + 1] == 200 && down.rgba[i + 2] == 30 && down.rgba[i + 3] == 255);
    }
    assert(resample(Image{}, 4, 4).rgba.empty());
}

int main(int argc, char **argv) {
    assert(argc == 2);
    identity();
    resampling();
    native_words();
    palettes();
    names();
    pack(argv[1]);
    custom_packs(argv[1]);
    std::printf("hd textures: ok\n");
    return 0;
}
