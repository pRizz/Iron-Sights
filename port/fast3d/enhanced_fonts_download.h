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

/* Why a fetch failed: a short reason and, when the server was busy (429,
 * 502, 503 or 504), the wait its Retry-After asked for in seconds (0 when it
 * named none). retry_after stays -1 for any other failure. */
struct Failure {
    std::string reason;
    int retry_after = -1;
};

/* Fetches url into out; false with a Failure otherwise. It should give up soon
 * after cancel is set. */
using Transport = std::function<bool(const std::string &url, std::vector<uint8_t> &out, Failure &failure,
                                     const std::atomic<bool> &cancel)>;

/* What install reports while it runs. */
struct Progress {
    std::atomic<int> done{0};     /* files verified */
    std::atomic<int> waiting{0};  /* seconds left before asking a busy server again; 0 = not waiting */
};

/* Waits seconds for a busy server, counting progress.waiting down; false once
 * cancel is set. */
using Wait = std::function<bool(int seconds, std::atomic<int> &waiting, const std::atomic<bool> &cancel)>;
/* The real wait: sleeps in short steps so cancel answers quickly. */
Wait sleeping_wait();

enum class Result { Done, Cancelled, Failed };

struct Outcome {
    Result result = Result::Failed;
    std::string error;
};

/* The pinned files' URL at the pack's commit; base ends in '/'. */
std::string file_url(const std::string &base, const enhanced_font_masks::PackFile &file);
/* The pack's own repository at the pinned commit. */
std::string default_base();

/* The download one step at a time, for a caller that cannot block (the
 * browser drives it a frame at a time); install() drives it with a blocking
 * transport and wait. Ask next() what to do, then report it with fetched() or
 * waited(). It keeps each valid file, verifies each fetched one, writes it as
 * .part and renames it into place, and writes SOURCE.txt last. */
class Installer {
  public:
    struct Step {
        enum class Kind { Fetch, Wait, Finished };
        Kind kind = Kind::Finished;
        std::string url;  /* Fetch */
        int seconds = 0;  /* Wait */
    };

    Installer(std::string base, const enhanced_font_masks::PackFile *files, size_t count, std::string directory,
              Progress &progress);
    /* What to do now; asking again before answering returns the same step. */
    Step next();
    /* The answer to a Fetch step. */
    void fetched(bool ok, const std::vector<uint8_t> &data, const Failure &failure);
    /* The answer to a Wait step. */
    void waited();
    /* How it ended, once next() says Finished. */
    const Outcome &outcome() const { return outcome_; }

  private:
    void finish(Result result, std::string error);

    std::string base_, directory_;
    const enhanced_font_masks::PackFile *files_;
    size_t count_;
    Progress &progress_;
    size_t index_ = 0;
    bool checked_ = false, waiting_ = false, finished_ = false;
    int attempt_ = 1, backoff_ = 0, wait_seconds_ = 0;
    Outcome outcome_;
};

/* Installs files[0..count) into directory: each one already valid is kept,
 * the others fetched and verified. progress.done is bumped per verified file.
 * While the server is busy a file is asked for again after the wait it names,
 * or after 2, 4, 8 and 16 seconds, five tries in all; a wait over a minute,
 * or a fifth busy answer, fails with "GitHub busy". */
Outcome install(const std::string &base, const enhanced_font_masks::PackFile *files, size_t count,
                const std::string &directory, const Transport &transport, const std::atomic<bool> &cancel,
                Progress &progress, const Wait &wait = sleeping_wait());
/* Deletes the pinned files, their temporaries and SOURCE.txt from directory. */
void remove_files(const enhanced_font_masks::PackFile *files, size_t count, const std::string &directory);
/* How many of the pinned files in directory are valid. */
int count_valid(const enhanced_font_masks::PackFile *files, size_t count, const std::string &directory);

/* This platform's blocking HTTPS transport, or an empty function when there
 * is none (no libcurl on Linux; the browser fetches through its own pump in
 * the menus' C API instead). */
Transport platform_transport();

/* Hands a body's bytes on as they arrive; false stops the transfer. */
using Sink = std::function<bool(const uint8_t *data, size_t size)>;
/* Streams url's body to sink, for one large file (the HD texture pack,
 * FORK-053): no size cap, and it gives up when the connection stalls rather
 * than after a fixed time. Failures are reported as for Transport. */
using StreamTransport =
    std::function<bool(const std::string &url, const Sink &sink, Failure &failure, const std::atomic<bool> &cancel)>;
/* The streaming form of platform_transport(); empty where there is none. */
StreamTransport platform_stream_transport();

}  // namespace enhanced_fonts_download
#endif
