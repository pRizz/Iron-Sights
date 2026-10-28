/* Linux adapter: optional executable-relative SDK, writable-root preferences. */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "port_crash.h"
#include "port_buildinfo.h"
#include "port_sentry_config.h"

extern const char *portRootPath(const char *name);
extern void portConLogf(const char *format, ...);
extern int portIniWriteStrs(const char *, const char *const *, const char *const *, int);
static int sPreference = 1, sActive, sSuppressed;
static char sDatabase[1400], sExecutableDir[1200];

/* Walk only opened directories, with no symlink traversal, even during races. */
static void clearDirectory(int fd)
{
    DIR *maybe_dir = fdopendir(fd);
    if (!maybe_dir) { close(fd); return; }
    struct dirent *entry;
    while ((entry = readdir(maybe_dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        struct stat info;
        if (fstatat(fd, entry->d_name, &info, AT_SYMLINK_NOFOLLOW)) continue;
        if (S_ISDIR(info.st_mode)) {
            int child = openat(fd, entry->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
            if (child >= 0) clearDirectory(child);
            unlinkat(fd, entry->d_name, AT_REMOVEDIR);
        } else if (!S_ISLNK(info.st_mode)) unlinkat(fd, entry->d_name, 0);
    }
    closedir(maybe_dir);
}

static void clearDatabase(void)
{
    int data = open(portRootPath("Data"), O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (data < 0) return;
    int reports = openat(data, "CrashReports", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (reports >= 0) clearDirectory(reports);
    close(data);
}

static char *trim(char *text)
{
    text += strspn(text, " \t\r\n");
    size_t length = strlen(text);
    while (length && strchr(" \t\r\n", text[length - 1])) text[--length] = 0;
    return text;
}

static int readPreference(void)
{
    FILE *maybe_file = fopen(portRootPath("midas.ini"), "r");
    if (!maybe_file) return 1;
    char line[4096];
    int section = 0, preference = 1;
    while (fgets(line, sizeof(line), maybe_file)) {
        char *text = trim(line);
        if (*text == '[') { section = !strcasecmp(text, "[diagnostics]"); continue; }
        char *maybe_equals = strchr(text, '=');
        if (!section || !maybe_equals) continue;
        *maybe_equals++ = 0;
        if (!strcasecmp(trim(text), "crash_reporting")) preference = atoi(trim(maybe_equals)) != 0;
    }
    fclose(maybe_file);
    return preference;
}

#ifdef MIDAS_SENTRY_ENABLED
#include <sentry.h>
#define SENTRY_FUNCTIONS(X) \
    X(sentry_options_new) X(sentry_options_set_dsn) X(sentry_options_set_debug) \
    X(sentry_options_set_database_path) X(sentry_options_set_handler_path) \
    X(sentry_options_set_release) X(sentry_options_set_environment) \
    X(sentry_options_set_auto_session_tracking) X(sentry_options_set_require_user_consent) \
    X(sentry_options_set_crash_reporting_mode) X(sentry_options_set_symbolize_stacktraces) \
    X(sentry_options_set_cache_keep) X(sentry_options_set_cache_max_items) \
    X(sentry_options_set_cache_max_size) X(sentry_options_set_cache_max_age) \
    X(sentry_options_set_http_retry) X(sentry_options_set_transfer_timeout) \
    X(sentry_options_set_shutdown_timeout) X(sentry_options_set_on_crash) \
    X(sentry_init) X(sentry_close) X(sentry_user_consent_give) X(sentry_user_consent_revoke) \
    X(sentry_set_tag) X(sentry_value_new_object) X(sentry_value_new_int32) X(sentry_value_set_by_key)
#define DECLARE(name) static __typeof__(&name) p_##name;
SENTRY_FUNCTIONS(DECLARE)
#undef DECLARE
static void *sSdk;

static int loadSdk(void)
{
    if (sSdk) return 1;
    char path[1400];
    snprintf(path, sizeof(path), "%slib/libsentry.so", sExecutableDir);
    void *maybe_sdk = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!maybe_sdk) return 0;
#define RESOLVE(name) \
    p_##name = (__typeof__(p_##name))dlsym(maybe_sdk, #name); \
    if (!p_##name) { dlclose(maybe_sdk); return 0; }
    SENTRY_FUNCTIONS(RESOLVE)
#undef RESOLVE
    sSdk = maybe_sdk; /* Never unload code still referenced by a signal handler. */
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

static void startReporting(void)
{
    const char *maybe_dsn = getenv("GE_SENTRY_DSN");
    const char *maybe_environment = getenv("GE_SENTRY_ENVIRONMENT");
    if (!maybe_dsn) maybe_dsn = MIDAS_SENTRY_DSN;
    if (sActive || !sPreference || sSuppressed || !maybe_dsn[0] || !loadSdk()) return;
    char handler[1400];
    snprintf(handler, sizeof(handler), "%ssentry-crash", sExecutableDir);
    if (access(handler, X_OK)) return;
    sentry_options_t *options = p_sentry_options_new();
    p_sentry_options_set_dsn(options, maybe_dsn);
    p_sentry_options_set_debug(options, getenv("GE_SENTRY_DEBUG") != NULL);
    p_sentry_options_set_database_path(options, sDatabase);
    p_sentry_options_set_handler_path(options, handler);
    p_sentry_options_set_release(options, "iron-sights@" PORT_BUILD_SHA);
    p_sentry_options_set_environment(options, maybe_environment ? maybe_environment :
        (PORT_BUILD_CI ? "linux-release" : "linux-local"));
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
    if (p_sentry_init(options)) return;
    sActive = 1;
    p_sentry_set_tag("build_dirty", PORT_BUILD_DIRTY ? "true" : "false");
    p_sentry_set_tag("build_time", PORT_BUILD_TIME);
    p_sentry_set_tag("build_number", PORT_BUILD_STRING);
    if (PORT_BUILD_RUN_URL[0]) p_sentry_set_tag("build_run", PORT_BUILD_RUN_URL);
    p_sentry_set_tag("architecture", "i686");
    p_sentry_user_consent_give();
}
#endif

void portCrashShutdown(void)
{
#ifdef MIDAS_SENTRY_ENABLED
    if (sActive) { sActive = 0; p_sentry_close(); }
#endif
}

void portCrashInit(void)
{
    ssize_t length = readlink("/proc/self/exe", sExecutableDir, sizeof(sExecutableDir) - 1);
    if (length <= 0 || length >= sizeof(sExecutableDir) - 1) return;
    sExecutableDir[length] = 0;
    char *maybe_slash = strrchr(sExecutableDir, '/');
    if (!maybe_slash) return;
    maybe_slash[1] = 0;
    sPreference = readPreference();
    snprintf(sDatabase, sizeof(sDatabase), "%s", portRootPath("Data/CrashReports"));
    mkdir(portRootPath("Data"), 0700);
    struct stat info;
    if (lstat(portRootPath("Data"), &info) || !S_ISDIR(info.st_mode)) return;
    if (!lstat(sDatabase, &info) && !S_ISDIR(info.st_mode)) return;
    const char *maybe_override = getenv("GE_CRASH_REPORTING");
    sSuppressed = (maybe_override && !strcmp(maybe_override, "0")) ||
        getenv("GE_HAND_GALLERY") || getenv("GE_NOINISAVE") || getenv("GE_NOINPUT");
    if (!sPreference) clearDatabase();
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
    const char *keys[] = { "crash_reporting" }, *values[] = { value ? "1" : "0" };
    int saved = portIniWriteStrs("[diagnostics]", keys, values, 1);
    if (!saved && value) return 0;
    if (!saved) portConLogf("ge: cannot persist opt-out; reporting is OFF for this run\n");
    sPreference = value != 0;
    if (!sPreference) {
#ifdef MIDAS_SENTRY_ENABLED
        if (sActive) p_sentry_user_consent_revoke();
#endif
        portCrashShutdown();
        clearDatabase();
    } else {
#ifdef MIDAS_SENTRY_ENABLED
        startReporting();
#endif
    }
    return 1;
}

__attribute__((noinline)) static void testCrashFault(volatile int *address)
{
    *address = 42; /* Intentional ROM-free crash: verify this line in Sentry. */
}

int portCrashTestMode(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--test-crash-reporting") || !strcmp(argv[i], "--test-crash-reporting-optout")) {
            if (!strcmp(argv[i], "--test-crash-reporting-optout")) portCrashOptionSet("crash_reporting", 0);
            portConLogf("ge: intentional crash reporting test active=%d release=iron-sights@%s\n", sActive, PORT_BUILD_SHA);
            testCrashFault((volatile int *)(uintptr_t)1);
        }
        if (!strcmp(argv[i], "--check-crash-reporting")) return 1;
    }
    return 0;
}
