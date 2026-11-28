/* The in-game updater's process glue (port_update.h): this executable's
 * paths and build, the hooks main.c and port_startup.c call, and the restart
 * into the build the files now hold. Compiled only where the updater is
 * (GE_UPDATER, CMakeLists.txt): desktop builds with video, never a
 * comparison build. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "port_buildinfo.h"
#include "port_console.h"
#include "port_modal.h"
#include "port_update.h"
#include "port_update_files.h"

extern const char *portRootPath(const char *name);

static char sRoot[PORT_UPDATE_PATH], sExeDir[PORT_UPDATE_PATH], sExePath[PORT_UPDATE_PATH];
static char sAppImage[PORT_UPDATE_PATH];
static char sDownloads[256];
static struct PortUpdateEnv sEnv;
static int sReady;

#define LOG_LIMIT (64 * 1024)

/* A line in the run's log and in Data/Updates/log.txt, which keeps the
 * updater's lines across the restart (the restarted game starts its own
 * ge_direct.log). At LOG_LIMIT the file moves to log.old.txt. */
static void note(const char *fmt, ...)
{
    char line[512], path[PORT_UPDATE_PATH], old[PORT_UPDATE_PATH];
    struct stat st;
    va_list args;
    FILE *f;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    portConLogf("ge: update: %s\n", line);
    if (!portUpdateJoin(path, sizeof(path), portRootPath(""), "Data/Updates/log.txt") ||
        !portUpdateJoin(old, sizeof(old), portRootPath(""), "Data/Updates/log.old.txt") ||
        !portUpdateMakeParents(path)) {
        return;
    }
    if (stat(path, &st) == 0 && st.st_size > LOG_LIMIT) {
        portUpdateReplace(path, old);
    }
    f = fopen(path, "ab");
    if (f != NULL) {
        fprintf(f, "%lld build %ld: %s\n", (long long) time(NULL), (long) PORT_BUILD_NUMBER, line);
        fclose(f);
    }
}

static int findExecutable(void)
{
    char *slash;
#ifdef _WIN32
    const DWORD n = GetModuleFileNameA(NULL, sExePath, sizeof(sExePath));
    if (n == 0 || n >= sizeof(sExePath)) {
        return 0;
    }
#else
    const ssize_t n = readlink("/proc/self/exe", sExePath, sizeof(sExePath) - 1);
    if (n <= 0) {
        return 0;
    }
    sExePath[n] = '\0';
#endif
    snprintf(sExeDir, sizeof(sExeDir), "%s", sExePath);
    slash = strrchr(sExeDir, '/');
#ifdef _WIN32
    if (strrchr(sExeDir, '\\') > slash) {
        slash = strrchr(sExeDir, '\\');
    }
#endif
    if (slash == NULL) {
        return 0;
    }
    slash[1] = '\0';
    return 1;
}

/* A test build (GE_UPDATE_TEST) can be pointed at a local fixture and
 * pretend to be another CI build; nothing else reads these variables. */
static void testOverrides(void)
{
#ifdef GE_UPDATE_TEST
    const char *base = getenv("GE_UPDATE_TEST_DOWNLOADS"), *build = getenv("GE_UPDATE_TEST_BUILD");
    if (base != NULL && portUpdateStartsWith(base, "http://127.0.0.1:") && strlen(base) < sizeof(sDownloads)) {
        snprintf(sDownloads, sizeof(sDownloads), "%s", base);
    }
    if (build != NULL) {
        sEnv.build = strtol(build, NULL, 10);
    }
    if (getenv("GE_UPDATE_TEST_CI") != NULL) {
        sEnv.ci = 1;
    }
#endif
}

static const struct PortUpdateEnv *environment(void)
{
    const char *appimage = getenv("APPIMAGE");

    if (sReady) {
        sEnv.now = (long long) time(NULL);
        return &sEnv;
    }
    if (!findExecutable() || strlen(portRootPath("")) >= sizeof(sRoot)) {
        return NULL;
    }
    snprintf(sRoot, sizeof(sRoot), "%s", portRootPath(""));
    snprintf(sDownloads, sizeof(sDownloads), "%s", PORT_BUILD_DOWNLOADS_URL);
    memset(&sEnv, 0, sizeof(sEnv));
    sEnv.root = sRoot;
    sEnv.exe_dir = sExeDir;
    sEnv.exe_path = sExePath;
    if (appimage != NULL && appimage[0] == '/' && strlen(appimage) < sizeof(sAppImage)) {
        snprintf(sAppImage, sizeof(sAppImage), "%s", appimage);
        sEnv.appimage = sAppImage;
    }
    sEnv.build = PORT_BUILD_NUMBER;
    sEnv.ci = PORT_BUILD_CI && !PORT_BUILD_DIRTY;
    sEnv.http = portUpdatePlatformHttp();
    testOverrides();
    sEnv.downloads_url = sDownloads;
    sEnv.now = (long long) time(NULL);
    sReady = 1;
    return &sEnv;
}

/* Starts the build the files now hold with this run's arguments, then
 * ends this one. Returns only when that is impossible. */
static void restart(int argc, char **argv)
{
#ifdef _WIN32
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    const wchar_t *line = GetCommandLineW();
    wchar_t *copy = (wchar_t *) malloc((wcslen(line) + 1) * sizeof(wchar_t));

    (void) argc;
    (void) argv;
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    portUpdateUnlock();
    if (copy != NULL) {
        wcscpy(copy, line);
        if (CreateProcessW(NULL, copy, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            note("restarted into the installed build");
            exit(0);
        }
    }
    note("cannot restart (error %lu); open Iron Sights again", (unsigned long) GetLastError());
    exit(1);
#else
    char *path = sEnv.appimage != NULL ? sAppImage : sExePath;
    char **args = calloc((size_t) argc + 1, sizeof(char *));
    int i;

    portUpdateUnlock();
    fflush(NULL);
    if (args != NULL) {
        args[0] = path;
        for (i = 1; i < argc; i++) {
            args[i] = argv[i];
        }
        execv(path, args);
    }
    note("cannot restart %s; open Iron Sights again", path);
    exit(1);
#endif
}

static int wantsUpdateNow(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--update-now") == 0) {
            return 1;
        }
    }
    return 0;
}

static int iniMode(void)
{
    size_t size;
    char *ini = portUpdateReadFile(portRootPath("midas.ini"), 1u << 20, &size);
    const int mode = portUpdateModeFromIni(ini);

    free(ini);
    return mode;
}

static const char *const kResultNames[] = {
    "not eligible", "not due", "up to date", "failed", "installed",
};

void portUpdateStartup(int argc, char **argv)
{
    const struct PortUpdateEnv *env = environment();
    char msg[256];
    int mode, force, result;

    if (env == NULL) {
        note("cannot find this executable; updates off");
        return;
    }
    if (portUpdateRecover(env, msg, sizeof(msg))) {
        note("%s; restarting", msg);
        restart(argc, argv);
    } else if (msg[0] != '\0') {
        note("%s", msg);
    }
    mode = iniMode();
    force = wantsUpdateNow(argc, argv);
    if (mode == PORT_UPDATE_NOTIFY && !force) {
        note("notify mode needs the menus (UPD-003); not checking");
        return;
    }
    if (mode != PORT_UPDATE_AUTOMATIC && !force) {
        return;
    }
    portModalBegin();  /* the download may outlast the stall watchdog's grace */
    result = portUpdateRun(env, force, msg, sizeof(msg));
    portModalEnd();
    note("%s (%s)", kResultNames[result], msg);
    if (result == PORT_UPDATE_INSTALLED) {
        restart(argc, argv);
    }
}

void portUpdateLegalScreen(void)
{
    static int done;
    const struct PortUpdateEnv *env;
    long build;

    if (done) {
        return;
    }
    done = 1;
    env = environment();
    if (env != NULL && (build = portUpdateConfirm(env)) != 0) {
        note("build %ld reached the legal screen; the previous build's files are removed", build);
    }
}
