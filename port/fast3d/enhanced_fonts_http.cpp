/* The Enhanced fonts' HTTPS transports (FORK-052, FONT-3): WinHTTP on
 * Windows and libcurl, loaded when first needed, on Linux. Other targets have
 * none yet (the browser's fetch() is FONT-4). Each fetch caps the body at
 * 1 MiB, gives up on cancel and reports a short reason. */
#include "enhanced_fonts_download.h"

#if defined(_WIN32) && !defined(GE_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <dlfcn.h>
#endif

namespace enhanced_fonts_download {
namespace {

constexpr size_t kMaxBody = 1u << 20;

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

bool winhttp_fetch(const std::string &url, std::vector<uint8_t> &out, std::string &error, const std::atomic<bool> &cancel) {
    out.clear();
    const std::wstring wide(url.begin(), url.end());  /* the URLs are ASCII */
    wchar_t host[256] = {0}, path[2048] = {0};
    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = 255;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2047;
    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts)) {
        error = "Bad address";
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
        error = winhttp_reason(GetLastError());
        return false;
    }
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        error = "Server error " + std::to_string(status);
        return false;
    }
    for (;;) {
        DWORD available = 0, read = 0;
        if (cancel) return false;
        if (!WinHttpQueryDataAvailable(request.h, &available)) {
            error = winhttp_reason(GetLastError());
            return false;
        }
        if (available == 0) return true;
        if (out.size() + available > kMaxBody) {
            error = "File too large";
            return false;
        }
        const size_t at = out.size();
        out.resize(at + available);
        if (!WinHttpReadData(request.h, out.data() + at, available, &read)) {
            error = winhttp_reason(GetLastError());
            return false;
        }
        out.resize(at + read);
    }
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

struct Sink {
    std::vector<uint8_t> *out;
    const std::atomic<bool> *cancel;
    bool too_large;
};

size_t curl_write(char *data, size_t size, size_t count, void *user) {
    Sink *sink = static_cast<Sink *>(user);
    const size_t n = size * count;
    if (sink->out->size() + n > kMaxBody) {
        sink->too_large = true;
        return 0;
    }
    sink->out->insert(sink->out->end(), data, data + n);
    return n;
}

int curl_progress(void *user, long long, long long, long long, long long) {
    return static_cast<Sink *>(user)->cancel->load() ? 1 : 0;
}

bool curl_fetch(const std::string &url, std::vector<uint8_t> &out, std::string &error, const std::atomic<bool> &cancel) {
    const Curl *curl = load_curl();
    out.clear();
    void *easy = curl ? curl->init() : nullptr;
    if (easy == nullptr) {
        error = "No libcurl";
        return false;
    }
    Sink sink{&out, &cancel, false};
    curl->setopt(easy, kOptUrl, url.c_str());
    curl->setopt(easy, kOptUserAgent, "IronSights");
    curl->setopt(easy, kOptFollow, 1L);
    curl->setopt(easy, kOptNoSignal, 1L);
    curl->setopt(easy, kOptConnectTimeout, 10L);
    curl->setopt(easy, kOptTimeout, 30L);
    curl->setopt(easy, kOptWriteFunction, &curl_write);
    curl->setopt(easy, kOptWriteData, &sink);
    curl->setopt(easy, kOptNoProgress, 0L);
    curl->setopt(easy, kOptXferInfoFunction, &curl_progress);
    curl->setopt(easy, kOptXferInfoData, &sink);
    const int code = curl->perform(easy);
    long status = 0;
    curl->getinfo(easy, kInfoResponseCode, &status);
    curl->cleanup(easy);
    if (code == 0 && status == 200) return true;
    if (sink.too_large) error = "File too large";
    else if (code == kErrResolve || code == kErrConnect) error = "No connection";
    else if (code == kErrTimeout) error = "Timed out";
    else if (code == kErrSsl || code == kErrPeerCert) error = "Secure link failed";
    else if (code == 0) error = "Server error " + std::to_string(status);
    else error = "Download failed";
    return false;
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

}  // namespace enhanced_fonts_download
