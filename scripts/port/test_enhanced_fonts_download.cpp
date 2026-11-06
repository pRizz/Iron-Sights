// ROM-free checks for the Enhanced fonts' download engine (FORK-052, FONT-3)
// with a fake transport: fetching, keeping valid files, checksum refusal,
// cancel, failure, removal and URL encoding. No network or pack art.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "../../port/fast3d/enhanced_fonts_download.h"
#include "../../port/fast3d/third_party/picosha2/picosha2.h"

namespace fs = std::filesystem;
using namespace enhanced_fonts_download;
using enhanced_font_masks::PackFile;

namespace {

struct Fixture {
    std::vector<std::string> bodies, hashes, paths;
    std::vector<PackFile> files;
    fs::path dir;

    Fixture() {
        bodies = {"first file", "second, longer file", "third"};
        paths = {"Bank Gothic/Upper Case/GOLDENEYE#AAAA#4#1_rgb.png", "Bank Gothic/Misc/GOLDENEYE#BBBB#4#1_rgb.png",
                 "Zurich Bold/Misc/GOLDENEYE#CCCC#4#1_rgb.png"};
        for (const auto &body : bodies) hashes.push_back(picosha2::hash256_hex_string(body));
        for (size_t i = 0; i < bodies.size(); i++) {
            files.push_back({0, char('A' + i), paths[i].c_str(), uint32_t(bodies[i].size()), hashes[i].c_str()});
        }
        dir = fs::temp_directory_path() / "enhanced_fonts_download_test" / "Data" / "FontPacks" / "gepd";
        fs::remove_all(dir.parent_path().parent_path());
    }
    ~Fixture() { fs::remove_all(dir.parent_path().parent_path()); }

    /* Serves each file's body by its URL's ending; counts the calls. */
    Transport serving(int &calls, bool corrupt = false) {
        return [this, &calls, corrupt](const std::string &url, std::vector<uint8_t> &out, std::string &, const std::atomic<bool> &) {
            calls++;
            for (size_t i = 0; i < files.size(); i++) {
                const std::string name = enhanced_font_masks::file_name(files[i]);
                const std::string encoded = name.substr(0, name.find('#')) + "%23";
                if (url.find(encoded) != std::string::npos && url.find(name.substr(name.find('#') + 1, 4)) != std::string::npos) {
                    out.assign(bodies[i].begin(), bodies[i].end());
                    if (corrupt) out[0] ^= 1;
                    return true;
                }
            }
            return false;
        };
    }
};

void test_url() {
    Fixture f;
    const std::string url = file_url("https://example.test/", f.files[0]);
    assert(url == "https://example.test/GoldenEye%20Texture%20Pack/Bank%20Gothic/Upper%20Case/GOLDENEYE%23AAAA%234%231_rgb.png");
    assert(default_base() == "https://raw.githubusercontent.com/Graslu/GEPDTexturePacks/ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63/");
}

void test_install_and_keep() {
    Fixture f;
    std::atomic<bool> cancel{false};
    std::atomic<int> done{0};
    int calls = 0;
    Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(), f.serving(calls), cancel, done);
    assert(o.result == Result::Done && done == 3 && calls == 3);
    assert(fs::exists(f.dir / "SOURCE.txt") && !fs::exists(f.dir / "GOLDENEYE#AAAA#4#1_rgb.png.part"));
    assert(count_valid(f.files.data(), f.files.size(), f.dir.string()) == 3);

    calls = 0;
    done = 0;
    o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(), f.serving(calls), cancel, done);
    assert(o.result == Result::Done && done == 3 && calls == 0);  // every file was already valid

    remove_files(f.files.data(), f.files.size(), f.dir.string());
    assert(count_valid(f.files.data(), f.files.size(), f.dir.string()) == 0 && !fs::exists(f.dir / "SOURCE.txt"));
}

void test_refuses_mismatch() {
    Fixture f;
    std::atomic<bool> cancel{false};
    std::atomic<int> done{0};
    int calls = 0;
    const Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(), f.serving(calls, true),
                              cancel, done);
    assert(o.result == Result::Failed && o.error == "A file did not match" && done == 0);
    assert(count_valid(f.files.data(), f.files.size(), f.dir.string()) == 0);
    assert(!fs::exists(f.dir / enhanced_font_masks::file_name(f.files[0])));
}

void test_cancel_and_failure() {
    Fixture f;
    std::atomic<bool> cancel{false};
    std::atomic<int> done{0};
    const Transport cancelling = [&cancel](const std::string &, std::vector<uint8_t> &, std::string &, const std::atomic<bool> &) {
        cancel = true;
        return false;
    };
    Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(), cancelling, cancel, done);
    assert(o.result == Result::Cancelled);

    cancel = false;
    const Transport offline = [](const std::string &, std::vector<uint8_t> &, std::string &error, const std::atomic<bool> &) {
        error = "No connection";
        return false;
    };
    o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(), offline, cancel, done);
    assert(o.result == Result::Failed && o.error == "No connection");
}

}  // namespace

int main() {
    test_url();
    test_install_and_keep();
    test_refuses_mismatch();
    test_cancel_and_failure();
    std::printf("enhanced fonts download: ok\n");
    return 0;
}
