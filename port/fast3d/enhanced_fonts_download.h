#ifndef PORT_ENHANCED_FONTS_DOWNLOAD_H
#define PORT_ENHANCED_FONTS_DOWNLOAD_H
/* The Enhanced fonts' download (FORK-052, font styles plan FONT-3). The
 * engine fetches each pinned file that is not already installed through a
 * transport, checks its size and SHA-256, writes it under a temporary name
 * and renames it into place, then writes SOURCE.txt. The menus' C API is in
 * port/include/port_enhanced_fonts.h. */
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "enhanced_font_masks.h"

namespace enhanced_fonts_download {

/* Fetches url into out; false with a short reason on failure. It should give
 * up soon after cancel is set. */
using Transport = std::function<bool(const std::string &url, std::vector<uint8_t> &out, std::string &error,
                                     const std::atomic<bool> &cancel)>;

enum class Result { Done, Cancelled, Failed };

struct Outcome {
    Result result = Result::Failed;
    std::string error;
};

/* The pinned files' URL at the pack's commit; base ends in '/'. */
std::string file_url(const std::string &base, const enhanced_font_masks::PackFile &file);
/* The pack's own repository at the pinned commit. */
std::string default_base();

/* Installs files[0..count) into directory: each one already valid is kept,
 * the others fetched and verified. done is bumped per verified file. */
Outcome install(const std::string &base, const enhanced_font_masks::PackFile *files, size_t count,
                const std::string &directory, const Transport &transport, const std::atomic<bool> &cancel,
                std::atomic<int> &done);
/* Deletes the pinned files, their temporaries and SOURCE.txt from directory. */
void remove_files(const enhanced_font_masks::PackFile *files, size_t count, const std::string &directory);
/* How many of the pinned files in directory are valid. */
int count_valid(const enhanced_font_masks::PackFile *files, size_t count, const std::string &directory);

/* This platform's HTTPS transport, or an empty function when there is none
 * (no libcurl on Linux, or the browser until FONT-4). */
Transport platform_transport();

}  // namespace enhanced_fonts_download
#endif
