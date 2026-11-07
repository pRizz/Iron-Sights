/* The HTTPS transports (FORK-052, FONT-3; streaming for FORK-053): WinHTTP
 * on Windows and libcurl, loaded when first needed, on Linux. Other targets
 * have none (the browser's fetch() is FONT-4). A font fetch caps the body at
 * 1 MiB; a stream hands every byte to its sink. Both give up on cancel and
 * report a short reason; a busy server's answer also carries its Retry-After,
 * for the callers' backoff. */
#include "enhanced_fonts_download.h"

#if defined(_WIN32) && !defined(GE_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <cwchar>
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <dlfcn.h>
#endif

namespace enhanced_fonts_download {
namespace {

constexpr size_t kMaxBody = 1u << 20;

#if (defined(_WIN32) && !defined(GE_XBOX)) || (defined(__linux__) && !defined(__EMSCRIPTEN__))
/* Answers worth asking again after a wait: rate limited, or the server or its
 * gateway briefly unavailable. */
bool busy_status(long status) {
    return status == 429 || status == 502 || status == 503 || status == 504;
}

/* A failure for an HTTP status; a busy one keeps the server's wait, 0 when it
 * named none. */
void status_failure(long status, long retry_after, Failure &failure) {
    failure.reason = "Server error " + std::to_string(status);
    if (busy_status(status)) failure.retry_after = retry_after > 0 ? int(retry_after < 3600 ? retry_after : 3600) : 0;
}
#endif

#if defined(_WIN32) && !defined(GE_XBOX)

struct Handle {
    HINTERNET h = nullptr;
    ~Handle() {
        if (h) WinHttpCloseHandle(h);
    }
};

const char *winhttp_reason(DWORD error) {
    switch (error) {
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
        case ERROR_WINHTTP_CANNOT_CONNECT:
        case ERROR_WINHTTP_CONNECTION_ERROR: return "No connection";
        case ERROR_WINHTTP_TIMEOUT: return "Timed out";
        case ERROR_WINHTTP_SECURE_FAILURE: return "Secure link failed";
        default: return "Download failed";
    }
}

bool winhttp_get(const std::string &url, const Sink &sink, Failure &failure, const std::atomic<bool> &cancel) {
    const std::wstring wide(url.begin(), url.end());  /* the URLs are ASCII */
    wchar_t host[256] = {0}, path[2048] = {0};
    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = 255;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2047;
    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts)) {
        failure.reason = "Bad address";
        return false;
    }
    Handle session, connection, request;
    session.h = WinHttpOpen(L"IronSights", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session.h) WinHttpSetTimeouts(session.h, 10000, 10000, 15000, 15000);
    if (session.h) connection.h = WinHttpConnect(session.h, host, parts.nPort, 0);
    if (connection.h) {
        request.h = WinHttpOpenRequest(connection.h, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    }
    if (!request.h || !WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        || !WinHttpReceiveResponse(request.h, nullptr)) {
        failure.reason = winhttp_reason(GetLastError());
        return false;
    }
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        /* Retry-After as text (the numeric query refuses it): seconds; a date
         * form reads as none */
        wchar_t text[32] = {0};
        long retry_after = 0;
        size = sizeof(text) - sizeof(text[0]);
        if (WinHttpQueryHeaders(request.h, WINHTTP_QUERY_RETRY_AFTER, WINHTTP_HEADER_NAME_BY_INDEX, text, &size,
                                WINHTTP_NO_HEADER_INDEX)) {
            retry_after = std::wcstol(text, nullptr, 10);
        }
        status_failure(long(status), retry_after, failure);
        return false;
    }
    std::vector<uint8_t> chunk(1u << 16);
    for (;;) {
        DWORD available = 0, read = 0;
        if (cancel) return false;
        if (!WinHttpQueryDataAvailable(request.h, &available)) {
            failure.reason = winhttp_reason(GetLastError());
            return false;
        }
        if (available == 0) return true;
        if (!WinHttpReadData(request.h, chunk.data(), DWORD(available < chunk.size() ? available : chunk.size()), &read)) {
            failure.reason = winhttp_reason(GetLastError());
            return false;
        }
        if (!sink(chunk.data(), read)) return false;
    }
}

bool winhttp_fetch(const std::string &url, std::vector<uint8_t> &out, Failure &failure, const std::atomic<bool> &cancel) {
    out.clear();
    bool too_large = false;
    const bool ok = winhttp_get(url, [&](const uint8_t *data, size_t size) {
        if (out.size() + size > kMaxBody) return !(too_large = true);
        out.insert(out.end(), data, data + size);
        return true;
    }, failure, cancel);
    if (too_large) failure.reason = "File too large";
    return ok && !too_large;
}

#elif defined(__linux__) && !defined(__EMSCRIPTEN__)

/* The few libcurl entry points and constants used, so the game needs no
 * libcurl to start: curl.h's values, stable across libcurl 7 and 8. */
struct Curl {
    void *(*init)() = nullptr;
    int (*setopt)(void *, int, ...) = nullptr;
    int (*perform)(void *) = nullptr;
    int (*getinfo)(void *, int, ...) = nullptr;
    void (*cleanup)(void *) = nullptr;
};
enum {
    kOptWriteData = 10001, kOptUrl = 10002, kOptUserAgent = 10018, kOptTimeout = 13, kOptNoProgress = 43,
    kOptFollow = 52, kOptConnectTimeout = 78, kOptNoSignal = 99, kOptXferInfoData = 10057,
    kOptWriteFunction = 20011, kOptXferInfoFunction = 20219, kInfoResponseCode = 0x200002,
    kOptLowSpeedLimit = 19, kOptLowSpeedTime = 20,
    kInfoRetryAfter = 0x600000 + 57,  /* CURLINFO_RETRY_AFTER, libcurl 7.66 on; older ones refuse it */
    kErrResolve = 6, kErrConnect = 7, kErrTimeout = 28, kErrSsl = 35, kErrPeerCert = 60,
};

const Curl *load_curl() {
    static Curl curl;
    static bool tried = false;
    if (!tried) {
        tried = true;
        void *lib = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
        if (lib == nullptr) lib = dlopen("libcurl-gnutls.so.4", RTLD_NOW | RTLD_LOCAL);
        if (lib != nullptr) {
            curl.init = reinterpret_cast<void *(*)()>(dlsym(lib, "curl_easy_init"));
            curl.setopt = reinterpret_cast<int (*)(void *, int, ...)>(dlsym(lib, "curl_easy_setopt"));
            curl.perform = reinterpret_cast<int (*)(void *)>(dlsym(lib, "curl_easy_perform"));
            curl.getinfo = reinterpret_cast<int (*)(void *, int, ...)>(dlsym(lib, "curl_easy_getinfo"));
            curl.cleanup = reinterpret_cast<void (*)(void *)>(dlsym(lib, "curl_easy_cleanup"));
        }
    }
    return (curl.init && curl.setopt && curl.perform && curl.getinfo && curl.cleanup) ? &curl : nullptr;
}

struct Receiver {
    const Sink *sink;
    const std::atomic<bool> *cancel;
    bool stopped;
};

size_t curl_write(char *data, size_t size, size_t count, void *user) {
    Receiver *receiver = static_cast<Receiver *>(user);
    const size_t n = size * count;
    if (!(*receiver->sink)(reinterpret_cast<const uint8_t *>(data), n)) {
        receiver->stopped = true;
        return 0;
    }
    return n;
}

int curl_progress(void *user, long long, long long, long long, long long) {
    return static_cast<Receiver *>(user)->cancel->load() ? 1 : 0;
}

/* stream: no overall time limit, only a stall limit, for one large file. */
bool curl_get(const std::string &url, const Sink &sink_fn, Failure &failure, const std::atomic<bool> &cancel, bool stream) {
    const Curl *curl = load_curl();
    void *easy = curl ? curl->init() : nullptr;
    if (easy == nullptr) {
        failure.reason = "No libcurl";
        return false;
    }
    Receiver sink{&sink_fn, &cancel, false};
    curl->setopt(easy, kOptUrl, url.c_str());
    curl->setopt(easy, kOptUserAgent, "IronSights");
    curl->setopt(easy, kOptFollow, 1L);
    curl->setopt(easy, kOptNoSignal, 1L);
    curl->setopt(easy, kOptConnectTimeout, 10L);
    if (stream) {
        curl->setopt(easy, kOptLowSpeedLimit, 1L);   /* under a byte a second ... */
        curl->setopt(easy, kOptLowSpeedTime, 30L);   /* ... for 30 s gives up */
    } else {
        curl->setopt(easy, kOptTimeout, 30L);
    }
    curl->setopt(easy, kOptWriteFunction, &curl_write);
    curl->setopt(easy, kOptWriteData, &sink);
    curl->setopt(easy, kOptNoProgress, 0L);
    curl->setopt(easy, kOptXferInfoFunction, &curl_progress);
    curl->setopt(easy, kOptXferInfoData, &sink);
    const int code = curl->perform(easy);
    long status = 0;
    long long retry_after = 0;
    curl->getinfo(easy, kInfoResponseCode, &status);
    if (curl->getinfo(easy, kInfoRetryAfter, &retry_after) != 0) retry_after = 0;
    curl->cleanup(easy);
    if (code == 0 && status == 200) return true;
    if (sink.stopped) failure.reason = "File too large";
    else if (code == kErrResolve || code == kErrConnect) failure.reason = "No connection";
    else if (code == kErrTimeout) failure.reason = "Timed out";
    else if (code == kErrSsl || code == kErrPeerCert) failure.reason = "Secure link failed";
    else if (code == 0) status_failure(status, long(retry_after), failure);
    else failure.reason = "Download failed";
    return false;
}

bool curl_fetch(const std::string &url, std::vector<uint8_t> &out, Failure &failure, const std::atomic<bool> &cancel) {
    out.clear();
    return curl_get(url, [&](const uint8_t *data, size_t size) {
        if (out.size() + size > kMaxBody) return false;
        out.insert(out.end(), data, data + size);
        return true;
    }, failure, cancel, false);
}

bool curl_stream(const std::string &url, const Sink &sink, Failure &failure, const std::atomic<bool> &cancel) {
    return curl_get(url, sink, failure, cancel, true);
}

#endif

}  // namespace

Transport platform_transport() {
#if defined(_WIN32) && !defined(GE_XBOX)
    return winhttp_fetch;
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
    if (load_curl() == nullptr) return Transport();
    return curl_fetch;
#else
    return Transport();
#endif
}

StreamTransport platform_stream_transport() {
#if defined(_WIN32) && !defined(GE_XBOX)
    return winhttp_get;
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
    if (load_curl() == nullptr) return StreamTransport();
    return curl_stream;
#else
    return StreamTransport();
#endif
}

}  // namespace enhanced_fonts_download
