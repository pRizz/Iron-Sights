/* The in-game updater's process glue (port_update.h, port_update_app.h):
 * this executable's paths and build, the hooks main.c and port_startup.c
 * call, what the launch should run, and the restart into the build the files
 * now hold. The run itself is port_update_ui.c's. Compiled only where the
 * updater is (GE_UPDATER, CMakeLists.txt): desktop builds with video, never
 * a comparison build. */
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
#include "port_update.h"
#include "port_update_app.h"
#include "port_update_files.h"

extern const char *portRootPath(const char *name);

static char sRoot[PORT_UPDATE_PATH], sExeDir[PORT_UPDATE_PATH], sExePath[PORT_UPDATE_PATH];
static char sAppImage[PORT_UPDATE_PATH];
static char sDownloads[256];
static struct PortUpdateEnv sEnv;
static int sReady;
static int sArgc;
static char **sArgv;
static int sLaunch, sLaunchForce;

#define LOG_LIMIT (64 * 1024)

/* A line in the run's log and in Data/Updates/log.txt, which keeps the
 * updater's lines across the restart (the restarted game starts its own
 * ge_direct.log). At LOG_LIMIT the file moves to log.old.txt. */
void portUpdateAppNote(const char *fmt, ...)
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

const struct PortUpdateEnv *portUpdateAppEnv(void)
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

void portUpdateAppRestart(void)
{
#ifdef _WIN32
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    const wchar_t *line = GetCommandLineW();
    /* lstrlenW and lstrcpyW: windows.h declares them, while the game's
     * string.h shadows the C library's wide-string functions. */
    wchar_t *copy = malloc(((size_t) lstrlenW(line) + 1) * sizeof(wchar_t));

    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    portUpdateUnlock();
    if (copy != NULL) {
        lstrcpyW(copy, line);
        if (CreateProcessW(NULL, copy, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            portUpdateAppNote("restarted into the installed build");
            exit(0);
        }
    }
    portUpdateAppNote("cannot restart (error %lu); open Iron Sights again", (unsigned long) GetLastError());
    exit(1);
#else
    char *path = sEnv.appimage != NULL ? sAppImage : sExePath;
    char **args = calloc((size_t) sArgc + 1, sizeof(char *));
    int i;

    portUpdateUnlock();
    portUpdateAppNote("restarting into the installed build");
    fflush(NULL);
    if (args != NULL) {
        args[0] = path;
        for (i = 1; i < sArgc; i++) {
            args[i] = sArgv[i];
        }
        execv(path, args);
    }
    portUpdateAppNote("cannot restart %s; open Iron Sights again", path);
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

int portUpdateAppIniMode(void)
{
    size_t size;
    char *ini = portUpdateReadFile(portRootPath("midas.ini"), 1u << 20, &size);
    const int mode = portUpdateModeFromIni(ini);

    free(ini);
    return mode;
}

/* An automated run (GE_FRAMES limits it, as the audits and smoke runs do)
 * never checks, unless a test build is pointed at a local fixture; neither
 * does any run with GE_NOUPDATE set. */
static int automated(void)
{
    if (getenv("GE_NOUPDATE") != NULL) {
        return 1;
    }
#ifdef GE_UPDATE_TEST
    if (getenv("GE_UPDATE_TEST_DOWNLOADS") != NULL) {
        return 0;
    }
#endif
    return getenv("GE_FRAMES") != NULL;
}

int portUpdateAppLaunch(int *force)
{
    *force = sLaunchForce;
    return sLaunch;
}

void portUpdateStartup(int argc, char **argv)
{
    const struct PortUpdateEnv *env = portUpdateAppEnv();
    char msg[256];
    int mode;

    sArgc = argc;
    sArgv = argv;
    if (env == NULL) {
        portUpdateAppNote("cannot find this executable; updates off");
        return;
    }
    if (portUpdateRecover(env, msg, sizeof(msg))) {
        portUpdateAppNote("%s; restarting", msg);
        portUpdateAppRestart();
    } else if (msg[0] != '\0') {
        portUpdateAppNote("%s", msg);
    }
    mode = portUpdateAppIniMode();
    sLaunchForce = wantsUpdateNow(argc, argv);
    if (automated() && !sLaunchForce) {
        return;
    }
    sLaunch = sLaunchForce || mode == PORT_UPDATE_AUTOMATIC ? PORT_UPDATE_LAUNCH_INSTALL
            : mode == PORT_UPDATE_NOTIFY ? PORT_UPDATE_LAUNCH_CHECK : PORT_UPDATE_LAUNCH_NONE;
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
    env = portUpdateAppEnv();
    if (env != NULL && (build = portUpdateConfirm(env)) != 0) {
        portUpdateAppNote("build %ld reached the legal screen; the previous build's files are removed", build);
    }
}
