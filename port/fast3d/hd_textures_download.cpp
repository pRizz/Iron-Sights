/* The HD textures' download for the menus (FORK-053). Run only when the
 * player picks Download on the HD Textures page, it fetches intermissionfb's
 * GoldenEye 007 HD pack exactly as its makers publish it - the PNG release's
 * ZIP, plus the three Egyptian textures that exist only in their repository -
 * from GitHub into Data/TexturePacks (hd_download_engine.cpp checks every
 * byte), then writes SOURCE.txt. Iron Sights never ships, converts or modifies
 * the pack. The C API is port/include/port_hd_download.h. */
#include "port_hd_download.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

#include "hd_download_engine.h"

namespace {

struct Download {
    std::mutex lock;
    std::atomic<bool> running{false}, cancel{false}, just_installed{false};
    hd_download::Progress progress;
    std::string directory, error;
    bool failed = false;
    int present = -1;  /* pinned files in place; -1 = look again */
};
Download g_download;

const hd_download::PackFile *files(int &count) { return hd_download::pack_files(count); }

int count_present(const std::string &directory) {
    int count = 0, n = 0;
    const auto *list = files(count);
    for (int i = 0; i < count; i++) n += hd_download::in_place(directory, list[i]) ? 1 : 0;
    return n;
}

long long total_bytes() {
    int count = 0;
    long long n = 0;
    const auto *list = files(count);
    for (int i = 0; i < count; i++) n += list[i].size;
    return n;
}

/* Where a file comes from: its pinned address, or for tests a local fixture
 * server (GE_HDPACK_BASE_URL=http://127.0.0.1:port) serving every file by name. */
std::string url_of(const hd_download::PackFile &file) {
    const char *base = std::getenv("GE_HDPACK_BASE_URL");
    if (base == nullptr || std::strncmp(base, "http://127.0.0.1:", 17) != 0) return file.url;
    std::string url = std::string(base) + "/";
    for (const char *c = file.name; *c; c++) url += (*c == '#') ? std::string("%23") : std::string(1, *c);
    return url;
}

void write_source(const std::string &directory) {
    std::ofstream out(std::filesystem::path(directory) / "SOURCE.txt", std::ios::binary | std::ios::trunc);
    out << "GoldenEye 007 HD by intermissionfb (font textures and technical assistance:\n"
           "GhostlyDark), release v2025-12-30, downloaded at the player's request from\n"
           "https://github.com/GhostlyDark/GoldenEye-007-HD. It is not part of Iron\n"
           "Sights and is not covered by its license; the artwork belongs to its\n"
           "authors. Remove it from Gameplay > Iron Sights > System > HD Textures.\n";
}

/* The worker's whole job; it owns no state but g_download's atomics until it
 * takes the lock to report. */
void run_download(std::string directory) {
    int count = 0;
    const auto *list = files(count);
    const std::string why = hd_download::install(directory, list, count, url_of,
                                                 enhanced_fonts_download::platform_stream_transport(),
                                                 enhanced_fonts_download::sleeping_wait(), g_download.cancel,
                                                 g_download.progress);
    if (why.empty()) write_source(directory);
    std::lock_guard<std::mutex> guard(g_download.lock);
    g_download.failed = !why.empty() && why != "cancelled";
    g_download.error = g_download.failed ? why : std::string();
    g_download.present = -1;
    if (why.empty()) g_download.just_installed = true;
    g_download.running = false;
}

bool can_download() { return bool(enhanced_fonts_download::platform_stream_transport()); }

}  // namespace

extern "C" int portHdTexturesState(void) {
    if (g_download.running) return PORT_HDTEX_DOWNLOADING;
    int count = 0;
    files(count);
    std::lock_guard<std::mutex> guard(g_download.lock);
    if (g_download.present < 0) g_download.present = count_present(g_download.directory);
    if (g_download.failed) return PORT_HDTEX_FAILED;
    if (g_download.present >= count) return PORT_HDTEX_INSTALLED;
    if (!can_download()) return PORT_HDTEX_UNSUPPORTED;
    return g_download.present > 0 ? PORT_HDTEX_PARTIAL : PORT_HDTEX_MISSING;
}

extern "C" void portHdTexturesSetDirectory(const char *directory) {
    if (directory == nullptr || g_download.running) return;
    std::lock_guard<std::mutex> guard(g_download.lock);
    g_download.directory = directory;
    g_download.present = -1;
}

extern "C" void portHdTexturesProgress(int *done_mb, int *total_mb) {
    long long done = 0;
    if (g_download.running) {
        done = g_download.progress.bytes;
    } else {
        int count = 0;
        const auto *list = files(count);
        std::lock_guard<std::mutex> guard(g_download.lock);
        for (int i = 0; i < count; i++) done += hd_download::in_place(g_download.directory, list[i]) ? list[i].size : 0;
    }
    if (done_mb) *done_mb = int(done / 1000000);
    if (total_mb) *total_mb = int((total_bytes() + 500000) / 1000000);
}

extern "C" const char *portHdTexturesError(void) {
    std::lock_guard<std::mutex> guard(g_download.lock);
    return g_download.error.c_str();
}

extern "C" void portHdTexturesDownload(void) {
    if (g_download.running || !can_download()) return;
    std::string directory;
    {
        std::lock_guard<std::mutex> guard(g_download.lock);
        g_download.failed = false;
        g_download.error.clear();
        directory = g_download.directory;
    }
    g_download.cancel = false;
    g_download.progress.bytes = 0;
    g_download.progress.waiting = 0;
    g_download.running = true;
    std::thread(run_download, directory).detach();  /* quitting mid-download leaves only a .part, never read */
}

extern "C" void portHdTexturesCancel(void) { g_download.cancel = true; }

extern "C" void portHdTexturesRecount(void) {
    if (g_download.running) return;
    std::lock_guard<std::mutex> guard(g_download.lock);
    g_download.present = -1;
}

extern "C" void portHdTexturesRemove(void) {
    if (g_download.running) return;
    int count = 0;
    const auto *list = files(count);
    std::lock_guard<std::mutex> guard(g_download.lock);
    hd_download::remove_files(g_download.directory, list, count);
    g_download.present = 0;
    g_download.failed = false;
    g_download.error.clear();
}

extern "C" int portHdTexturesJustInstalled(void) { return g_download.just_installed.exchange(false) ? 1 : 0; }

extern "C" const char *portHdTexturesInfoLine(int i) {
    static char status[64];
    /* Short enough for the watch face (about 29 characters a line). */
    static const char *const lines[] = {
        "GoldenEye 007 HD by\n",
        "intermissionfb. Not part\n",
        "of Iron Sights. Download\n",
        "gets the pack, 150 MB,\n",
        "from its makers' GitHub.\n",
        "Walls, floors, scenery.\n",
    };
    const int fixed = int(sizeof(lines) / sizeof(lines[0]));
    if (i >= 0 && i < fixed) return lines[i];
    if (i != fixed) return nullptr;
    int done = 0, total = 0;
    portHdTexturesProgress(&done, &total);
    switch (portHdTexturesState()) {
        case PORT_HDTEX_INSTALLED: return "Installed.\n";
        case PORT_HDTEX_DOWNLOADING:
            if (g_download.progress.waiting > 0) {
                std::snprintf(status, sizeof(status), "GitHub busy: retry in %ds\n", g_download.progress.waiting.load());
            } else {
                std::snprintf(status, sizeof(status), "Downloading: %d of %d MB\n", done, total);
            }
            break;
        case PORT_HDTEX_PARTIAL: std::snprintf(status, sizeof(status), "Incomplete: %d of %d MB\n", done, total); break;
        case PORT_HDTEX_FAILED: std::snprintf(status, sizeof(status), "Failed: %s\n", portHdTexturesError()); break;
        case PORT_HDTEX_UNSUPPORTED: return "Copy to Data/TexturePacks\n";
        default: return "Not installed.\n";
    }
    return status;
}
