#ifndef PORT_UPDATE_H
#define PORT_UPDATE_H
/* The in-game updater (FORK-095, UPD-002 of scripts/port/self_update_plan.md).
 *
 * A CI build installed from a release package reads the package's identity
 * (package.txt, UPD-001) and compares its build number with the latest
 * release's build-info file for the same flavor. A newer build is downloaded
 * into Data/Updates/<tag>/, checked against the release's SHA256SUMS file and
 * the package's own SHA256SUMS.txt, and swapped in file by file: each current
 * program file is renamed to NAME.old first, then the new one moved into place,
 * with a journal that undoes the swap after a failure or a crash. The game then
 * restarts with its own arguments. The new build counts its starts until it
 * reaches the legal screen; a third start without that puts the old files
 * back.
 *
 * port_update_core.c parses (no files, no network), port_update_sha256.c
 * hashes, port_update_archive.c reads zips and tarballs, port_update.c does
 * the files, and port_update_http.cpp is the HTTPS transport bridge. */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum PortUpdateMode { PORT_UPDATE_OFF, PORT_UPDATE_NOTIFY, PORT_UPDATE_AUTOMATIC };

/* UPD-002 ships without screens, so a build only updates when its midas.ini
 * asks; UPD-003 makes Automatic the default together with the indicator. */
#define PORT_UPDATE_DEFAULT_MODE PORT_UPDATE_OFF
/* Owner's decision 1: a check at launch at most twice a day. */
#define PORT_UPDATE_CHECK_INTERVAL (12LL * 60 * 60)
/* Owner's decision 5: the check's limit and the download's stall limit. */
#define PORT_UPDATE_CHECK_SECONDS 5
#define PORT_UPDATE_STALL_SECONDS 15
/* A new build that starts this often without reaching the legal screen is
 * rolled back. */
#define PORT_UPDATE_MAX_ATTEMPTS 3

/* ---- core: port_update_core.c ------------------------------------------- */

/* [updates] mode= of midas.ini's text (NULL for no file); the default when
 * absent or unreadable. */
int portUpdateModeFromIni(const char *ini);
/* 1 when a launch check is due: never checked, the interval has passed, or
 * the clock went back. */
int portUpdateCheckDue(long long now, long long last);

enum PortUpdateKind { PORT_UPDATE_KIND_ZIP, PORT_UPDATE_KIND_TARGZ, PORT_UPDATE_KIND_APPIMAGE };

/* A package flavor and the release assets it updates from (the same table as
 * scripts/port/package_identity.py's FLAVORS; test_update checks both). */
struct PortUpdateFlavor {
    const char *name;
    const char *asset;
    const char *sums;
    const char *build_info;
    int kind;
    /* The folder the package's files sit in inside the archive ("" for none). */
    const char *folder;
};

const struct PortUpdateFlavor *portUpdateFlavorNamed(const char *name);
/* The flavor package.txt's text names (schema 1), or NULL. */
const struct PortUpdateFlavor *portUpdateFlavorFromPackage(const char *text);
/* The table, for the harness. */
const struct PortUpdateFlavor *portUpdateFlavorAt(size_t index);

struct PortUpdateRelease {
    long build;
    int ci;
    int dirty;
    char tag[64];
};

/* Reads a release's build-info.json. Its release_url must be
 * DOWNLOADS_URL/tag/TAG with a valid TAG. 1 on success. */
int portUpdateParseBuildInfo(const char *json, const char *downloads_url, struct PortUpdateRelease *out);
/* build-N.A-HHHHHHHHHHHH, the release workflow's tags. */
int portUpdateTagValid(const char *tag);
/* A relative path with no empty, "." or ".." part, no backslash, colon or
 * control character, and no leading slash. */
int portUpdatePathSafe(const char *path);
/* A path a package may install: safe, and not the player's (midas.ini, or
 * anything under Data/ except the package's README.txt and shaders_base.txt). */
int portUpdatePackagePath(const char *path);

/* Called for each "HEX  NAME" line of a SHA256SUMS text; 0 stops. */
typedef int (*PortUpdateSumsFn)(void *user, const char *name, const char *hex);
/* The number of lines, or -1 for a malformed line or a stop. */
int portUpdateSumsEach(const char *sums, PortUpdateSumsFn fn, void *user);
/* NAME's hash into hex; 1 when listed. */
int portUpdateSumsLookup(const char *sums, const char *name, char hex[65]);

/* 1 when text begins with prefix. The game's include/string.h declares no
 * strncmp, strcpy or strcat, so the updater uses this and snprintf. */
int portUpdateStartsWith(const char *text, const char *prefix);

/* key=value lines (state, journal headers): VALUE of KEY into out; 1 when
 * present. */
int portUpdateKeyValue(const char *text, const char *key, char *out, size_t size);

/* ---- SHA-256: port_update_sha256.c --------------------------------------- */

struct PortSha256 {
    unsigned int h[8];
    unsigned long long length;
    unsigned char block[64];
    size_t used;
};

void portSha256Init(struct PortSha256 *s);
void portSha256Add(struct PortSha256 *s, const void *data, size_t size);
/* Lowercase hex and a terminator. */
void portSha256Hex(struct PortSha256 *s, char hex[65]);
void portSha256Of(const void *data, size_t size, char hex[65]);

/* ---- archives: port_update_archive.c ------------------------------------- */

/* A regular file of an archive; executable from its mode bits. 0 stops. */
typedef int (*PortUpdateEntryFn)(void *user, const char *name, const unsigned char *data, size_t size,
                                 int executable);
/* Each file of a zip (stored or deflated), directories skipped; anything
 * else (encryption, zip64, a link) fails. 1 on success, else 0 with err. */
int portUpdateZipEach(const unsigned char *zip, size_t size, PortUpdateEntryFn fn, void *user, char *err,
                      size_t errsize);
/* A gzip member's contents into a malloc'd buffer the caller frees. */
int portUpdateGunzip(const unsigned char *gz, size_t size, unsigned char **out, size_t *outsize, char *err,
                     size_t errsize);
/* Each regular file of a ustar archive; links and devices fail. */
int portUpdateTarEach(const unsigned char *tar, size_t size, PortUpdateEntryFn fn, void *user, char *err,
                      size_t errsize);

/* ---- transport --------------------------------------------------------- */

/* Receives a body's bytes as they arrive; 0 stops the transfer. */
typedef int (*PortUpdateSink)(void *user, const unsigned char *data, size_t size);
/* Fetches url into sink. stream 0: a small file, PORT_UPDATE_CHECK_SECONDS
 * in all; stream 1: a package, given up after PORT_UPDATE_STALL_SECONDS
 * without data. 1 on success, else 0 with a short reason. */
typedef int (*PortUpdateHttp)(const char *url, int stream, PortUpdateSink sink, void *user, char *reason,
                              size_t reasonsize);
/* The platform's, or NULL (port_update_http.cpp; none on the web). */
PortUpdateHttp portUpdatePlatformHttp(void);

/* ---- files: port_update.c ------------------------------------------------ */

/* Where an install lives and what it is. Paths end in a separator. */
struct PortUpdateEnv {
    const char *root;           /* the data root: midas.ini and Data/ */
    const char *exe_dir;        /* package.txt and the program files */
    const char *exe_path;       /* the running executable */
    const char *appimage;       /* $APPIMAGE for an AppImage, else NULL */
    long build;                 /* this build's number */
    int ci;                     /* a clean CI build */
    const char *downloads_url;  /* https://github.com/OWNER/REPO/releases */
    long long now;              /* seconds since 1970 */
    PortUpdateHttp http;
};

enum PortUpdateResult {
    PORT_UPDATE_NOT_ELIGIBLE,  /* local build, no package.txt, unknown flavor */
    PORT_UPDATE_NOT_DUE,
    PORT_UPDATE_UP_TO_DATE,
    PORT_UPDATE_FAILED,
    PORT_UPDATE_INSTALLED,     /* restart into the new build */
};

/* At every start, before settings or saves load: finishes or undoes an
 * interrupted swap, counts a new build's starts and rolls back a build that
 * never reached the legal screen. 1 when the game must restart (it is not
 * the build the files now hold). */
int portUpdateRecover(const struct PortUpdateEnv *env, char *msg, size_t msgsize);
/* Checks and, when a newer build exists, installs it. force ignores the
 * throttle. msg says what happened. */
int portUpdateRun(const struct PortUpdateEnv *env, int force, char *msg, size_t msgsize);
/* The new build reached the legal screen: drop the old files and the
 * journal. Returns the build it confirmed, 0 when there was none. */
long portUpdateConfirm(const struct PortUpdateEnv *env);

/* main.c: recovery, then the check when midas.ini asks for Automatic or the
 * command line holds --update-now. Restarts the game itself after an install
 * or a rollback, and returns otherwise. */
void portUpdateStartup(int argc, char **argv);
/* port_startup.c: the legal screen was drawn (once per run is enough). */
void portUpdateLegalScreen(void);

#ifdef __cplusplus
}
#endif
#endif
