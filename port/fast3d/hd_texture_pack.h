#ifndef MIDAS_HD_TEXTURE_PACK_H
#define MIDAS_HD_TEXTURE_PACK_H
/* Player-installed HD texture packs (FORK-053): an index of a pack exactly as
 * its makers publish it - a GLideN64 PNG pack's ZIP, or that ZIP extracted -
 * limited to a scope of textures, with each image read when first needed.
 * Iron Sights never ships, converts or downloads a pack. */
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "hd_texture_format.h"

namespace hd_textures {
struct Image {
    unsigned width = 0, height = 0;
    std::vector<uint8_t> rgba;
};

/* Which pack textures may replace native ones, and the native palette
 * identities that stand for a pack key (reviewed aliases: native palettes
 * fingerprint differently from GLideN64's). */
struct Scope {
    std::set<Key> keys;                    /* pack keys; palette 0 = any palette */
    std::multimap<Key, uint32_t> aliases;  /* pack key -> native palette */
};

/* A Rice file name in any folder ("GOLDENEYE#tttttttt#f#s_all.png", or with
 * "#pppppppp" or "#$" before "_ciByRGBA.png") as a key; "$" gives palette 0. */
bool parse_name(const std::string &name, Key &key);

/* src resized to width x height with a tent filter (as wide as the
 * reduction, at least one source texel) in premultiplied alpha, so edges do
 * not darken: the HD ammo icons' art has other proportions than the icons. */
Image resample(const Image &src, unsigned width, unsigned height);

/* The scope from the inventory and alias JSON documents. */
bool parse_scope(const std::string &sources, const std::vector<std::string> &aliases, Scope &scope);

struct Entry {
    std::filesystem::path file;  /* the ZIP, or the PNG itself */
    bool zipped = false;
    uint64_t header = 0;         /* the ZIP entry's local header */
    uint32_t method = 0, packed = 0, size = 0;
};

class Pack {
public:
    /* Index every ZIP and PNG under root; returns the scoped keys found. */
    size_t index(const std::filesystem::path &root, const Scope &scope);
    void clear() { entries_.clear(); }
    size_t size() const { return entries_.size(); }
    /* The entry for a native key: exact, then any palette. */
    const Entry *find(const Key &key) const;
    /* The entry's image as RGBA; false for anything unreadable. */
    static bool read(const Entry &entry, Image &image);

private:
    void add(const Key &key, const Entry &entry, const Scope &scope);
    void index_zip(const std::filesystem::path &zip, const Scope &scope);
    std::map<Key, Entry> entries_;
};
}
#endif
