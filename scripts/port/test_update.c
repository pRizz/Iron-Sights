/* ROM-free tests of the in-game updater (port_update.h, FORK-095 UPD-002):
 * parsing, hashing, archives, and whole installs, rollbacks and failures
 * against the fake releases update_fixture.py writes. A fake transport maps
 * https://example.test/RELEASE/releases/... to FIXTURE/RELEASE/.... Run by
 * test_update.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "port_update.h"
#include "port_update_files.h"

/* Room for a fixture folder and any name below it. */
#define TEST_PATH (PORT_UPDATE_PATH * 2)

static const char *sFixture;
static int sFailures, sHttpFails, sHttpCorrupt, sHttpCalls;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            fprintf(stderr, "test_update: %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                              \
        }                                                                             \
    } while (0)

/* ---- the fake transport ------------------------------------------------- */

static int fakeHttp(const char *url, int stream, PortUpdateSink sink, void *user, char *reason, size_t size)
{
    const char *prefix = "https://example.test/", *rest, *slash, *file;
    char release[64], path[PORT_UPDATE_PATH];
    unsigned char *data;
    size_t length, at;

    (void) stream;
    sHttpCalls++;
    if (sHttpFails || strncmp(url, prefix, strlen(prefix)) != 0) {
        snprintf(reason, size, "No connection");
        return 0;
    }
    rest = url + strlen(prefix);
    slash = strchr(rest, '/');
    file = strrchr(url, '/') + 1;
    if (slash == NULL || (size_t) (slash - rest) >= sizeof(release)) {
        snprintf(reason, size, "Server error 404");
        return 0;
    }
    memcpy(release, rest, (size_t) (slash - rest));
    release[slash - rest] = '\0';
    snprintf(path, sizeof(path), "%s/%s/%s", sFixture, release, file);
    data = (unsigned char *) portUpdateReadFile(path, 1u << 26, &length);
    if (data == NULL) {
        snprintf(reason, size, "Server error 404");
        return 0;
    }
    if (sHttpCorrupt && stream) {  /* the package, not its build info or sums */
        data[length / 2] ^= 0xff;
    }
    for (at = 0; at < length; at += 1000) {
        if (!sink(user, data + at, length - at < 1000 ? length - at : 1000)) {
            free(data);
            snprintf(reason, size, "File too large");
            return 0;
        }
    }
    free(data);
    return 1;
}

/* ---- helpers ------------------------------------------------------------ */

struct Case {
    char dir[PORT_UPDATE_PATH], exe[TEST_PATH], downloads[256], appimage[TEST_PATH];
    struct PortUpdateEnv env;
    char msg[256];
};

static void setUp(struct Case *c, const char *install, const char *release)
{
    memset(c, 0, sizeof(*c));
    snprintf(c->dir, sizeof(c->dir), "%s/%s/", sFixture, install);
    snprintf(c->exe, sizeof(c->exe), "%sIronSights.exe", c->dir);
    snprintf(c->downloads, sizeof(c->downloads), "https://example.test/%s/releases", release);
    c->env.root = c->dir;
    c->env.exe_dir = c->dir;
    c->env.exe_path = c->exe;
    c->env.build = 100;
    c->env.ci = 1;
    c->env.downloads_url = c->downloads;
    c->env.now = 1000000000LL;
    c->env.http = fakeHttp;
}

static void useRelease(struct Case *c, const char *release)
{
    snprintf(c->downloads, sizeof(c->downloads), "https://example.test/%s/releases", release);
}

/* 1 when DIR/NAME holds exactly TEXT. */
static int holds(const struct Case *c, const char *name, const char *text)
{
    char path[TEST_PATH];
    size_t size;
    char *data;
    int same;

    snprintf(path, sizeof(path), "%s%s", c->dir, name);
    data = portUpdateReadFile(path, 1u << 20, &size);
    same = data != NULL && size == strlen(text) && memcmp(data, text, size) == 0;
    if (!same) {
        fprintf(stderr, "test_update: %s holds \"%s\", not \"%s\"\n", path, data != NULL ? data : "(nothing)", text);
    }
    free(data);
    return same;
}

static int has(const struct Case *c, const char *name)
{
    char path[TEST_PATH];

    snprintf(path, sizeof(path), "%s%s", c->dir, name);
    return portUpdateExists(path);
}

static int run(struct Case *c, int force)
{
    const int result = portUpdateRun(&c->env, force, c->msg, sizeof(c->msg));

    printf("test_update:   run -> %d (%s)\n", result, c->msg);
    return result;
}

static int playerDataUntouched(const struct Case *c)
{
    return holds(c, "midas.ini", "[updates]\nmode=automatic\n") && holds(c, "Data/Saves/eeprom.bin", "player save");
}

static int oldBuildIntact(const struct Case *c)
{
    return holds(c, "IronSights.exe", "old exe") && holds(c, "Data/README.txt", "old readme") &&
           holds(c, "gamecontrollerdefault.txt", "edited by the player") && holds(c, "old-only.txt", "obsolete") &&
           !has(c, "new-only.txt") && !has(c, "IronSights.exe.old") && !has(c, "Data/Updates/journal.txt") &&
           !has(c, "Data/Updates/build-200.1-0123456789ab") && playerDataUntouched(c);
}

/* ---- parsing, hashing, archives ----------------------------------------- */

static void testSha256(void)
{
    char hex[65], *million = (char *) malloc(1000000);
    struct PortSha256 s;
    int i;

    portSha256Of("", 0, hex);
    CHECK(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);
    portSha256Of("abc", 3, hex);
    CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    memset(million, 'a', 1000000);
    portSha256Init(&s);
    for (i = 0; i < 1000; i++) {
        portSha256Add(&s, million + i * 1000, 1000);
    }
    portSha256Hex(&s, hex);
    CHECK(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
    free(million);
}

static void testCore(void)
{
    struct PortUpdateRelease r;
    char hex[65];
    const char *base = "https://github.com/pRizz/Iron-Sights/releases";
    const char *good = "{\"build_number\": 699, \"ci\": true, \"dirty\": false, \"release_url\": "
                       "\"https://github.com/pRizz/Iron-Sights/releases/tag/build-454.1-c62e2236a117\"}";

    CHECK(portUpdateModeFromIni(NULL) == PORT_UPDATE_DEFAULT_MODE);
    CHECK(portUpdateModeFromIni("[display]\nmode=automatic\n") == PORT_UPDATE_DEFAULT_MODE);
    CHECK(portUpdateModeFromIni("[Updates]\r\n Mode = Automatic \r\n") == PORT_UPDATE_AUTOMATIC);
    CHECK(portUpdateModeFromIni("[updates]\nmode=notify\n[display]\nmode=off\n") == PORT_UPDATE_NOTIFY);
    CHECK(portUpdateModeFromIni("[updates]\nmode=sometimes\n") == PORT_UPDATE_DEFAULT_MODE);
    CHECK(portUpdateCheckDue(1000, 0));
    CHECK(!portUpdateCheckDue(1000 + 11 * 3600, 1000));
    CHECK(portUpdateCheckDue(1000 + 12 * 3600, 1000));
    CHECK(portUpdateCheckDue(500, 1000));
    CHECK(portUpdateTagValid("build-454.1-c62e2236a117"));
    CHECK(!portUpdateTagValid("build-454.1-c62e2236a11"));
    CHECK(!portUpdateTagValid("build-454.1-C62E2236A117"));
    CHECK(!portUpdateTagValid("build-454-c62e2236a117"));
    CHECK(!portUpdateTagValid("build-454.1-c62e2236a117/../x"));
    CHECK(portUpdatePathSafe("Data/README.txt") && portUpdatePathSafe("lib/libsentry.so"));
    CHECK(!portUpdatePathSafe("../evil") && !portUpdatePathSafe("/etc/passwd") && !portUpdatePathSafe("a//b"));
    CHECK(!portUpdatePathSafe("a/./b") && !portUpdatePathSafe("C:evil") && !portUpdatePathSafe("a\\b"));
    CHECK(!portUpdatePathSafe("") && !portUpdatePathSafe("dir/"));
    CHECK(portUpdatePackagePath("Data/README.txt") && portUpdatePackagePath("Data/shaders_base.txt"));
    CHECK(portUpdatePackagePath("lib/libSDL2-2.0.so.0") && portUpdatePackagePath("IronSights.exe"));
    CHECK(!portUpdatePackagePath("Data/Saves/eeprom.bin") && !portUpdatePackagePath("data/Profiles/a.geprofile"));
    CHECK(!portUpdatePackagePath("Data/baserom.u.z64") && !portUpdatePackagePath("MIDAS.INI"));
    CHECK(!portUpdatePackagePath("DATA/Saves/eeprom.bin") && !portUpdatePackagePath("Data/readme.txt"));
    CHECK(portUpdateParseBuildInfo(good, base, &r) && r.build == 699 && r.ci && !r.dirty);
    CHECK(strcmp(r.tag, "build-454.1-c62e2236a117") == 0);
    CHECK(!portUpdateParseBuildInfo(good, "https://github.com/someone/else/releases", &r));
    CHECK(!portUpdateParseBuildInfo("{\"build_number\": 1, \"ci\": true, \"dirty\": false, \"release_url\": "
                                    "\"https://github.com/pRizz/Iron-Sights/releases/tag/v1\"}", base, &r));
    CHECK(portUpdateFlavorFromPackage("# x\nschema=1\nflavor=linux-x86_64-tar\n") ==
          portUpdateFlavorNamed("linux-x86_64-tar"));
    CHECK(portUpdateFlavorFromPackage("schema=2\nflavor=linux-x86_64-tar\n") == NULL);
    CHECK(portUpdateFlavorFromPackage("schema=1\nflavor=macos-app\n") == NULL);
    CHECK(portUpdateSumsLookup("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  a b.txt\n",
                               "a b.txt", hex));
    CHECK(portUpdateSumsEach("not a sums line\n", NULL, NULL) == -1);
}

/* Every flavor matches package_identity.py's FLAVORS (flavors.txt). */
static void testFlavorsMatchThePackagers(void)
{
    char path[PORT_UPDATE_PATH], line[512], name[64], asset[128], sums[128], info[128];
    FILE *f;
    size_t count = 0;

    snprintf(path, sizeof(path), "%s/flavors.txt", sFixture);
    f = fopen(path, "r");
    CHECK(f != NULL);
    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        const struct PortUpdateFlavor *flavor;
        CHECK(sscanf(line, "%63s %127s %127s %127s", name, asset, sums, info) == 4);
        flavor = portUpdateFlavorNamed(name);
        CHECK(flavor != NULL);
        if (flavor != NULL) {
            CHECK(strcmp(flavor->asset, asset) == 0 && strcmp(flavor->sums, sums) == 0);
            CHECK(strcmp(flavor->build_info, info) == 0);
        }
        count++;
    }
    if (f != NULL) {
        fclose(f);
    }
    CHECK(count > 0 && portUpdateFlavorAt(count - 1) != NULL && portUpdateFlavorAt(count) == NULL);
}

static int countEntry(void *user, const char *name, const unsigned char *data, size_t size, int executable)
{
    (void) name;
    (void) data;
    (void) size;
    (void) executable;
    (*(int *) user)++;
    return 1;
}

static void testArchivesRefuseGarbage(void)
{
    const unsigned char junk[64] = { 0x1f, 0x8b, 8, 0 };
    unsigned char *out;
    size_t outsize;
    char err[160];
    int count = 0;

    CHECK(!portUpdateZipEach(junk, sizeof(junk), countEntry, &count, err, sizeof(err)));
    CHECK(!portUpdateGunzip(junk, sizeof(junk), &out, &outsize, err, sizeof(err)));
    CHECK(!portUpdateTarEach(junk, sizeof(junk), countEntry, &count, err, sizeof(err)));
    CHECK(count == 0);
}

/* ---- installs ------------------------------------------------------------ */

static void testZipInstallAndConfirm(void)
{
    struct Case c;
    struct PortUpdateState state;
    char statePath[TEST_PATH];

    setUp(&c, "zip-install", "good");
    CHECK(run(&c, 0) == PORT_UPDATE_INSTALLED);
    CHECK(holds(&c, "IronSights.exe", "new exe") && holds(&c, "IronSights.exe.old", "old exe"));
    CHECK(holds(&c, "Data/README.txt", "new readme") && holds(&c, "new-only.txt", "added"));
    CHECK(holds(&c, "gamecontrollerdefault.txt", "new defaults"));
    CHECK(!has(&c, "old-only.txt") && holds(&c, "old-only.txt.old", "obsolete"));
    CHECK(has(&c, "Data/Updates/journal.txt") && !has(&c, "Data/Updates/build-200.1-0123456789ab"));
    CHECK(playerDataUntouched(&c));
    snprintf(statePath, sizeof(statePath), "%sData/Updates/state.txt", c.dir);
    portUpdateStateRead(statePath, &state);
    CHECK(state.last_check == c.env.now && state.newest == 200);

    c.env.build = 200;  /* the new build starts */
    CHECK(portUpdateRecover(&c.env, c.msg, sizeof(c.msg)) == 0 && strstr(c.msg, "start 1") != NULL);
    portUpdateConfirm(&c.env);
    CHECK(!has(&c, "IronSights.exe.old") && !has(&c, "old-only.txt.old") && !has(&c, "Data/README.txt.old"));
    CHECK(holds(&c, "gamecontrollerdefault.txt.edited", "edited by the player"));
    CHECK(!has(&c, "Data/Updates/journal.txt") && !has(&c, "Data/Updates/attempts.txt"));
    CHECK(portUpdateRecover(&c.env, c.msg, sizeof(c.msg)) == 0 && c.msg[0] == '\0');
    CHECK(run(&c, 1) == PORT_UPDATE_UP_TO_DATE);
}

static void testThrottle(void)
{
    struct Case c;
    const long long start = 1000000000LL;

    setUp(&c, "zip-throttle", "older");
    CHECK(run(&c, 0) == PORT_UPDATE_UP_TO_DATE);
    c.env.now = start + 3600;
    CHECK(run(&c, 0) == PORT_UPDATE_NOT_DUE);
    sHttpCalls = 0;
    CHECK(run(&c, 1) == PORT_UPDATE_UP_TO_DATE && sHttpCalls == 1);
    c.env.now = start + 3600 + 12 * 3600;
    sHttpFails = 1;
    CHECK(run(&c, 0) == PORT_UPDATE_FAILED);  /* a failed check counts too */
    sHttpFails = 0;
    c.env.now += 3600;
    CHECK(run(&c, 0) == PORT_UPDATE_NOT_DUE);
    CHECK(oldBuildIntact(&c));
}

static void testRollbackAfterThreeStarts(void)
{
    struct Case c;

    setUp(&c, "zip-rollback", "good");
    CHECK(run(&c, 0) == PORT_UPDATE_INSTALLED);
    c.env.build = 200;
    CHECK(portUpdateRecover(&c.env, c.msg, sizeof(c.msg)) == 0);
    CHECK(portUpdateRecover(&c.env, c.msg, sizeof(c.msg)) == 0);
    CHECK(portUpdateRecover(&c.env, c.msg, sizeof(c.msg)) == 1);  /* restart into the old build */
    printf("test_update:   recover: %s\n", c.msg);
    CHECK(oldBuildIntact(&c) && !has(&c, "IronSights.exe.bad"));
    c.env.build = 100;
    CHECK(run(&c, 1) == PORT_UPDATE_UP_TO_DATE && strstr(c.msg, "rolled-back") != NULL);
}

static void testFailureMidSwapUndoesEverything(void)
{
    struct Case c;
    char blocker[TEST_PATH];
    FILE *f = NULL;

    setUp(&c, "zip-failmid", "good");
    /* package.txt is the last file swapped; a folder where its .old must go
     * stops the swap after every other file has moved. */
    snprintf(blocker, sizeof(blocker), "%spackage.txt.old/keep", c.dir);
    CHECK(portUpdateMakeParents(blocker) && (f = fopen(blocker, "wb")) != NULL);
    if (f != NULL) {
        fclose(f);
    }
    CHECK(run(&c, 0) == PORT_UPDATE_FAILED && strstr(c.msg, "package.txt") != NULL);
    CHECK(oldBuildIntact(&c) && has(&c, "package.txt"));
}

static void testRefusedReleases(void)
{
    static const char *const refused[][2] = {
        { "badinner", "does not match" }, { "unsafe", "unsafe path" }, { "wrongflavor", "not a windows-x86-zip" },
        { "notci", "not a clean CI build" }, { "missing", "404" }, { "saves", "unsafe path Data/Saves" },
    };
    struct Case c;
    size_t i;

    setUp(&c, "zip-bad", "good");
    for (i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
        useRelease(&c, refused[i][0]);
        CHECK(run(&c, 1) == PORT_UPDATE_FAILED && strstr(c.msg, refused[i][1]) != NULL);
        CHECK(oldBuildIntact(&c));
    }
    useRelease(&c, "good");
    sHttpCorrupt = 1;
    CHECK(run(&c, 1) == PORT_UPDATE_FAILED && strstr(c.msg, "checksum mismatch") != NULL);
    sHttpCorrupt = 0;
    CHECK(oldBuildIntact(&c));
    c.env.ci = 0;
    CHECK(run(&c, 1) == PORT_UPDATE_NOT_ELIGIBLE);
    c.env.ci = 1;
    c.env.exe_dir = sFixture;  /* no package.txt */
    CHECK(run(&c, 1) == PORT_UPDATE_NOT_ELIGIBLE);
}

static void testInterruptedSwapIsUndone(void)
{
    struct Case c;
    char old[TEST_PATH + 8], journal[TEST_PATH], text[TEST_PATH * 3];

    setUp(&c, "zip-interrupted", "good");
    /* A crash after the executable was swapped, before the journal ended. */
    snprintf(old, sizeof(old), "%s.old", c.exe);
    snprintf(journal, sizeof(journal), "%sData/Updates/journal.txt", c.dir);
    snprintf(text, sizeof(text), "tag=build-200.1-0123456789ab\nbuild=200\nmoved %s\nplaced %s\n", c.exe, c.exe);
    CHECK(portUpdateRename(c.exe, old) && portUpdateWriteFile(c.exe, "new exe", 7));
    CHECK(portUpdateMakeParents(journal) && portUpdateWriteFile(journal, text, strlen(text)));
    CHECK(portUpdateRecover(&c.env, c.msg, sizeof(c.msg)) == 1);
    printf("test_update:   recover: %s\n", c.msg);
    CHECK(oldBuildIntact(&c));
}

/* What the run reported (UPD-003's screens read these). */
static struct {
    int phases[8], count, cancelAt;
    long build;
    long long bytes;
} sProgress;

static void onProgress(void *user, int phase, long build, long long bytes)
{
    (void) user;
    if (sProgress.count == 0 || sProgress.phases[sProgress.count - 1] != phase) {
        if (sProgress.count < 8) {
            sProgress.phases[sProgress.count++] = phase;
        }
    }
    sProgress.build = build;
    sProgress.bytes = bytes;
}

/* Cancelled once the run has reached phase cancelAt (-1: never). */
static int onCancelled(void *user)
{
    (void) user;
    return sProgress.cancelAt >= 0 && sProgress.count > 0 && sProgress.phases[sProgress.count - 1] >= sProgress.cancelAt;
}

static void watch(struct Case *c, int cancelAt)
{
    memset(&sProgress, 0, sizeof(sProgress));
    sProgress.cancelAt = cancelAt;
    c->env.progress = onProgress;
    c->env.cancelled = onCancelled;
}

static void testCheckOnlyAndProgress(void)
{
    struct Case c;
    struct PortUpdateState state;
    char statePath[TEST_PATH];

    setUp(&c, "zip-checkonly", "good");
    snprintf(statePath, sizeof(statePath), "%sData/Updates/state.txt", c.dir);
    CHECK(portUpdateEligible(&c.env, c.msg, sizeof(c.msg)) && c.msg[0] == '\0');
    watch(&c, -1);
    c.env.check_only = 1;
    CHECK(run(&c, 0) == PORT_UPDATE_AVAILABLE && strstr(c.msg, "200") != NULL);
    CHECK(sProgress.count == 1 && sProgress.phases[0] == PORT_UPDATE_PHASE_CHECKING);
    CHECK(oldBuildIntact(&c));
    portUpdateStateRead(statePath, &state);
    CHECK(state.newest == 200 && strcmp(state.newest_tag, "build-200.1-0123456789ab") == 0);
    CHECK(state.outcome == PORT_UPDATE_AVAILABLE && state.last_check == c.env.now);
    CHECK(run(&c, 0) == PORT_UPDATE_NOT_DUE);

    watch(&c, -1);
    c.env.check_only = 0;
    CHECK(run(&c, 1) == PORT_UPDATE_INSTALLED);
    CHECK(sProgress.count == 3 && sProgress.phases[0] == PORT_UPDATE_PHASE_CHECKING &&
          sProgress.phases[1] == PORT_UPDATE_PHASE_DOWNLOADING && sProgress.phases[2] == PORT_UPDATE_PHASE_INSTALLING);
    CHECK(sProgress.build == 200 && holds(&c, "IronSights.exe", "new exe"));
    portUpdateStateRead(statePath, &state);
    CHECK(state.outcome == PORT_UPDATE_INSTALLED);
    c.env.ci = 0;
    CHECK(!portUpdateEligible(&c.env, c.msg, sizeof(c.msg)) && strstr(c.msg, "local build") != NULL);
}

static void testCancelMovesNothing(void)
{
    struct Case c;
    struct PortUpdateState state;
    char statePath[TEST_PATH];

    setUp(&c, "zip-cancel", "good");
    snprintf(statePath, sizeof(statePath), "%sData/Updates/state.txt", c.dir);
    watch(&c, PORT_UPDATE_PHASE_CHECKING);  /* skipped while the check runs */
    CHECK(run(&c, 1) == PORT_UPDATE_CANCELLED && strstr(c.msg, "before the download") != NULL);
    CHECK(oldBuildIntact(&c));
    watch(&c, PORT_UPDATE_PHASE_DOWNLOADING);  /* skipped during the download */
    CHECK(run(&c, 1) == PORT_UPDATE_CANCELLED && strstr(c.msg, "during the download") != NULL);
    CHECK(sProgress.phases[sProgress.count - 1] == PORT_UPDATE_PHASE_DOWNLOADING);
    CHECK(oldBuildIntact(&c));
    portUpdateStateRead(statePath, &state);
    CHECK(state.outcome == PORT_UPDATE_CANCELLED && state.last_check == c.env.now);  /* a finished check */
    watch(&c, -1);
    CHECK(run(&c, 1) == PORT_UPDATE_INSTALLED && holds(&c, "IronSights.exe", "new exe"));
}

static void testTarballInstall(void)
{
    struct Case c;

    setUp(&c, "tar-install", "good");
    CHECK(run(&c, 0) == PORT_UPDATE_INSTALLED);
    CHECK(holds(&c, "IronSights.exe", "new exe") && holds(&c, "lib/libnew.so", "new library"));
    CHECK(holds(&c, "licenses/a-long-bundled-package-name-a-long-bundled-package-name-a-long-bundled-package-name-"
                    "a-long-bundled-package-name-a-long-bundled-package-name-/copyright", "licence"));
#ifndef _WIN32
    CHECK(portUpdateIsExecutable(c.exe));
#endif
    CHECK(playerDataUntouched(&c));
}

static void testAppImageInstall(void)
{
    struct Case c;

    setUp(&c, "appimage-install", "good");
    snprintf(c.appimage, sizeof(c.appimage), "%sIron-Sights.AppImage", c.dir);
    c.env.appimage = c.appimage;
    CHECK(run(&c, 0) == PORT_UPDATE_INSTALLED);
    CHECK(holds(&c, "Iron-Sights.AppImage", "new appimage") && holds(&c, "Iron-Sights.AppImage.old", "old appimage"));
    CHECK(!has(&c, "Iron-Sights.AppImage.download") && holds(&c, "IronSights.exe", "old exe"));
#ifndef _WIN32
    CHECK(portUpdateIsExecutable(c.appimage));
#endif
    portUpdateConfirm(&c.env);
    CHECK(!has(&c, "Iron-Sights.AppImage.old"));
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: test_update FIXTURE\n");
        return 2;
    }
    sFixture = argv[1];
    testSha256();
    testCore();
    testFlavorsMatchThePackagers();
    testArchivesRefuseGarbage();
    testZipInstallAndConfirm();
    testThrottle();
    testRollbackAfterThreeStarts();
    testFailureMidSwapUndoesEverything();
    testRefusedReleases();
    testInterruptedSwapIsUndone();
    testCheckOnlyAndProgress();
    testCancelMovesNothing();
    testTarballInstall();
    testAppImageInstall();
    if (sFailures != 0) {
        fprintf(stderr, "test_update: %d check(s) failed\n", sFailures);
        return 1;
    }
    return 0;
}
