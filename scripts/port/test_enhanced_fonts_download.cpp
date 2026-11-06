// ROM-free checks for the Enhanced fonts' download engine (FORK-052, FONT-3)
// with a fake transport: fetching, keeping valid files, checksum refusal,
// cancel, failure, a busy server's backoff, removal and URL encoding. No
// network, pack art or real waiting.
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
        return [this, &calls, corrupt](const std::string &url, std::vector<uint8_t> &out, Failure &, const std::atomic<bool> &) {
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

/* Records the waits install asks for instead of sleeping. */
Wait recording(std::vector<int> &waits) {
    return [&waits](int seconds, std::atomic<int> &, const std::atomic<bool> &cancel) {
        waits.push_back(seconds);
        return !cancel;
    };
}

void test_url() {
    Fixture f;
    const std::string url = file_url("https://example.test/", f.files[0]);
    assert(url == "https://example.test/GoldenEye%20Texture%20Pack/Bank%20Gothic/Upper%20Case/GOLDENEYE%23AAAA%234%231_rgb.png");
    assert(default_base() == "https://raw.githubusercontent.com/Graslu/GEPDTexturePacks/ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63/");
}

void test_install_and_keep() {
    Fixture f;
    std::atomic<bool> cancel{false};
    Progress progress;
    int calls = 0;
    Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(), f.serving(calls), cancel, progress);
    assert(o.result == Result::Done && progress.done == 3 && calls == 3);
    assert(fs::exists(f.dir / "SOURCE.txt") && !fs::exists(f.dir / "GOLDENEYE#AAAA#4#1_rgb.png.part"));
    assert(count_valid(f.files.data(), f.files.size(), f.dir.string()) == 3);

    calls = 0;
    progress.done = 0;
    o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(), f.serving(calls), cancel, progress);
    assert(o.result == Result::Done && progress.done == 3 && calls == 0);  // every file was already valid

    remove_files(f.files.data(), f.files.size(), f.dir.string());
    assert(count_valid(f.files.data(), f.files.size(), f.dir.string()) == 0 && !fs::exists(f.dir / "SOURCE.txt"));
}

void test_refuses_mismatch() {
    Fixture f;
    std::atomic<bool> cancel{false};
    Progress progress;
    int calls = 0;
    const Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(), f.serving(calls, true),
                              cancel, progress);
    assert(o.result == Result::Failed && o.error == "A file did not match" && progress.done == 0);
    assert(count_valid(f.files.data(), f.files.size(), f.dir.string()) == 0);
    assert(!fs::exists(f.dir / enhanced_font_masks::file_name(f.files[0])));
}

void test_cancel_and_failure() {
    Fixture f;
    std::atomic<bool> cancel{false};
    Progress progress;
    std::vector<int> waits;
    const Transport cancelling = [&cancel](const std::string &, std::vector<uint8_t> &, Failure &, const std::atomic<bool> &) {
        cancel = true;
        return false;
    };
    Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(), cancelling, cancel, progress);
    assert(o.result == Result::Cancelled);

    cancel = false;
    const Transport offline = [](const std::string &, std::vector<uint8_t> &, Failure &failure, const std::atomic<bool> &) {
        failure.reason = "No connection";
        return false;
    };
    o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(), offline, cancel, progress,
                recording(waits));
    assert(o.result == Result::Failed && o.error == "No connection" && waits.empty());  // not busy: no retry
}

/* Busy answers (429 and the like) for the first `busy` calls, then the files;
 * retry_after is what each busy answer names (0 = nothing). */
Transport busy_then_serving(Fixture &f, int busy, int retry_after, int &calls, int &served) {
    const Transport serve = f.serving(served);
    return [serve, busy, retry_after, &calls](const std::string &url, std::vector<uint8_t> &out, Failure &failure,
                                              const std::atomic<bool> &cancel) {
        if (++calls <= busy) {
            failure.reason = "Server error 429";
            failure.retry_after = retry_after;
            return false;
        }
        return serve(url, out, failure, cancel);
    };
}

void test_busy_server() {
    std::atomic<bool> cancel{false};
    {   // no Retry-After: 2, 4 and 8 seconds, then the download carries on
        Fixture f;
        Progress progress;
        std::vector<int> waits;
        int calls = 0, served = 0;
        const Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(),
                                  busy_then_serving(f, 3, 0, calls, served), cancel, progress, recording(waits));
        assert(o.result == Result::Done && progress.done == 3 && served == 3);
        assert((waits == std::vector<int>{2, 4, 8}));
    }
    {   // the server's own wait is used as given
        Fixture f;
        Progress progress;
        std::vector<int> waits;
        int calls = 0, served = 0;
        const Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(),
                                  busy_then_serving(f, 2, 7, calls, served), cancel, progress, recording(waits));
        assert(o.result == Result::Done && (waits == std::vector<int>{7, 7}));
    }
    {   // still busy after five tries: give up, nothing written
        Fixture f;
        Progress progress;
        std::vector<int> waits;
        int calls = 0, served = 0;
        const Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(),
                                  busy_then_serving(f, 100, 0, calls, served), cancel, progress, recording(waits));
        assert(o.result == Result::Failed && o.error == "GitHub busy" && calls == 5);
        assert((waits == std::vector<int>{2, 4, 8, 16}) && progress.done == 0);
    }
    {   // a wait over a minute gives up at once rather than stalling the page
        Fixture f;
        Progress progress;
        std::vector<int> waits;
        int calls = 0, served = 0;
        const Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(),
                                  busy_then_serving(f, 1, 120, calls, served), cancel, progress, recording(waits));
        assert(o.result == Result::Failed && o.error == "GitHub busy" && waits.empty());
    }
    {   // cancelled during the wait
        Fixture f;
        Progress progress;
        int calls = 0, served = 0;
        const Wait cancelled = [&cancel](int, std::atomic<int> &, const std::atomic<bool> &) {
            cancel = true;
            return false;
        };
        const Outcome o = install("https://example.test/", f.files.data(), f.files.size(), f.dir.string(),
                                  busy_then_serving(f, 1, 0, calls, served), cancel, progress, cancelled);
        assert(o.result == Result::Cancelled && served == 0);
        cancel = false;
    }
}

void test_sleeping_wait_counts_down() {
    std::atomic<bool> cancel{false};
    std::atomic<int> waiting{0};
    const Wait wait = sleeping_wait();
    assert(wait(1, waiting, cancel) && waiting == 0);
    cancel = true;
    assert(!wait(30, waiting, cancel) && waiting == 0);  // returns at once when cancelled
}

}  // namespace

int main() {
    test_url();
    test_install_and_keep();
    test_refuses_mismatch();
    test_cancel_and_failure();
    test_busy_server();
    test_sleeping_wait_counts_down();
    std::printf("enhanced fonts download: ok\n");
    return 0;
}
