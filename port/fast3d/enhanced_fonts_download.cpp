/* The Enhanced fonts' download (FORK-052, FONT-3): the engine and the menus'
 * C API. See enhanced_fonts_download.h and port/include/port_enhanced_fonts.h. */
#include "enhanced_fonts_download.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "../include/port_enhanced_fonts.h"
#include "enhanced_fonts.h"

namespace enhanced_fonts_download {
namespace {

namespace fs = std::filesystem;

const char kRepository[] = "Graslu/GEPDTexturePacks";
const char kCommit[] = "ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63";
const char kPackDir[] = "GoldenEye Texture Pack/";
/* A busy server: tries per file, the first wait when it names none (doubling
 * after), and the longest wait accepted before giving up. */
constexpr int kBusyTries = 5;
constexpr int kFirstBackoff = 2;
constexpr int kLongestWait = 60;
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

Wait sleeping_wait() {
    return [](int seconds, std::atomic<int> &waiting, const std::atomic<bool> &cancel) {
        for (int tenths = seconds * 10; tenths > 0; tenths--) {
            if (cancel) break;
            waiting = (tenths + 9) / 10;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        waiting = 0;
        return !cancel;
    };
}

std::string default_base() {
    return std::string("https://raw.githubusercontent.com/") + kRepository + "/" + kCommit + "/";
}

std::string file_url(const std::string &base, const enhanced_font_masks::PackFile &file) {
    return base + encode_path(std::string(kPackDir) + file.path);
}

Installer::Installer(std::string base, const enhanced_font_masks::PackFile *files, size_t count, std::string directory,
                     Progress &progress)
    : base_(std::move(base)), directory_(std::move(directory)), files_(files), count_(count), progress_(progress),
      backoff_(kFirstBackoff) {
    std::error_code ec;
    fs::create_directories(fs::path(directory_), ec);
    if (ec) finish(Result::Failed, "Folder not writable");
}

void Installer::finish(Result result, std::string error) {
    finished_ = true;
    outcome_ = {result, std::move(error)};
}

Installer::Step Installer::next() {
    if (finished_) return {};
    if (waiting_) return {Step::Kind::Wait, "", wait_seconds_};
    while (!checked_) {
        if (index_ == count_) {
            if (write_file(fs::path(directory_) / "SOURCE.txt", kSourceNote, sizeof(kSourceNote) - 1)) {
                finish(Result::Done, "");
            } else {
                finish(Result::Failed, "Folder not writable");
            }
            return {};
        }
        if (!valid_file(fs::path(directory_) / enhanced_font_masks::file_name(files_[index_]), files_[index_])) {
            checked_ = true;
            break;
        }
        progress_.done++;
        index_++;
    }
    return {Step::Kind::Fetch, file_url(base_, files_[index_]), 0};
}

void Installer::fetched(bool ok, const std::vector<uint8_t> &data, const Failure &failure) {
    if (finished_ || waiting_ || !checked_) return;  /* no fetch was asked for */
    if (!ok) {
        if (failure.retry_after < 0) {
            finish(Result::Failed, failure.reason.empty() ? "Download failed" : failure.reason);
            return;
        }
        const int seconds = failure.retry_after > 0 ? failure.retry_after : backoff_;
        if (attempt_ >= kBusyTries || seconds > kLongestWait) {
            finish(Result::Failed, "GitHub busy");
            return;
        }
        attempt_++;
        backoff_ *= 2;
        waiting_ = true;
        wait_seconds_ = seconds;
        return;
    }
    const auto &file = files_[index_];
    if (!enhanced_font_masks::verify(data.data(), data.size(), file)) {
        finish(Result::Failed, "A file did not match");
        return;
    }
    const fs::path target = fs::path(directory_) / enhanced_font_masks::file_name(file);
    fs::path partial = target;
    partial += ".part";
    std::error_code ec;
    if (!write_file(partial, data.data(), data.size())) {
        finish(Result::Failed, "Folder not writable");
        return;
    }
    fs::rename(partial, target, ec);
    if (ec) {
        finish(Result::Failed, "Folder not writable");
        return;
    }
    progress_.done++;
    index_++;
    checked_ = false;
    attempt_ = 1;
    backoff_ = kFirstBackoff;
}

void Installer::waited() {
    waiting_ = false;
}

Outcome install(const std::string &base, const enhanced_font_masks::PackFile *files, size_t count,
                const std::string &directory, const Transport &transport, const std::atomic<bool> &cancel,
                Progress &progress, const Wait &wait) {
    Installer installer(base, files, count, directory, progress);
    std::vector<uint8_t> data;
    for (;;) {
        if (cancel) return {Result::Cancelled, ""};
        const Installer::Step step = installer.next();
        if (step.kind == Installer::Step::Kind::Finished) return installer.outcome();
        if (step.kind == Installer::Step::Kind::Wait) {
            if (!wait(step.seconds, progress.waiting, cancel)) return {Result::Cancelled, ""};
            installer.waited();
            continue;
        }
        Failure failure;
        const bool ok = transport(step.url, data, failure, cancel);
        if (!ok && cancel) return {Result::Cancelled, ""};
        installer.fetched(ok, data, failure);
    }
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
    Progress progress;
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

std::string download_base() {
    const char *override_base = std::getenv("GE_FONTPACK_BASE_URL");  /* tests: a local fixture only */
    const bool local = override_base != nullptr && std::strncmp(override_base, "http://127.0.0.1:", 17) == 0;
    return local ? override_base : default_base();
}

/* Records how a download ended and lets the renderer see the new files. */
void report(const Outcome &outcome) {
    size_t count = 0;
    const auto *files = enhanced_font_masks::pack_files(count);
    const int valid = count_valid(files, count, enhanced_fonts::directory());
    enhanced_fonts::reload();
    std::lock_guard<std::mutex> guard(g_download.lock);
    g_download.valid = valid;
    g_download.failed = outcome.result == Result::Failed;
    g_download.error = outcome.error;
    if (outcome.result == Result::Done) g_download.just_installed = true;
    g_download.progress.waiting = 0;
    g_download.running = false;
}

#ifdef __EMSCRIPTEN__

/* The browser has no threads: fetch() runs beside the game and a pump on each
 * frame answers the Installer's steps (FONT-4). A request's answer waits in
 * Module.midasFontFetches until the pump takes it. */
EM_JS(int, efonts_fetch_start, (const char *url), {
    const table = Module.midasFontFetches || (Module.midasFontFetches = { next: 1, live: {} });
    const id = table.next++;
    const entry = { done: false, status: 0, retryAfter: -1, bytes: null, online: true, controller: new AbortController() };
    table.live[id] = entry;
    fetch(UTF8ToString(url), { signal: entry.controller.signal })
        .then(async response => {
            entry.status = response.status;
            const header = response.headers.get('Retry-After');
            entry.retryAfter = header ? (parseInt(header, 10) || 0) : 0;
            if (response.ok) entry.bytes = new Uint8Array(await response.arrayBuffer());
        })
        .catch(() => {
            entry.status = 0;
            entry.online = typeof navigator === 'undefined' || navigator.onLine !== false;
        })
        .finally(() => { entry.done = true; });
    return id;
});
/* -1 while in flight, else the HTTP status (0 = no answer). */
EM_JS(int, efonts_fetch_status, (int id), {
    const entry = Module.midasFontFetches && Module.midasFontFetches.live[id];
    return entry && entry.done ? entry.status : -1;
});
EM_JS(int, efonts_fetch_size, (int id), {
    const entry = Module.midasFontFetches.live[id];
    return entry.bytes ? entry.bytes.length : 0;
});
EM_JS(int, efonts_fetch_retry_after, (int id), {
    return Module.midasFontFetches.live[id].retryAfter;
});
EM_JS(int, efonts_fetch_online, (int id), {
    return Module.midasFontFetches.live[id].online ? 1 : 0;
});
/* Copies the body out and forgets the request. */
EM_JS(void, efonts_fetch_take, (int id, uint8_t *out), {
    const entry = Module.midasFontFetches.live[id];
    if (entry.bytes) HEAPU8.set(entry.bytes, out);
    delete Module.midasFontFetches.live[id];
});
EM_JS(void, efonts_fetch_abort, (int id), {
    const entry = Module.midasFontFetches && Module.midasFontFetches.live[id];
    if (entry) entry.controller.abort();
    if (entry) delete Module.midasFontFetches.live[id];
});

struct BrowserDownload {
    std::unique_ptr<Installer> installer;
    int request = 0;        /* the fetch in flight, 0 = none */
    double wait_until = 0;  /* emscripten_get_now() ms when a busy wait ends, 0 = not waiting */
};
BrowserDownload g_browser;

/* A finished fetch as the Installer reads it: the body, or why not. A failed
 * fetch while the browser is online may be a busy answer it hid (a 429
 * without CORS headers), so it is retried like one. */
void take_fetch(int id, int status, std::vector<uint8_t> &data, Failure &failure, bool &ok) {
    const int size = efonts_fetch_size(id);
    const int retry_after = efonts_fetch_retry_after(id);
    const bool online = efonts_fetch_online(id) != 0;
    data.assign(size_t(size), 0);
    efonts_fetch_take(id, data.data());
    ok = status == 200 && size_t(size) <= (1u << 20);
    if (ok) return;
    if (status == 200) {
        failure.reason = "File too large";
    } else if (status == 0) {
        failure.reason = "No connection";
        if (online) failure.retry_after = 0;
    } else {
        failure.reason = "Server error " + std::to_string(status);
        if (status == 429 || status == 502 || status == 503 || status == 504) failure.retry_after = retry_after > 0 ? retry_after : 0;
    }
}

void pump() {
    BrowserDownload &b = g_browser;
    if (!b.installer) return;
    if (g_download.cancel) {
        if (b.request != 0) efonts_fetch_abort(b.request);
        b = BrowserDownload();
        report({Result::Cancelled, ""});
        return;
    }
    for (;;) {
        if (b.request != 0) {
            const int status = efonts_fetch_status(b.request);
            if (status < 0) return;  /* still in flight */
            std::vector<uint8_t> data;
            Failure failure;
            bool ok = false;
            take_fetch(b.request, status, data, failure, ok);
            b.request = 0;
            b.installer->fetched(ok, data, failure);
            continue;
        }
        if (b.wait_until > 0) {
            const double left = b.wait_until - emscripten_get_now();
            if (left > 0) {
                g_download.progress.waiting = int((left + 999) / 1000);
                return;
            }
            b.wait_until = 0;
            g_download.progress.waiting = 0;
            b.installer->waited();
            continue;
        }
        const Installer::Step step = b.installer->next();
        if (step.kind == Installer::Step::Kind::Finished) {
            const Outcome outcome = b.installer->outcome();
            b = BrowserDownload();
            report(outcome);
            return;
        }
        if (step.kind == Installer::Step::Kind::Wait) {
            b.wait_until = emscripten_get_now() + step.seconds * 1000.0;
            g_download.progress.waiting = step.seconds;
            return;
        }
        b.request = efonts_fetch_start(step.url.c_str());
        return;
    }
}

#else

/* The worker's whole job; it owns no state but g_download's atomics until
 * report takes the lock. */
void run_download() {
    size_t count = 0;
    const auto *files = enhanced_font_masks::pack_files(count);
    report(install(download_base(), files, count, enhanced_fonts::directory(), platform_transport(), g_download.cancel,
                   g_download.progress));
}

#endif

/* This build can fetch the files itself. */
bool can_download() {
#ifdef __EMSCRIPTEN__
    return true;
#else
    return bool(platform_transport());
#endif
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
    if (!can_download()) return PORT_EFONTS_UNSUPPORTED;
    return g_download.valid > 0 ? PORT_EFONTS_PARTIAL : PORT_EFONTS_MISSING;
}

extern "C" void portEnhancedFontsProgress(int *done, int *total) {
    const int state = portEnhancedFontsState();
    std::lock_guard<std::mutex> guard(g_download.lock);
    if (done) *done = state == PORT_EFONTS_DOWNLOADING ? g_download.progress.done.load() : g_download.valid;
    if (total) *total = int(pinned_count());
}

extern "C" const char *portEnhancedFontsError(void) {
    std::lock_guard<std::mutex> guard(g_download.lock);
    return g_download.error.c_str();
}

extern "C" void portEnhancedFontsDownload(void) {
    if (g_download.running || !can_download()) return;
    {
        std::lock_guard<std::mutex> guard(g_download.lock);
        g_download.failed = false;
        g_download.error.clear();
    }
    g_download.cancel = false;
    g_download.progress.done = 0;
    g_download.progress.waiting = 0;
    g_download.running = true;
#ifdef __EMSCRIPTEN__
    size_t count = 0;
    const auto *files = enhanced_font_masks::pack_files(count);
    g_browser.installer = std::make_unique<Installer>(download_base(), files, count, enhanced_fonts::directory(),
                                                      g_download.progress);
#else
    std::thread(run_download).detach();  /* quitting mid-download just abandons it; no partial file is used */
#endif
}

extern "C" void portEnhancedFontsPump(void) {
#ifdef __EMSCRIPTEN__
    pump();
#endif
}

extern "C" void portEnhancedFontsSetDirectory(const char *directory) {
    if (directory != nullptr && !g_download.running) {
        enhanced_fonts::set_directory(directory);
        std::lock_guard<std::mutex> guard(g_download.lock);
        g_download.valid = -1;
    }
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
        case PORT_EFONTS_DOWNLOADING:
            if (g_download.progress.waiting > 0) {
                std::snprintf(status, sizeof(status), "GitHub busy: retry in %ds\n", g_download.progress.waiting.load());
            } else {
                std::snprintf(status, sizeof(status), "Downloading: %d of %d\n", done, total);
            }
            break;
        case PORT_EFONTS_PARTIAL: std::snprintf(status, sizeof(status), "Incomplete: %d of %d files\n", done, total); break;
        case PORT_EFONTS_FAILED: std::snprintf(status, sizeof(status), "Failed: %s\n", portEnhancedFontsError()); break;
        case PORT_EFONTS_UNSUPPORTED: return "Copy to Data/FontPacks/gepd\n";
        default: return "Not installed.\n";
    }
    return status;
}
