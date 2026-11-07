#ifndef MIDAS_HD_DOWNLOAD_ENGINE_H
#define MIDAS_HD_DOWNLOAD_ENGINE_H
/* The HD textures' download, one file at a time (FORK-053): stream each pinned
 * file not already in place to a temporary name while hashing it, check its
 * size and SHA-256, rename it into place, and ask a busy server again after a
 * wait. The transport and the wait are passed in, so tests need no network.
 * The menus' side is hd_textures_download.cpp. */
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>

#include "enhanced_fonts_download.h"

namespace hd_download {

struct PackFile {
    const char *folder;  /* under the pack folder; "" for the folder itself */
    const char *name;
    const char *url;
    long long size;
    const char *sha256;  /* lowercase hex */
};

/* The pinned files: the v2025-12-30 PNG release's ZIP and three Egyptian
 * textures from the repository. */
const PackFile *pack_files(int &count);

std::filesystem::path path_of(const std::string &directory, const PackFile &file);
/* Present at its pinned size (the bytes were checked when written). */
bool in_place(const std::string &directory, const PackFile &file);

/* What install reports while it runs. */
struct Progress {
    std::atomic<long long> bytes{0};  /* files in place plus the current one so far */
    std::atomic<int> waiting{0};      /* seconds before asking a busy server again */
};

/* Installs files[0..count) into directory, keeping those in place. Returns ""
 * when all are in place, "cancelled", or a short reason. A busy server (429,
 * 502, 503, 504) is asked again after its Retry-After or 2, 4, 8 and 16 s, five
 * tries a file; a wait over a minute or a fifth busy answer gives "GitHub busy". */
std::string install(const std::string &directory, const PackFile *files, int count,
                    const std::function<std::string(const PackFile &)> &url_of,
                    const enhanced_fonts_download::StreamTransport &transport, const enhanced_fonts_download::Wait &wait,
                    const std::atomic<bool> &cancel, Progress &progress);

/* Deletes the pinned files, their temporaries, SOURCE.txt and empty folders. */
void remove_files(const std::string &directory, const PackFile *files, int count);

}  // namespace hd_download
#endif
