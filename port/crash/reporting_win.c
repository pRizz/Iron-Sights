/* Windows-only reporting. No SDK import library or mandatory DLL dependency. */
#define WIN32_LEAN_AND_MEAN
#include <wchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "port_crash.h"
#include "port_buildinfo.h"
#include "port_sentry_config.h"

static wchar_t sRoot[MAX_PATH], sIni[MAX_PATH], sDatabase[MAX_PATH];
static int sPreference = 1, sSuppressed, sActive;
static LPTOP_LEVEL_EXCEPTION_FILTER sLocalFilter, sSdkFilter;
extern void portConLogf(const char *format, ...);

/* Only traverse this dedicated SDK cache; never follow junctions/symlinks. */
static void clearDatabase(const wchar_t *directory)
{
    WIN32_FIND_DATAW entry;
    wchar_t pattern[MAX_PATH], child[MAX_PATH];
    if (!directory[0] || wcslen(directory) + 4 >= MAX_PATH) return;
    DWORD attributes = GetFileAttributesW(directory);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return;
    swprintf(pattern, MAX_PATH, L"%ls\\*", directory);
    HANDLE search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(entry.cFileName, L".") || !wcscmp(entry.cFileName, L"..")) continue;
        if (wcslen(directory) + wcslen(entry.cFileName) + 2 >= MAX_PATH) continue;
        swprintf(child, MAX_PATH, L"%ls\\%ls", directory, entry.cFileName);
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            clearDatabase(child);
            RemoveDirectoryW(child);
        } else {
            DeleteFileW(child);
        }
    } while (FindNextFileW(search, &entry));
    FindClose(search);
}

#ifdef MIDAS_SENTRY_ENABLED
#include <sentry.h>

#define SENTRY_FUNCTIONS(X) \
    X(sentry_options_new) X(sentry_options_set_dsn) X(sentry_options_set_debug) \
    X(sentry_options_set_database_pathw) X(sentry_options_set_handler_pathw) \
    X(sentry_options_set_release) X(sentry_options_set_environment) \
    X(sentry_options_set_auto_session_tracking) X(sentry_options_set_require_user_consent) \
    X(sentry_options_set_crash_reporting_mode) X(sentry_options_set_symbolize_stacktraces) \
    X(sentry_options_set_cache_keep) X(sentry_options_set_cache_max_items) \
    X(sentry_options_set_cache_max_size) X(sentry_options_set_cache_max_age) \
    X(sentry_options_set_http_retry) X(sentry_options_set_transfer_timeout) \
    X(sentry_options_set_shutdown_timeout) X(sentry_options_set_on_crash) \
    X(sentry_init) X(sentry_close) X(sentry_user_consent_give) X(sentry_user_consent_revoke) \
    X(sentry_set_tag) \
    X(sentry_value_new_object) X(sentry_value_new_int32) X(sentry_value_set_by_key) \
    X(sentry_value_new_message_event) X(sentry_capture_event) X(sentry_flush)
#define DECLARE(name) static __typeof__(&name) p_##name;
SENTRY_FUNCTIONS(DECLARE)
#undef DECLARE
static HMODULE sSdk;

static int loadSdk(void)
{
    wchar_t path[MAX_PATH];
    if (sSdk) return 1;
    swprintf(path, MAX_PATH, L"%lssentry.dll", sRoot);
    HMODULE maybe_sdk = LoadLibraryExW(path, NULL,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!maybe_sdk) return 0;
#define RESOLVE(name) \
    p_##name = (__typeof__(p_##name))GetProcAddress(maybe_sdk, #name); \
    if (!p_##name) { FreeLibrary(maybe_sdk); return 0; }
    SENTRY_FUNCTIONS(RESOLVE)
#undef RESOLVE
    /* Keep the module loaded: filters can retain code pointers after shutdown. */
    sSdk = maybe_sdk;
    return 1;
}

static sentry_value_t crashContext(const sentry_ucontext_t *context,
    sentry_value_t event, sentry_hint_t *hint, void *data)
{
    extern int current_menu, g_MainStageNum;
    extern unsigned int g_PortFramesPumped;
    (void)context; (void)hint; (void)data;
    sentry_value_t details = p_sentry_value_new_object();
    p_sentry_value_set_by_key(details, "stage", p_sentry_value_new_int32(g_MainStageNum));
    p_sentry_value_set_by_key(details, "menu", p_sentry_value_new_int32(current_menu));
    p_sentry_value_set_by_key(details, "frame", p_sentry_value_new_int32(g_PortFramesPumped));
    p_sentry_value_set_by_key(event, "extra", details);
    return event;
}

static LONG WINAPI reportException(EXCEPTION_POINTERS *exception)
{
    /* The local StackWalk64 logger must receive a copy of the CPU context. */
    if (sLocalFilter) sLocalFilter(exception);
    if (sActive && sSdkFilter) return sSdkFilter(exception);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void startReporting(void)
{
    wchar_t handler[MAX_PATH];
    const char *maybe_dsn = getenv("GE_SENTRY_DSN");
    const char *maybe_environment = getenv("GE_SENTRY_ENVIRONMENT");
    if (!maybe_dsn) maybe_dsn = MIDAS_SENTRY_DSN;
    if (sActive || !sPreference || sSuppressed || !maybe_dsn[0] || !loadSdk()) return;
    swprintf(handler, MAX_PATH, L"%lssentry-crash.exe", sRoot);
    if (GetFileAttributesW(handler) == INVALID_FILE_ATTRIBUTES) return;
    sentry_options_t *options = p_sentry_options_new();
    p_sentry_options_set_dsn(options, maybe_dsn);
    p_sentry_options_set_debug(options, getenv("GE_SENTRY_DEBUG") != NULL);
    p_sentry_options_set_database_pathw(options, sDatabase);
    p_sentry_options_set_handler_pathw(options, handler);
    p_sentry_options_set_release(options, "iron-sights@" PORT_BUILD_SHA);
    p_sentry_options_set_environment(options, maybe_environment ? maybe_environment :
        (PORT_BUILD_CI ? "windows-release" : "windows-local"));
    p_sentry_options_set_auto_session_tracking(options, 0);
    p_sentry_options_set_require_user_consent(options, 1);
    p_sentry_options_set_crash_reporting_mode(options, SENTRY_CRASH_REPORTING_MODE_NATIVE);
    p_sentry_options_set_symbolize_stacktraces(options, 0);
    p_sentry_options_set_cache_keep(options, SENTRY_CACHE_KEEP_OFFLINE);
    p_sentry_options_set_cache_max_items(options, 10);
    p_sentry_options_set_cache_max_size(options, 5 * 1024 * 1024);
    p_sentry_options_set_cache_max_age(options, 7 * 24 * 60 * 60);
    p_sentry_options_set_http_retry(options, 1);
    p_sentry_options_set_transfer_timeout(options, 2000);
    p_sentry_options_set_shutdown_timeout(options, 2000);
    p_sentry_options_set_on_crash(options, crashContext, NULL);
    if (p_sentry_init(options) != 0) {
        portConLogf("ge: crash reporting unavailable; local diagnostics remain enabled\n");
        return;
    }
    sActive = 1;
    p_sentry_set_tag("build_dirty", PORT_BUILD_DIRTY ? "true" : "false");
    p_sentry_set_tag("build_time", PORT_BUILD_TIME);
    p_sentry_set_tag("build_number", PORT_BUILD_STRING);
    if (PORT_BUILD_RUN_URL[0]) p_sentry_set_tag("build_run", PORT_BUILD_RUN_URL);
    p_sentry_user_consent_give();
    sSdkFilter = SetUnhandledExceptionFilter(reportException);
    portConLogf("ge: crash reporting ON (Sentry); opt out in Port Options or [diagnostics] crash_reporting=0\n");
}
#endif

void portCrashShutdown(void)
{
#ifdef MIDAS_SENTRY_ENABLED
    if (!sActive) return;
    sActive = 0;
    sSdkFilter = NULL;
    p_sentry_close();
    SetUnhandledExceptionFilter(sLocalFilter);
#endif
}

void portCrashInit(LPTOP_LEVEL_EXCEPTION_FILTER local_filter)
{
    sLocalFilter = local_filter;
    DWORD length = GetModuleFileNameW(NULL, sRoot, MAX_PATH);
    if (!length || length >= MAX_PATH - 40) return;
    wchar_t *slash = wcsrchr(sRoot, L'\\');
    if (!slash) return;
    slash[1] = 0;
    swprintf(sIni, MAX_PATH, L"%lsmidas.ini", sRoot);
    /* Dedicated, executable-relative directory: test copies cannot share queues. */
    swprintf(sDatabase, MAX_PATH, L"%lsData\\CrashReports", sRoot);
    wchar_t data[MAX_PATH];
    swprintf(data, MAX_PATH, L"%lsData", sRoot);
    CreateDirectoryW(data, NULL);
    sPreference = GetPrivateProfileIntW(L"diagnostics", L"crash_reporting", 1, sIni) != 0;
    if ((GetFileAttributesW(data) & FILE_ATTRIBUTE_REPARSE_POINT) ||
        (GetFileAttributesW(sDatabase) != INVALID_FILE_ATTRIBUTES &&
         (GetFileAttributesW(sDatabase) & FILE_ATTRIBUTE_REPARSE_POINT))) return;
    const char *maybe_override = getenv("GE_CRASH_REPORTING");
    sSuppressed = (maybe_override && !strcmp(maybe_override, "0")) ||
        getenv("GE_HAND_GALLERY") != NULL || getenv("GE_NOINISAVE") != NULL ||
        getenv("GE_NOINPUT") != NULL;
    if (!sPreference) clearDatabase(sDatabase);
#ifdef MIDAS_SENTRY_ENABLED
    startReporting();
#endif
    atexit(portCrashShutdown);
    portConLogf("ge: crash reporting preference=%s active=%d\n", sPreference ? "ON" : "OFF", sActive);
}

int portCrashOptionGet(const char *key, int *out)
{
    if (strcmp(key, "crash_reporting")) return 0;
    *out = sPreference;
    return 1;
}

int portCrashOptionSet(const char *key, int value)
{
    if (strcmp(key, "crash_reporting")) return 0;
    int saved = sIni[0] && WritePrivateProfileStringW(L"diagnostics", L"crash_reporting", value ? L"1" : L"0", sIni);
    if (!saved && value) return 0;
    if (!saved) portConLogf("ge: unable to save crash-reporting opt-out; reporting is OFF for this run\n");
    sPreference = value != 0;
    if (!sPreference) {
#ifdef MIDAS_SENTRY_ENABLED
        if (sActive) p_sentry_user_consent_revoke();
#endif
        portCrashShutdown();
        clearDatabase(sDatabase);
    } else {
#ifdef MIDAS_SENTRY_ENABLED
        startReporting();
#endif
    }
    return 1;
}

#ifdef MIDAS_SENTRY_ENABLED
static DWORD WINAPI reportStallWorker(void *unused)
{
    (void)unused;
    sentry_value_t event = p_sentry_value_new_message_event(SENTRY_LEVEL_ERROR,
        "watchdog", "Game watchdog detected a persistent stall");
    p_sentry_capture_event(event);
    p_sentry_flush(1000);
    return 0;
}
#endif

void portCrashReportStall(void)
{
#ifdef MIDAS_SENTRY_ENABLED
    if (!sActive) return;
    /* The stalled thread may hold a heap/SDK lock. Bound the reporting attempt
     * so the existing watchdog can still exit even if SDK work cannot finish. */
    HANDLE maybe_worker = CreateThread(NULL, 0, reportStallWorker, NULL, 0, NULL);
    if (maybe_worker) {
        WaitForSingleObject(maybe_worker, 1500);
        CloseHandle(maybe_worker);
    }
#endif
}

__attribute__((noinline)) static void testCrashFault(volatile int *address)
{
    *address = 42; /* Intentional ROM-free crash: verify this source line in Sentry. */
}

int portCrashTestMode(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--test-stall-reporting")) {
            portCrashReportStall();
            return 1;
        }
        if (!strcmp(argv[i], "--test-crash-reporting") || !strcmp(argv[i], "--test-crash-reporting-optout")) {
            if (!strcmp(argv[i], "--test-crash-reporting-optout")) portCrashOptionSet("crash_reporting", 0);
            portConLogf("ge: intentional crash reporting test active=%d release=iron-sights@%s\n", sActive, PORT_BUILD_SHA);
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
            testCrashFault((volatile int *)(uintptr_t)1);
        }
        if (!strcmp(argv[i], "--check-crash-reporting")) {
            portConLogf("ge: crash reporting check active=%d preference=%d\n", sActive, sPreference);
            return 1;
        }
    }
    return 0;
}
