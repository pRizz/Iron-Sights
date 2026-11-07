// ROM-free checks for the HD textures' download engine (FORK-053): files are
// streamed, verified and renamed into place; a wrong checksum or size, a busy
// server and a cancel leave nothing behind; removal deletes only pinned files.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "port/fast3d/hd_download_engine.h"
#include "port/fast3d/third_party/picosha2/picosha2.h"

using enhanced_fonts_download::Failure;
using enhanced_fonts_download::Sink;
namespace fs = std::filesystem;

static std::string read(const fs::path &p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static std::string url(const hd_download::PackFile &f) { return f.url; }

static bool no_wait(int, std::atomic<int> &, const std::atomic<bool> &) { return true; }

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string dir = argv[1];
    fs::create_directories(dir);
    const fs::path zip = fs::path(dir) / "pack.zip", png = fs::path(dir) / "sub" / "GOLDENEYE#00000001#3#1_all.png";
    const std::string big(3000, 'x');
    const std::string big_pin = picosha2::hash256_hex_string(big);
    // "abc" has the standard SHA-256 test vector.
    hd_download::PackFile files[2] = {
        {"", "pack.zip", "http://test/pack.zip", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"sub", "GOLDENEYE#00000001#3#1_all.png", "http://test/extra", 3000,
         "0000000000000000000000000000000000000000000000000000000000000000"},
    };
    std::map<std::string, std::string> bodies{{"http://test/pack.zip", "abc"}, {"http://test/extra", big}};
    std::vector<std::string> asked;
    int busy_left = 0;
    std::atomic<bool> cancel{false};
    auto transport = [&](const std::string &u, const Sink &sink, Failure &failure, const std::atomic<bool> &) {
        asked.push_back(u);
        if (busy_left > 0) {
            busy_left--;
            failure.reason = "Server error 429";
            failure.retry_after = 0;
            return false;
        }
        const std::string &b = bodies[u];
        for (size_t at = 0; at < b.size(); at += 1000) {
            const size_t n = b.size() - at < 1000 ? b.size() - at : 1000;
            if (!sink(reinterpret_cast<const uint8_t *>(b.data() + at), n)) return true;  // the sink stopped it
        }
        return true;
    };
    hd_download::Progress progress;

    // A wrong checksum: the first file lands, the second leaves nothing.
    std::string why = hd_download::install(dir, files, 2, url, transport, no_wait, cancel, progress);
    assert(why == "Checksum mismatch");
    assert(read(zip) == "abc" && !fs::exists(png) && !fs::exists(png.string() + ".part"));

    // A longer body than pinned is refused, not cut short.
    files[1].sha256 = big_pin.c_str();
    bodies["http://test/extra"] = big + "y";
    assert(hd_download::install(dir, files, 2, url, transport, no_wait, cancel, progress) == "Wrong file size");
    bodies["http://test/extra"] = big;

    // The right file installs; the first, already in place, is not asked again.
    asked.clear();
    why = hd_download::install(dir, files, 2, url, transport, no_wait, cancel, progress);
    assert(why.empty() && asked.size() == 1 && asked[0] == "http://test/extra");
    assert(read(png) == big && progress.bytes == 3003);
    assert(hd_download::in_place(dir, files[0]) && hd_download::in_place(dir, files[1]));

    // A busy server is asked again; a fifth busy answer gives up.
    hd_download::remove_files(dir, files, 2);
    busy_left = 2;
    asked.clear();
    assert(hd_download::install(dir, files, 2, url, transport, no_wait, cancel, progress).empty() && asked.size() == 4);
    hd_download::remove_files(dir, files, 2);
    busy_left = 5;
    assert(hd_download::install(dir, files, 2, url, transport, no_wait, cancel, progress) == "GitHub busy");
    assert(!fs::exists(zip));
    busy_left = 0;

    // A cancel stops without leaving a file.
    cancel = true;
    assert(hd_download::install(dir, files, 2, url, transport, no_wait, cancel, progress) == "cancelled");
    assert(!fs::exists(zip) && !fs::exists(zip.string() + ".part"));
    cancel = false;

    // Removal deletes the pinned files and the note, and nothing of the player's.
    assert(hd_download::install(dir, files, 2, url, transport, no_wait, cancel, progress).empty());
    std::ofstream(fs::path(dir) / "SOURCE.txt") << "note";
    std::ofstream(fs::path(dir) / "mine.zip") << "player's";
    hd_download::remove_files(dir, files, 2);
    assert(!fs::exists(zip) && !fs::exists(fs::path(dir) / "sub") && !fs::exists(fs::path(dir) / "SOURCE.txt"));
    assert(read(fs::path(dir) / "mine.zip") == "player's");

    std::printf("hd download: ok\n");
    return 0;
}
