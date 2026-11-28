/* The in-game updater's HTTPS transport (port_update.h): the Enhanced Fonts
 * and HD textures stream transport (port/fast3d/enhanced_fonts_http.cpp,
 * WinHTTP or libcurl), run on a worker thread so the owner's limits hold
 * whatever the transport's own timeouts are: a small file gets
 * PORT_UPDATE_CHECK_SECONDS in all, a package is given up after
 * PORT_UPDATE_STALL_SECONDS without data. A worker that has not stopped by
 * then is cancelled and left to finish on its own; it writes only into its
 * shared state, never into the caller's sink. Both transports follow GitHub's
 * release redirects. Built without the game's include folders, as the
 * transport is (CMakeLists.txt). */
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../include/port_update.h"
#include "../fast3d/enhanced_fonts_download.h"

namespace {

using Clock = std::chrono::steady_clock;

struct Shared {
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::vector<unsigned char>> chunks;
    bool finished = false, ok = false, refused = false;
    std::string reason;
    std::atomic<bool> cancel{false};
};

void work(std::shared_ptr<Shared> shared, std::string url, enhanced_fonts_download::StreamTransport transport) {
    enhanced_fonts_download::Failure failure;
    const bool ok = transport(url, [&](const uint8_t *data, size_t size) {
        std::lock_guard<std::mutex> lock(shared->mutex);
        if (shared->refused || shared->cancel) return false;
        shared->chunks.emplace_back(data, data + size);
        shared->changed.notify_one();
        return true;
    }, failure, shared->cancel);
    std::lock_guard<std::mutex> lock(shared->mutex);
    shared->finished = true;
    shared->ok = ok;
    shared->reason = failure.reason;
    shared->changed.notify_one();
}

void copyReason(const std::string &reason, char *out, size_t size) {
    if (size > 0) std::snprintf(out, size, "%s", reason.c_str());
}

int fetch(const char *url, int stream, PortUpdateSink sink, void *user, char *reason, size_t reasonsize) {
    auto transport = enhanced_fonts_download::platform_stream_transport();
    if (!transport) {
        copyReason("No HTTPS transport", reason, reasonsize);
        return 0;
    }
    auto shared = std::make_shared<Shared>();
    std::thread(work, shared, std::string(url), transport).detach();
    const auto limit = std::chrono::seconds(stream ? PORT_UPDATE_STALL_SECONDS : PORT_UPDATE_CHECK_SECONDS);
    auto deadline = Clock::now() + limit;
    std::unique_lock<std::mutex> lock(shared->mutex);
    for (;;) {
        while (!shared->chunks.empty()) {
            std::vector<unsigned char> chunk = std::move(shared->chunks.front());
            shared->chunks.pop_front();
            lock.unlock();
            const int kept = sink(user, chunk.data(), chunk.size());
            lock.lock();
            if (!kept) {
                shared->refused = true;
                shared->cancel = true;
                copyReason("Refused by the updater", reason, reasonsize);
                return 0;
            }
            if (stream) deadline = Clock::now() + limit;
        }
        if (shared->finished) {
            if (!shared->ok) copyReason(shared->reason.empty() ? "Download failed" : shared->reason, reason, reasonsize);
            return shared->ok ? 1 : 0;
        }
        if (shared->changed.wait_until(lock, deadline) == std::cv_status::timeout && shared->chunks.empty() &&
            !shared->finished) {
            shared->cancel = true;
            copyReason(stream ? "Download stalled" : "Timed out", reason, reasonsize);
            return 0;
        }
    }
}

}  // namespace

extern "C" PortUpdateHttp portUpdatePlatformHttp(void) {
    return enhanced_fonts_download::platform_stream_transport() ? fetch : nullptr;
}
