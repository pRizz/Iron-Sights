/* The Enhanced fonts' download (FORK-052, FONT-3): the engine and the menus'
 * C API. See enhanced_fonts_download.h and port/include/port_enhanced_fonts.h. */
#include "enhanced_fonts_download.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>

#include "../include/port_enhanced_fonts.h"
#include "enhanced_fonts.h"

namespace enhanced_fonts_download {
namespace {

namespace fs = std::filesystem;

const char kRepository[] = "Graslu/GEPDTexturePacks";
const char kCommit[] = "ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63";
const char kPackDir[] = "GoldenEye Texture Pack/";
const char kSourceNote[] =
    "Enhanced fonts for Iron Sights\n\n"
    "Carnivorous's GEPD font images (CC0-1.0), from\n"
    "https://github.com/Graslu/GEPDTexturePacks at commit\n"
    "ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63.\n"
    "Installed by the player; Iron Sights does not bundle them.\n";

bool read_file(const fs::path &path, std::vector<uint8_t> &out) {
    FILE *f = std::fopen(path.string().c_str(), "rb");
    if (f == nullptr) return false;
    out.clear();
    uint8_t buffer[4096];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0 && out.size() < (1u << 20)) out.insert(out.end(), buffer, buffer + n);
    std::fclose(f);
    return true;
}

bool write_file(const fs::path &path, const void *data, size_t size) {
    FILE *f = std::fopen(path.string().c_str(), "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(data, 1, size, f) == size;
    return std::fclose(f) == 0 && ok;
}

bool valid_file(const fs::path &path, const enhanced_font_masks::PackFile &file) {
    std::vector<uint8_t> data;
    return read_file(path, data) && enhanced_font_masks::verify(data.data(), data.size(), file);
}

/* Percent-encodes a pack path (spaces and '#' appear in its names). */
std::string encode_path(const std::string &path) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : path) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || std::strchr("-._~/", c)) {
            out += char(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

}  // namespace

std::string default_base() {
    return std::string("https://raw.githubusercontent.com/") + kRepository + "/" + kCommit + "/";
}

std::string file_url(const std::string &base, const enhanced_font_masks::PackFile &file) {
    return base + encode_path(std::string(kPackDir) + file.path);
}

Outcome install(const std::string &base, const enhanced_font_masks::PackFile *files, size_t count,
                const std::string &directory, const Transport &transport, const std::atomic<bool> &cancel,
                std::atomic<int> &done) {
    std::error_code ec;
    fs::create_directories(fs::path(directory), ec);
    if (ec) return {Result::Failed, "Folder not writable"};
    std::vector<uint8_t> data;
    for (size_t i = 0; i < count; i++) {
        if (cancel) return {Result::Cancelled, ""};
        const fs::path target = fs::path(directory) / enhanced_font_masks::file_name(files[i]);
        if (valid_file(target, files[i])) {
            done++;
            continue;
        }
        std::string error;
        if (!transport(file_url(base, files[i]), data, error, cancel)) {
            if (cancel) return {Result::Cancelled, ""};
            return {Result::Failed, error.empty() ? "Download failed" : error};
        }
        if (!enhanced_font_masks::verify(data.data(), data.size(), files[i])) return {Result::Failed, "A file did not match"};
        fs::path partial = target;
        partial += ".part";
        if (!write_file(partial, data.data(), data.size())) return {Result::Failed, "Folder not writable"};
        fs::rename(partial, target, ec);
        if (ec) return {Result::Failed, "Folder not writable"};
        done++;
    }
    if (!write_file(fs::path(directory) / "SOURCE.txt", kSourceNote, sizeof(kSourceNote) - 1)) {
        return {Result::Failed, "Folder not writable"};
    }
    return {Result::Done, ""};
}

void remove_files(const enhanced_font_masks::PackFile *files, size_t count, const std::string &directory) {
    std::error_code ec;
    for (size_t i = 0; i < count; i++) {
        fs::path target = fs::path(directory) / enhanced_font_masks::file_name(files[i]);
        fs::remove(target, ec);
        target += ".part";
        fs::remove(target, ec);
    }
    fs::remove(fs::path(directory) / "SOURCE.txt", ec);
}

int count_valid(const enhanced_font_masks::PackFile *files, size_t count, const std::string &directory) {
    int valid = 0;
    for (size_t i = 0; i < count; i++) {
        valid += valid_file(fs::path(directory) / enhanced_font_masks::file_name(files[i]), files[i]) ? 1 : 0;
    }
    return valid;
}

}  // namespace enhanced_fonts_download

/* ---- The menus' C API: one download at a time, on a worker thread. ---- */
namespace {

using namespace enhanced_fonts_download;

struct Download {
    std::mutex lock;
    std::atomic<bool> running{false}, cancel{false}, just_installed{false};
    std::atomic<int> done{0};
    int valid = -1;  /* verified files in the folder; -1 = not counted yet */
    bool failed = false;
    std::string error;
};

Download g_download;

size_t pinned_count() {
    size_t count = 0;
    enhanced_font_masks::pack_files(count);
    return count;
}

/* The worker's whole job; it owns no state but g_download's atomics until it
 * takes the lock to report. */
void run_download() {
    size_t count = 0;
    const auto *files = enhanced_font_masks::pack_files(count);
    const std::string directory = enhanced_fonts::directory();
    const char *override_base = std::getenv("GE_FONTPACK_BASE_URL");  /* tests: a local fixture only */
    const bool local = override_base != nullptr && std::strncmp(override_base, "http://127.0.0.1:", 17) == 0;
    const Outcome outcome = install(local ? override_base : default_base(), files, count, directory,
                                    platform_transport(), g_download.cancel, g_download.done);
    const int valid = count_valid(files, count, directory);
    enhanced_fonts::reload();
    std::lock_guard<std::mutex> guard(g_download.lock);
    g_download.valid = valid;
    g_download.failed = outcome.result == Result::Failed;
    g_download.error = outcome.error;
    if (outcome.result == Result::Done) g_download.just_installed = true;
    g_download.running = false;
}

}  // namespace

extern "C" int portEnhancedFontsState(void) {
    if (g_download.running) return PORT_EFONTS_DOWNLOADING;
    std::lock_guard<std::mutex> guard(g_download.lock);
    if (g_download.valid < 0) {
        size_t count = 0;
        const auto *files = enhanced_font_masks::pack_files(count);
        g_download.valid = count_valid(files, count, enhanced_fonts::directory());
    }
    if (g_download.failed) return PORT_EFONTS_FAILED;
    if (g_download.valid >= int(pinned_count())) return PORT_EFONTS_INSTALLED;
    if (!platform_transport()) return PORT_EFONTS_UNSUPPORTED;
    return g_download.valid > 0 ? PORT_EFONTS_PARTIAL : PORT_EFONTS_MISSING;
}

extern "C" void portEnhancedFontsProgress(int *done, int *total) {
    const int state = portEnhancedFontsState();
    std::lock_guard<std::mutex> guard(g_download.lock);
    if (done) *done = state == PORT_EFONTS_DOWNLOADING ? g_download.done.load() : g_download.valid;
    if (total) *total = int(pinned_count());
}

extern "C" const char *portEnhancedFontsError(void) {
    std::lock_guard<std::mutex> guard(g_download.lock);
    return g_download.error.c_str();
}

extern "C" void portEnhancedFontsDownload(void) {
    if (g_download.running || !platform_transport()) return;
    {
        std::lock_guard<std::mutex> guard(g_download.lock);
        g_download.failed = false;
        g_download.error.clear();
    }
    g_download.cancel = false;
    g_download.done = 0;
    g_download.running = true;
#ifdef __EMSCRIPTEN__
    run_download();  /* unreachable until FONT-4: the browser has no transport yet */
#else
    std::thread(run_download).detach();  /* quitting mid-download just abandons it; no partial file is used */
#endif
}

extern "C" void portEnhancedFontsCancel(void) {
    g_download.cancel = true;
}

extern "C" void portEnhancedFontsRecount(void) {
    if (g_download.running) return;
    enhanced_fonts::reload();
    std::lock_guard<std::mutex> guard(g_download.lock);
    g_download.valid = -1;
}

extern "C" void portEnhancedFontsRemove(void) {
    if (g_download.running) return;
    size_t count = 0;
    const auto *files = enhanced_font_masks::pack_files(count);
    remove_files(files, count, enhanced_fonts::directory());
    enhanced_fonts::reload();
    std::lock_guard<std::mutex> guard(g_download.lock);
    g_download.valid = 0;
    g_download.failed = false;
    g_download.error.clear();
}

extern "C" int portEnhancedFontsJustInstalled(void) {
    return g_download.just_installed.exchange(false) ? 1 : 0;
}

extern "C" const char *portEnhancedFontsInfoLine(int i) {
    static char status[64];
    /* Short enough for the watch face (about 29 characters a line). */
    static const char *const lines[] = {
        "A community redraw by\n",
        "Carnivorous, CC0.\n",
        "Not part of Iron Sights.\n",
        "Download gets 186 files,\n",
        "250 KB, from GitHub:\n",
        "Graslu/GEPDTexturePacks.\n",
    };
    const int fixed = int(sizeof(lines) / sizeof(lines[0]));
    if (i >= 0 && i < fixed) return lines[i];
    if (i != fixed) return nullptr;
    int done = 0, total = 0;
    portEnhancedFontsProgress(&done, &total);
    switch (portEnhancedFontsState()) {
        case PORT_EFONTS_INSTALLED: return "Installed.\n";
        case PORT_EFONTS_DOWNLOADING: std::snprintf(status, sizeof(status), "Downloading: %d of %d\n", done, total); break;
        case PORT_EFONTS_PARTIAL: std::snprintf(status, sizeof(status), "Incomplete: %d of %d files\n", done, total); break;
        case PORT_EFONTS_FAILED: std::snprintf(status, sizeof(status), "Failed: %s\n", portEnhancedFontsError()); break;
        case PORT_EFONTS_UNSUPPORTED: return "Copy to Data/FontPacks/gepd\n";
        default: return "Not installed.\n";
    }
    return status;
}
