/* The HD textures' download, one file at a time (FORK-053). See
 * hd_download_engine.h. */
#include "hd_download_engine.h"

#include <fstream>
#include <system_error>

#include "third_party/picosha2/picosha2.h"

namespace hd_download {
namespace {

using enhanced_fonts_download::Failure;

#define HD_REPO "https://raw.githubusercontent.com/GhostlyDark/GoldenEye-007-HD/f16ed0acb0b9b5ca5f7dad90dc6d5ae0f18c0ead"
#define HD_EGYPTIAN HD_REPO "/GOLDENEYE/Areas/Mission%2020%20-%20Egyptian/GOLDENEYE%23"
/* Pinned to release v2025-12-30 (its SHA-256 matches GitHub's own digest)
 * and the repository revision the scope inventory was taken from; the
 * Egyptian files are absent from the release's ZIP. */
const PackFile kFiles[] = {
    {"", "ge007-hd-v2025.12.30-gliden64-png-hd.zip",
     "https://github.com/GhostlyDark/GoldenEye-007-HD/releases/download/v2025-12-30/"
     "ge007-hd-v2025.12.30-gliden64-png-hd.zip",
     135256079, "a72d082e23b516f28f4868b0b32cbb676a5b0ce7f00e7eb79badf89f71ab90ba"},
    {"ge007-hd-egyptian", "GOLDENEYE#1AA14C7C#2#1#F67C45E4_ciByRGBA.png", HD_EGYPTIAN "1AA14C7C%232%231%23F67C45E4_ciByRGBA.png",
     980101, "a10e078a7e96734541b3967c6acfd59e806f1f9c21b9e769cc357fc51eefd9b1"},
    {"ge007-hd-egyptian", "GOLDENEYE#6F6FF03F#2#1#8901ED54_ciByRGBA.png", HD_EGYPTIAN "6F6FF03F%232%231%238901ED54_ciByRGBA.png",
     7042567, "f30c1ec5d95206e342c4bc83735032fc29535d68a3226dde853770a85086c491"},
    {"ge007-hd-egyptian", "GOLDENEYE#79A7995C#2#1#9C0E73FE_ciByRGBA.png", HD_EGYPTIAN "79A7995C%232%231%239C0E73FE_ciByRGBA.png",
     7126484, "498472fdad06c5db6f3cdf1d7ef70c8afe6c44951242882f473fd23bab01aeb9"},
};
constexpr int kTries = 5;
constexpr int kLongestWait = 60;

/* One file, streamed to .part with its hash, verified and renamed into place.
 * "" on success, otherwise "cancelled" or the reason. */
std::string fetch_one(const std::string &directory, const PackFile &file, const std::string &url,
                      const enhanced_fonts_download::StreamTransport &transport, const enhanced_fonts_download::Wait &wait,
                      const std::atomic<bool> &cancel, Progress &progress, long long base) {
    const std::filesystem::path target = path_of(directory, file);
    std::filesystem::path part = target;
    part += ".part";
    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    int backoff = 2;
    for (int attempt = 1;; attempt++) {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        if (!out) return "Can't write the pack folder";
        picosha2::hash256_one_by_one hasher;
        hasher.init();
        long long got = 0;
        bool write_failed = false, too_long = false;
        Failure failure;
        const bool ok = transport(url, [&](const uint8_t *data, size_t size) {
            if (got + (long long) size > file.size) return !(too_long = true);  /* not the pinned file */
            out.write(reinterpret_cast<const char *>(data), std::streamsize(size));
            if (!out) return !(write_failed = true);
            hasher.process(data, data + size);
            got += (long long) size;
            progress.bytes = base + got;
            return true;
        }, failure, cancel);
        out.close();
        if (cancel) {
            std::filesystem::remove(part, error);
            return "cancelled";
        }
        if (ok && !too_long && got == file.size) {
            hasher.finish();
            if (picosha2::get_hash_hex_string(hasher) != file.sha256) {
                std::filesystem::remove(part, error);
                return "Checksum mismatch";
            }
            std::filesystem::remove(target, error);
            std::filesystem::rename(part, target, error);
            return error ? "Can't write the pack folder" : "";
        }
        std::filesystem::remove(part, error);
        progress.bytes = base;
        if (write_failed) return "Disk full or read-only";
        if (ok || too_long) return "Wrong file size";
        const int seconds = failure.retry_after > 0 ? failure.retry_after : backoff;
        if (failure.retry_after < 0) return failure.reason;
        if (attempt >= kTries || seconds > kLongestWait) return "GitHub busy";
        if (!wait(seconds, progress.waiting, cancel)) return "cancelled";
        backoff *= 2;
    }
}

}  // namespace

const PackFile *pack_files(int &count) {
    count = int(sizeof(kFiles) / sizeof(kFiles[0]));
    return kFiles;
}

std::filesystem::path path_of(const std::string &directory, const PackFile &file) {
    std::filesystem::path p(directory);
    if (file.folder[0] != '\0') p /= file.folder;
    return p / file.name;
}

bool in_place(const std::string &directory, const PackFile &file) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path_of(directory, file), error);
    return !error && (long long) size == file.size;
}

std::string install(const std::string &directory, const PackFile *files, int count,
                    const std::function<std::string(const PackFile &)> &url_of,
                    const enhanced_fonts_download::StreamTransport &transport, const enhanced_fonts_download::Wait &wait,
                    const std::atomic<bool> &cancel, Progress &progress) {
    long long base = 0;
    for (int i = 0; i < count; i++) {
        if (!in_place(directory, files[i])) {
            const std::string why = fetch_one(directory, files[i], url_of(files[i]), transport, wait, cancel, progress, base);
            if (!why.empty()) return why;
        }
        base += files[i].size;
        progress.bytes = base;
    }
    return "";
}

void remove_files(const std::string &directory, const PackFile *files, int count) {
    std::error_code error;
    for (int i = 0; i < count; i++) {
        const auto target = path_of(directory, files[i]);
        std::filesystem::path part = target;
        part += ".part";
        std::filesystem::remove(target, error);
        std::filesystem::remove(part, error);
        if (files[i].folder[0] != '\0') std::filesystem::remove(target.parent_path(), error);  /* only if empty */
    }
    std::filesystem::remove(std::filesystem::path(directory) / "SOURCE.txt", error);
}

}  // namespace hd_download
