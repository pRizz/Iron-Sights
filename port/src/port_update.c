/* The in-game updater's check, download and staging (port_update.h). The
 * swap and its journal are port_update_swap.c; the process glue (paths,
 * restart, the startup hook) is port_update_startup.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_update.h"
#include "port_update_files.h"

#define SMALL_LIMIT (64u * 1024u)
#define PACKAGE_LIMIT (512u * 1024u * 1024u)

static void say(char *msg, size_t size, const char *what, const char *detail)
{
    if (size > 0) {
        snprintf(msg, size, "%s%s%s", what, detail != NULL ? ": " : "", detail != NULL ? detail : "");
    }
}

/* ---- fetching ----------------------------------------------------------- */

struct Buffer {
    char data[SMALL_LIMIT + 1];
    size_t used;
};

static int intoBuffer(void *user, const unsigned char *data, size_t size)
{
    struct Buffer *buffer = (struct Buffer *) user;

    if (size > SMALL_LIMIT - buffer->used) {
        return 0;
    }
    memcpy(buffer->data + buffer->used, data, size);
    buffer->used += size;
    buffer->data[buffer->used] = '\0';
    return 1;
}

static int fetchSmall(const struct PortUpdateEnv *env, const char *url, struct Buffer *buffer, char *msg,
                      size_t msgsize)
{
    char reason[96] = "";

    buffer->used = 0;
    buffer->data[0] = '\0';
    if (!env->http(url, 0, intoBuffer, buffer, reason, sizeof(reason))) {
        say(msg, msgsize, reason[0] != '\0' ? reason : "download failed", url);
        return 0;
    }
    return 1;
}

struct FileSink {
    FILE *f;
    struct PortSha256 sha;
    size_t written;
};

static int intoFile(void *user, const unsigned char *data, size_t size)
{
    struct FileSink *sink = (struct FileSink *) user;

    if (size > PACKAGE_LIMIT - sink->written || fwrite(data, 1, size, sink->f) != size) {
        return 0;
    }
    portSha256Add(&sink->sha, data, size);
    sink->written += size;
    return 1;
}

/* Streams url to path and checks it against hex. */
static int fetchPackage(const struct PortUpdateEnv *env, const char *url, const char *path, const char *hex,
                        char *msg, size_t msgsize)
{
    struct FileSink sink;
    char reason[96] = "", actual[65];
    int ok;

    if (!portUpdateMakeParents(path) || (sink.f = fopen(path, "wb")) == NULL) {
        say(msg, msgsize, "cannot write", path);
        return 0;
    }
    portSha256Init(&sink.sha);
    sink.written = 0;
    ok = env->http(url, 1, intoFile, &sink, reason, sizeof(reason));
    ok = fclose(sink.f) == 0 && ok;
    if (!ok) {
        portUpdateRemove(path);
        say(msg, msgsize, reason[0] != '\0' ? reason : "download failed", url);
        return 0;
    }
    portSha256Hex(&sink.sha, actual);
    if (strcmp(actual, hex) != 0) {
        portUpdateRemove(path);
        say(msg, msgsize, "checksum mismatch", url);
        return 0;
    }
    return 1;
}

/* ---- staging ------------------------------------------------------------ */

struct Stage {
    const struct PortUpdateFlavor *flavor;
    const char *dir;      /* staged/, ends in a separator */
    char *sums;           /* the new package's SHA256SUMS.txt */
    char *package;        /* its package.txt */
    int listed;           /* files written that SHA256SUMS.txt lists */
    char error[160];
};

/* NAME without the flavor's folder; NULL when it lies outside it. */
static const char *inFolder(const struct PortUpdateFlavor *flavor, const char *name)
{
    const size_t n = strlen(flavor->folder);

    if (n == 0) {
        return name;
    }
    return portUpdateStartsWith(name, flavor->folder) && name[n] == '/' ? name + n + 1 : NULL;
}

static char *copyText(const unsigned char *data, size_t size)
{
    char *text = malloc(size + 1);

    if (text != NULL) {
        memcpy(text, data, size);
        text[size] = '\0';
    }
    return text;
}

static int collectLists(void *user, const char *name, const unsigned char *data, size_t size, int executable)
{
    struct Stage *stage = (struct Stage *) user;
    const char *own = inFolder(stage->flavor, name);

    (void) executable;
    if (own != NULL && strcmp(own, "SHA256SUMS.txt") == 0 && stage->sums == NULL) {
        stage->sums = copyText(data, size);
    } else if (own != NULL && strcmp(own, "package.txt") == 0 && stage->package == NULL) {
        stage->package = copyText(data, size);
    }
    return 1;
}

static int writeStaged(void *user, const char *name, const unsigned char *data, size_t size, int executable)
{
    struct Stage *stage = (struct Stage *) user;
    const char *own = inFolder(stage->flavor, name);
    char path[PORT_UPDATE_PATH], listed[65], actual[65];

    if (own == NULL || !portUpdatePackagePath(own)) {
        snprintf(stage->error, sizeof(stage->error), "unsafe path %s", name);
        return 0;
    }
    if (strcmp(own, "SHA256SUMS.txt") != 0) {
        if (!portUpdateSumsLookup(stage->sums, own, listed)) {
            snprintf(stage->error, sizeof(stage->error), "%s is not in SHA256SUMS.txt", own);
            return 0;
        }
        portSha256Of(data, size, actual);
        if (strcmp(actual, listed) != 0) {
            snprintf(stage->error, sizeof(stage->error), "%s does not match SHA256SUMS.txt", own);
            return 0;
        }
        stage->listed++;
    }
    if (!portUpdateJoin(path, sizeof(path), stage->dir, own) || !portUpdateMakeParents(path) ||
        !portUpdateWriteFile(path, data, size) || (executable && !portUpdateMarkExecutable(path))) {
        snprintf(stage->error, sizeof(stage->error), "cannot write %.140s", path);
        return 0;
    }
    return 1;
}

static int eachEntry(const struct PortUpdateFlavor *flavor, const unsigned char *data, size_t size,
                     PortUpdateEntryFn fn, void *user, char *err, size_t errsize)
{
    if (flavor->kind == PORT_UPDATE_KIND_ZIP) {
        return portUpdateZipEach(data, size, fn, user, err, errsize);
    }
    return portUpdateTarEach(data, size, fn, user, err, errsize);
}

/* The package at path, unpacked into stage->dir after both lists are read. */
static int unpack(struct Stage *stage, const char *path, char *msg, size_t msgsize)
{
    size_t size, tarSize;
    unsigned char *file = portUpdateReadFile(path, PACKAGE_LIMIT, &size), *tar = NULL;
    const unsigned char *entries = file;
    char err[160] = "";
    int ok = file != NULL;

    if (ok && stage->flavor->kind == PORT_UPDATE_KIND_TARGZ) {
        ok = portUpdateGunzip(file, size, &tar, &tarSize, err, sizeof(err));
        entries = tar;
        size = tarSize;
    }
    ok = ok && eachEntry(stage->flavor, entries, size, collectLists, stage, err, sizeof(err));
    if (ok && (stage->sums == NULL || portUpdateFlavorFromPackage(stage->package) != stage->flavor)) {
        snprintf(err, sizeof(err), "the package is not a %s with SHA256SUMS.txt", stage->flavor->name);
        ok = 0;
    }
    ok = ok && eachEntry(stage->flavor, entries, size, writeStaged, stage, err, sizeof(err));
    if (ok && stage->listed != portUpdateSumsEach(stage->sums, NULL, NULL)) {
        snprintf(err, sizeof(err), "the package lacks files its SHA256SUMS.txt lists");
        ok = 0;
    }
    free(tar);
    free(file);
    if (!ok) {
        say(msg, msgsize, "cannot unpack", stage->error[0] != '\0' ? stage->error : err);
    }
    return ok;
}

/* ---- the run ------------------------------------------------------------ */

struct Run {
    const struct PortUpdateEnv *env;
    const struct PortUpdateFlavor *flavor;
    struct PortUpdatePaths paths;
    struct PortUpdateState state;
    struct PortUpdateRelease release;
    char tagDir[PORT_UPDATE_PATH];
    char package[PORT_UPDATE_PATH];
    char assetHex[65];
};

static int finish(struct Run *run, int result, char *msg)
{
    snprintf(run->state.result, sizeof(run->state.result), "%s", msg);
    portUpdateStateWrite(run->paths.state, &run->state);
    portUpdateUnlock();
    return result;
}

static const struct PortUpdateFlavor *installedFlavor(const struct PortUpdateEnv *env)
{
    char path[PORT_UPDATE_PATH];
    size_t size;
    char *text;
    const struct PortUpdateFlavor *flavor;

    if (!portUpdateJoin(path, sizeof(path), env->exe_dir, "package.txt")) {
        return NULL;
    }
    text = portUpdateReadFile(path, 4096, &size);
    flavor = portUpdateFlavorFromPackage(text);
    free(text);
    return flavor;
}

enum Found { FOUND_FAILED = -1, FOUND_NONE = 0, FOUND_NEWER = 1 };

/* Looks for a newer build: FOUND_NEWER with run->release and run->assetHex
 * when there is one. */
static int findRelease(struct Run *run, char *msg, size_t msgsize)
{
    static struct Buffer buffer;
    char url[PORT_UPDATE_PATH], number[24];

    snprintf(url, sizeof(url), "%s/latest/download/%s", run->env->downloads_url, run->flavor->build_info);
    if (!fetchSmall(run->env, url, &buffer, msg, msgsize)) {
        return FOUND_FAILED;
    }
    if (!portUpdateParseBuildInfo(buffer.data, run->env->downloads_url, &run->release)) {
        say(msg, msgsize, "unreadable build info", url);
        return FOUND_FAILED;
    }
    run->state.newest = run->release.build;
    snprintf(number, sizeof(number), "%ld", run->release.build);
    if (!run->release.ci || run->release.dirty) {
        say(msg, msgsize, "the latest release is not a clean CI build", number);
        return FOUND_FAILED;
    }
    if (run->release.build <= run->env->build) {
        say(msg, msgsize, "up to date, the latest release is build", number);
        return FOUND_NONE;
    }
    if (run->release.build == run->state.bad_build) {
        say(msg, msgsize, "skipping the rolled-back build", number);
        return FOUND_NONE;
    }
    snprintf(url, sizeof(url), "%s/download/%s/%s", run->env->downloads_url, run->release.tag, run->flavor->sums);
    if (!fetchSmall(run->env, url, &buffer, msg, msgsize)) {
        return FOUND_FAILED;
    }
    if (!portUpdateSumsLookup(buffer.data, run->flavor->asset, run->assetHex)) {
        say(msg, msgsize, "the release's checksums do not name", run->flavor->asset);
        return FOUND_FAILED;
    }
    return FOUND_NEWER;
}

/* Each listed file staging may have left (a swap renames them away; a failure
 * leaves them), then its folders, deepest first. A folder that another
 * listed file still needs fails to go now and goes with that file's turn. */
static int clearStaged(void *user, const char *name, const char *hex)
{
    const char *staged = user;
    char path[PORT_UPDATE_PATH];
    size_t end;

    (void) hex;
    if (!portUpdatePathSafe(name) || !portUpdateJoin(path, sizeof(path), staged, name)) {
        return 1;
    }
    portUpdateRemove(path);
    for (end = strlen(path); end > strlen(staged); end--) {
        if (path[end] == '/') {
            path[end] = '\0';
            portUpdateRemoveDir(path);
        }
    }
    return 1;
}

/* Removes what staging left once the files are swapped in or abandoned. */
static void dropStaging(struct Run *run, const char *staged, const char *sums)
{
    char path[PORT_UPDATE_PATH];

    portUpdateRemove(run->package);
    if (sums != NULL) {
        portUpdateSumsEach(sums, clearStaged, (void *) staged);
    }
    if (portUpdateJoin(path, sizeof(path), staged, "SHA256SUMS.txt")) {
        portUpdateRemove(path);
    }
    portUpdateRemoveDir(staged);
    portUpdateRemoveDir(run->tagDir);
}

static int installPackage(struct Run *run, char *msg, size_t msgsize)
{
    struct Stage stage;
    char staged[PORT_UPDATE_PATH], oldSumsPath[PORT_UPDATE_PATH];
    char *oldSums;
    size_t size;
    int ok;

    memset(&stage, 0, sizeof(stage));
    stage.flavor = run->flavor;
    if (!portUpdateJoin(staged, sizeof(staged), run->tagDir, "staged/") ||
        !portUpdateJoin(oldSumsPath, sizeof(oldSumsPath), run->env->exe_dir, "SHA256SUMS.txt")) {
        say(msg, msgsize, "path too long", run->tagDir);
        return 0;
    }
    stage.dir = staged;
    ok = unpack(&stage, run->package, msg, msgsize);
    oldSums = portUpdateReadFile(oldSumsPath, 1u << 20, &size);
    ok = ok && portUpdateSwapPackage(run->env, &run->paths, staged, stage.sums, oldSums, run->release.tag,
                                     run->release.build, msg, msgsize);
    dropStaging(run, staged, stage.sums);
    free(oldSums);
    free(stage.sums);
    free(stage.package);
    return ok;
}

int portUpdateRun(const struct PortUpdateEnv *env, int force, char *msg, size_t msgsize)
{
    struct Run run;
    const char *target;
    int ok, found;

    memset(&run, 0, sizeof(run));
    run.env = env;
    run.flavor = installedFlavor(env);
    if (!env->ci || env->http == NULL || run.flavor == NULL || !portUpdatePaths(env, &run.paths)) {
        say(msg, msgsize, !env->ci ? "a local build never updates" : run.flavor == NULL ?
            "this copy has no package.txt" : "no HTTPS transport", NULL);
        return PORT_UPDATE_NOT_ELIGIBLE;
    }
    if (run.flavor->kind == PORT_UPDATE_KIND_APPIMAGE && env->appimage == NULL) {
        say(msg, msgsize, "an AppImage build without $APPIMAGE", NULL);
        return PORT_UPDATE_NOT_ELIGIBLE;
    }
    portUpdateStateRead(run.paths.state, &run.state);
    if (!force && !portUpdateCheckDue(env->now, run.state.last_check)) {
        say(msg, msgsize, "checked less than 12 hours ago", NULL);
        return PORT_UPDATE_NOT_DUE;
    }
    if (!portUpdateMakeParents(run.paths.lock) || !portUpdateLock(run.paths.lock)) {
        say(msg, msgsize, "another copy is updating", NULL);
        return PORT_UPDATE_FAILED;
    }
    run.state.last_check = env->now;
    found = findRelease(&run, msg, msgsize);
    if (found != FOUND_NEWER) {
        return finish(&run, found == FOUND_NONE ? PORT_UPDATE_UP_TO_DATE : PORT_UPDATE_FAILED, msg);
    }
    target = run.flavor->kind == PORT_UPDATE_KIND_APPIMAGE ? env->appimage : env->exe_dir;
    if (run.flavor->kind != PORT_UPDATE_KIND_APPIMAGE && !portUpdateFolderWritable(target)) {
        say(msg, msgsize, "the game's folder is not writable", target);
        return finish(&run, PORT_UPDATE_FAILED, msg);
    }
    if (snprintf(run.tagDir, sizeof(run.tagDir), "%s%s/", run.paths.dir, run.release.tag) >=
            (int) sizeof(run.tagDir) ||
        snprintf(run.package, sizeof(run.package), "%s%s", run.flavor->kind == PORT_UPDATE_KIND_APPIMAGE ?
                 target : run.tagDir, run.flavor->kind == PORT_UPDATE_KIND_APPIMAGE ? ".download" :
                 run.flavor->asset) >= (int) sizeof(run.package)) {
        say(msg, msgsize, "path too long", run.release.tag);
        return finish(&run, PORT_UPDATE_FAILED, msg);
    }
    {
        char url[PORT_UPDATE_PATH];
        snprintf(url, sizeof(url), "%s/download/%s/%s", env->downloads_url, run.release.tag, run.flavor->asset);
        if (!fetchPackage(env, url, run.package, run.assetHex, msg, msgsize)) {
            portUpdateRemoveDir(run.tagDir);
            return finish(&run, PORT_UPDATE_FAILED, msg);
        }
    }
    if (run.flavor->kind == PORT_UPDATE_KIND_APPIMAGE) {
        ok = portUpdateMarkExecutable(run.package) &&
             portUpdateSwapFile(&run.paths, run.package, env->appimage, run.release.tag, run.release.build, msg,
                                msgsize);
        if (!ok) {
            portUpdateRemove(run.package);
        }
    } else {
        ok = installPackage(&run, msg, msgsize);
    }
    return finish(&run, ok ? PORT_UPDATE_INSTALLED : PORT_UPDATE_FAILED, msg);
}
