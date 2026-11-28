/* The in-game updater's screens and worker (port_update_ui.h, FORK-095
 * UPD-003): the Updates option, the run on its own thread, the legal-screen
 * indicator, the file-select notice and the Updates page. The run is
 * port_update.c's; the paths, the log and the restart are
 * port_update_startup.c's (port_update_app.h). */
/* SDL first and no <string.h>: the game's string.h declares strlen
 * otherwise than MinGW's intrin.h, which SDL.h brings in (SDL_strcmp and
 * SDL_memset stand in). */
#include <SDL.h>
#include <stdio.h>

#include <ultra64.h>
#include <bondconstants.h>
#include "boss.h"

#include "port_download_page.h"
#include "port_update.h"
#include "port_update_app.h"
#include "port_update_files.h"
#include "port_update_ui.h"

extern s32 portIniWriteStrs(const char *section, const char *const *keys, const char *const *vals, s32 n);
extern s32 portInputP1PromptDevice(void);

/* A launch run that the legal screen has not held by then (a harness that
 * boots straight into a stage) no longer waits for it. */
#define LAUNCH_HOLD_FRAMES 600

/* ---- the option ----------------------------------------------------------- */

static int sMode = PORT_UPDATE_DEFAULT_MODE;
static const char *const kModeIni[] = { "off", "notify", "automatic" };

int portUpdateOptionGet(const char *key, int *out)
{
    if (SDL_strcmp(key, "mode") != 0) {
        return 0;
    }
    *out = sMode;
    return 1;
}

int portUpdateOptionSet(const char *key, int value)
{
    const char *keys[1] = { "mode" }, *values[1];

    if (SDL_strcmp(key, "mode") != 0) {
        return 0;
    }
    if (value < PORT_UPDATE_OFF || value > PORT_UPDATE_AUTOMATIC) {
        value = PORT_UPDATE_DEFAULT_MODE;
    }
    values[0] = kModeIni[value];
    portIniWriteStrs("[updates]", keys, values, 1);
    sMode = value;
    return 1;
}

void portUpdateOptionsLoad(void)
{
    sMode = portUpdateAppIniMode();
}

/* ---- the run, on its own thread ------------------------------------------- */

enum { RUN_CHECK, RUN_INSTALL };

static struct {
    SDL_atomic_t busy;    /* a worker is running or waits to be collected */
    SDL_atomic_t done;    /* the worker has finished: result and msg are set */
    SDL_atomic_t phase;   /* enum PortUpdatePhase, -1 before the first */
    SDL_atomic_t build;
    SDL_atomic_t kbytes;  /* package KB received */
    SDL_atomic_t cancel;
    struct PortUpdateEnv env;
    int kind, force, launch, result;
    char msg[256];
} sRun;

static struct PortUpdateState sState;
static int sStarted, sEligible = -1, sWritable = -1;
static long sPendingBuild;     /* installed while in a mission: used at the next start */
static long sFrames, sHeldAt;  /* frames seen, and the last the legal screen held */

static void progress(void *user, int phase, long build, long long bytes)
{
    (void) user;
    SDL_AtomicSet(&sRun.phase, phase);
    SDL_AtomicSet(&sRun.build, (int) build);
    SDL_AtomicSet(&sRun.kbytes, (int) (bytes / 1024));
}

static int cancelled(void *user)
{
    (void) user;
    return SDL_AtomicGet(&sRun.cancel) != 0;
}

static int worker(void *data)
{
    (void) data;
    sRun.result = portUpdateRun(&sRun.env, sRun.force, sRun.msg, sizeof(sRun.msg));
    SDL_AtomicSet(&sRun.done, 1);  /* a full barrier: result and msg are visible first */
    return 0;
}

static void readState(void)
{
    struct PortUpdatePaths paths;
    const struct PortUpdateEnv *env = portUpdateAppEnv();

    SDL_memset(&sState, 0, sizeof(sState));
    if (env != NULL && portUpdatePaths(env, &paths)) {
        portUpdateStateRead(paths.state, &sState);
    }
}

static int eligible(void)
{
    if (sEligible < 0) {
        char why[160];
        const struct PortUpdateEnv *env = portUpdateAppEnv();

        sEligible = env != NULL && portUpdateEligible(env, why, sizeof(why));
        if (env != NULL && !sEligible) {
            portUpdateAppNote("%s; this copy cannot update itself", why);
        }
    }
    return sEligible;
}

/* The program folder takes new files (an AppImage is replaced beside
 * $APPIMAGE, which the swap checks itself). */
static int canInstall(void)
{
    const struct PortUpdateEnv *env = portUpdateAppEnv();

    if (!eligible()) {
        return 0;
    }
    if (sWritable < 0) {
        sWritable = env->appimage != NULL || portUpdateFolderWritable(env->exe_dir);
    }
    return sWritable;
}

static int busy(void)
{
    return SDL_AtomicGet(&sRun.busy) != 0;
}

static int inFrontEnd(void)
{
    return bossGetStageNum() == LEVELID_TITLE;
}

/* A newer build that a check saw and that was not rolled back. */
static long available(void)
{
    const struct PortUpdateEnv *env = portUpdateAppEnv();

    if (env == NULL || sPendingBuild != 0 || sState.newest_tag[0] == '\0' || sState.newest <= env->build ||
        sState.newest == sState.bad_build) {
        return 0;
    }
    return sState.newest;
}

/* hold: the launch's install, which the legal screen waits for. */
static void start(int kind, int force, int hold, int atLaunch)
{
    const struct PortUpdateEnv *env = portUpdateAppEnv();
    SDL_Thread *thread;

    if (env == NULL || busy() || !eligible()) {
        return;
    }
    sRun.env = *env;
    sRun.env.check_only = kind == RUN_CHECK;
    sRun.env.progress = progress;
    sRun.env.cancelled = cancelled;
    sRun.env.user = NULL;
    sRun.kind = kind;
    sRun.force = force;
    sRun.launch = hold;
    sRun.msg[0] = '\0';
    SDL_AtomicSet(&sRun.phase, -1);
    SDL_AtomicSet(&sRun.kbytes, 0);
    SDL_AtomicSet(&sRun.cancel, 0);
    SDL_AtomicSet(&sRun.done, 0);
    SDL_AtomicSet(&sRun.busy, 1);
    thread = SDL_CreateThread(worker, "ge-update", NULL);
    if (thread == NULL) {
        SDL_AtomicSet(&sRun.busy, 0);
        portUpdateAppNote("cannot start the update thread: %s", SDL_GetError());
        return;
    }
    SDL_DetachThread(thread);
    portUpdateAppNote("%s %s", kind == RUN_CHECK ? "checking for a newer build" : "looking for a build to install",
                      atLaunch ? "at launch" : "from the menu");
}

static const char *const kResultNames[PORT_UPDATE_RESULTS] = {
    "not eligible", "not due", "up to date", "failed", "installed", "available", "cancelled",
};

static void collect(void)
{
    const int result = sRun.result;

    portUpdateAppNote("%s (%s)", result >= 0 && result < PORT_UPDATE_RESULTS ? kResultNames[result] : "?",
                      sRun.msg);
    readState();
    sRun.launch = 0;
    SDL_AtomicSet(&sRun.done, 0);
    SDL_AtomicSet(&sRun.busy, 0);
    if (result != PORT_UPDATE_INSTALLED) {
        return;
    }
    if (inFrontEnd()) {
        portUpdateAppRestart();
    }
    /* In a mission the new files wait for the next start; the swap's
     * journal counts that start as the new build's. */
    sPendingBuild = sState.newest;
    portUpdateAppNote("build %ld is installed; it starts next time", sPendingBuild);
}

void portUpdateFrame(void)
{
    sFrames++;
    if (!sStarted) {
        int force, launch;
        const struct PortUpdateEnv *env;

        sStarted = 1;
        /* the files the screens read, at boot rather than when a menu
         * first shows them: the state, package.txt and the folder probe */
        readState();
        canInstall();
        launch = portUpdateAppLaunch(&force);
        env = portUpdateAppEnv();
        if (launch != PORT_UPDATE_LAUNCH_NONE && env != NULL &&
            (force || portUpdateCheckDue(env->now, sState.last_check))) {
            start(launch == PORT_UPDATE_LAUNCH_INSTALL ? RUN_INSTALL : RUN_CHECK, force,
                  launch == PORT_UPDATE_LAUNCH_INSTALL, 1);
        }
    }
    if (sRun.launch && sHeldAt == 0 && sFrames > LAUNCH_HOLD_FRAMES) {
        sRun.launch = 0;  /* never held: it finishes in the background */
    }
    if (busy() && SDL_AtomicGet(&sRun.done)) {
        collect();
    }
}

/* ---- the legal screen and the file select -------------------------------- */

int portUpdateLegalHold(int pressed)
{
    if (!sRun.launch || !busy()) {
        return 0;
    }
    sHeldAt = sFrames;
    if (pressed && SDL_AtomicGet(&sRun.phase) != PORT_UPDATE_PHASE_INSTALLING && !SDL_AtomicGet(&sRun.done)) {
        SDL_AtomicSet(&sRun.cancel, 1);
        sRun.launch = 0;
        portUpdateAppNote("the player skipped the launch's update");
    }
    return 1;
}

static void megabytes(char *out, size_t size, long kb)
{
    snprintf(out, size, "%ld.%ld MB", kb / 1024, (kb % 1024) * 10 / 1024);
}

const char *portUpdateIndicatorLine(int i)
{
    static char line[96];
    const int phase = SDL_AtomicGet(&sRun.phase);
    const long build = SDL_AtomicGet(&sRun.build);

    if (!sRun.launch || !busy() || phase < 0) {
        return NULL;
    }
    if (i == 0) {
        if (phase == PORT_UPDATE_PHASE_CHECKING) {
            return "Checking for updates...\n";
        }
        if (phase == PORT_UPDATE_PHASE_DOWNLOADING) {
            char mb[32];
            megabytes(mb, sizeof(mb), SDL_AtomicGet(&sRun.kbytes));
            snprintf(line, sizeof(line), "Downloading build %ld - %s\n", build, mb);
            return line;
        }
        snprintf(line, sizeof(line), "Installing build %ld - restarting\n", build);
        return line;
    }
    if (i == 1 && phase != PORT_UPDATE_PHASE_INSTALLING) {
        return portInputP1PromptDevice() > 0 ? "Press any button to skip\n" : "Press Esc or click to skip\n";
    }
    return NULL;
}

const char *portUpdateNotifyLine(void)
{
    static char line[64];
    const long build = available();

    if (sMode == PORT_UPDATE_OFF || build == 0 || busy()) {
        return NULL;
    }
    snprintf(line, sizeof(line), "Build %ld is out - System > Updates\n", build);
    return line;
}

/* ---- the Updates page ------------------------------------------------------ */

void portUpdatesOpenReleasePage(void)
{
    const struct PortUpdateEnv *env = portUpdateAppEnv();
    char url[320];

    if (env == NULL) {
        return;
    }
    if (sState.newest_tag[0] != '\0' && sState.newest > env->build) {
        snprintf(url, sizeof(url), "%s/tag/%s", env->downloads_url, sState.newest_tag);
    } else {
        snprintf(url, sizeof(url), "%s/latest", env->downloads_url);
    }
    portUpdateAppNote("opening %s", url);
    if (SDL_OpenURL(url) != 0) {
        portUpdateAppNote("cannot open the release page: %s", SDL_GetError());
    }
}

static const char *statusLine(void)
{
    static char line[48];
    const struct PortUpdateEnv *env = portUpdateAppEnv();

    if (env == NULL || !eligible()) {
        return "This copy cannot update.\n";
    }
    if (busy()) {
        const int phase = SDL_AtomicGet(&sRun.phase);
        const long build = SDL_AtomicGet(&sRun.build);

        if (phase == PORT_UPDATE_PHASE_DOWNLOADING) {
            char mb[32];
            megabytes(mb, sizeof(mb), SDL_AtomicGet(&sRun.kbytes));
            snprintf(line, sizeof(line), "Downloading: %s\n", mb);
            return line;
        }
        if (phase == PORT_UPDATE_PHASE_INSTALLING) {
            snprintf(line, sizeof(line), "Installing build %ld...\n", build);
            return line;
        }
        return "Checking for updates...\n";
    }
    if (sPendingBuild != 0) {
        snprintf(line, sizeof(line), "Build %ld starts next time.\n", sPendingBuild);
        return line;
    }
    if (available() != 0) {
        snprintf(line, sizeof(line), canInstall() ? "Build %ld is available.\n" : "Build %ld is out (read-only).\n",
                 available());
        return line;
    }
    if (sState.last_check == 0) {
        return "Not checked yet.\n";
    }
    if (sState.outcome == PORT_UPDATE_FAILED) {
        return "Last check failed.\n";
    }
    snprintf(line, sizeof(line), "Up to date (build %ld).\n", env->build);
    return line;
}

static const char *infoLine(int i)
{
    /* Short enough for the watch face (about 29 characters a line). */
    static const char *const lines[] = {
        "Checks GitHub at most twice\n",
        "a day for a newer build.\n",
        "Automatic installs it at\n",
        "launch; Notify only tells\n",
        "you. Saves and settings stay.\n",
        "Install from the main menu.\n",
    };
    const int fixed = (int) (sizeof(lines) / sizeof(lines[0]));

    if (i >= 0 && i < fixed) {
        return lines[i];
    }
    return i == fixed ? statusLine() : NULL;
}

/* What the first button does: */
enum { ACT_CHECK, ACT_CANCEL, ACT_WAIT, ACT_INSTALL, ACT_PAGE };

static int firstAction(int watch)
{
    if (!eligible()) {
        return ACT_PAGE;
    }
    if (busy()) {
        return SDL_AtomicGet(&sRun.phase) == PORT_UPDATE_PHASE_INSTALLING ? ACT_WAIT : ACT_CANCEL;
    }
    if (available() != 0 && !watch) {
        return canInstall() && inFrontEnd() ? ACT_INSTALL : ACT_PAGE;
    }
    return ACT_CHECK;
}

static const char *actionText(int action)
{
    static char line[40];

    switch (action) {
    case ACT_CANCEL:  return "Cancel\n";
    case ACT_WAIT:    return "Installing...\n";
    case ACT_PAGE:    return "Open Release Page\n";
    case ACT_INSTALL:
        snprintf(line, sizeof(line), "Install Build %ld\n", available());
        return line;
    default:          return "Check for Updates\n";
    }
}

static void doAction(int action)
{
    switch (action) {
    case ACT_CANCEL:  SDL_AtomicSet(&sRun.cancel, 1); break;
    case ACT_WAIT:    break;
    case ACT_PAGE:    portUpdatesOpenReleasePage(); break;
    case ACT_INSTALL: start(RUN_INSTALL, 1, 0, 0); break;
    default:          start(RUN_CHECK, 1, 0, 0); break;
    }
}

static const char *actionLabel(int action)
{
    return action == 0 ? actionText(firstAction(0)) : "What's New\n";
}

static void primaryAction(void)
{
    doAction(firstAction(0));
}

static int hasPage(void)
{
    return portUpdateAppEnv() != NULL;
}

const char *portUpdatesWatchActionLabel(void)
{
    return actionText(firstAction(1));
}

void portUpdatesWatchAction(void)
{
    doAction(firstAction(1));
}

const struct PortDownloadPage portUpdatesPage = {
    infoLine,
    actionLabel,
    primaryAction,
    hasPage,
    portUpdatesOpenReleasePage,
    "Open the Release Page?\n",
    "@updates_action",
    "@updates_news",
};
