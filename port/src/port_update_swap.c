/* The in-game updater's swap, journal, rollback and confirmation
 * (port_update.h). Every step of a swap is a rename, logged in
 * Data/Updates/journal.txt before the next one starts:
 *
 *   moved P    P was renamed to P.old
 *   edited P   P differed from the installed SHA256SUMS.txt (kept as P.edited)
 *   placed P   the new file was renamed to P
 *   installed  every step finished
 *
 * Undoing walks the lines backwards: a placed P is renamed to P.bad (a
 * running executable can be renamed but not deleted on Windows; cleanup.txt
 * retries the delete at later starts) and a moved P gets P.old back. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_update.h"
#include "port_update_files.h"

#define TEXT_LIMIT (1u << 20)

int portUpdatePaths(const struct PortUpdateEnv *env, struct PortUpdatePaths *paths)
{
    return portUpdateJoin(paths->dir, sizeof(paths->dir), env->root, "Data/Updates/") &&
           portUpdateJoin(paths->state, sizeof(paths->state), paths->dir, "state.txt") &&
           portUpdateJoin(paths->journal, sizeof(paths->journal), paths->dir, "journal.txt") &&
           portUpdateJoin(paths->attempts, sizeof(paths->attempts), paths->dir, "attempts.txt") &&
           portUpdateJoin(paths->lock, sizeof(paths->lock), paths->dir, "lock") &&
           portUpdateJoin(paths->cleanup, sizeof(paths->cleanup), paths->dir, "cleanup.txt");
}

static void say(char *msg, size_t size, const char *what, const char *detail)
{
    if (size > 0) {
        snprintf(msg, size, "%s%s%s", what, detail != NULL ? ": " : "", detail != NULL ? detail : "");
    }
}

/* ---- state -------------------------------------------------------------- */

void portUpdateStateRead(const char *path, struct PortUpdateState *state)
{
    size_t size;
    char *text = portUpdateReadFile(path, 4096, &size), value[32];

    memset(state, 0, sizeof(*state));
    if (text == NULL) {
        return;
    }
    if (portUpdateKeyValue(text, "last_check", value, sizeof(value))) state->last_check = strtoll(value, NULL, 10);
    if (portUpdateKeyValue(text, "newest", value, sizeof(value))) state->newest = strtol(value, NULL, 10);
    if (portUpdateKeyValue(text, "bad_build", value, sizeof(value))) state->bad_build = strtol(value, NULL, 10);
    portUpdateKeyValue(text, "result", state->result, sizeof(state->result));
    free(text);
}

int portUpdateStateWrite(const char *path, const struct PortUpdateState *state)
{
    char text[512];
    const int n = snprintf(text, sizeof(text), "last_check=%lld\nnewest=%ld\nbad_build=%ld\nresult=%s\n",
                           state->last_check, state->newest, state->bad_build, state->result);

    return n > 0 && (size_t) n < sizeof(text) && portUpdateMakeParents(path) &&
           portUpdateWriteFile(path, text, (size_t) n);
}

/* ---- cleanup ------------------------------------------------------------ */

static void addCleanup(const struct PortUpdatePaths *paths, const char *path)
{
    FILE *f = fopen(paths->cleanup, "ab");

    if (f != NULL) {
        fprintf(f, "%s\n", path);
        fclose(f);
    }
}

/* Deletes P or remembers it for a later start. */
static void discard(const struct PortUpdatePaths *paths, const char *path)
{
    if (!portUpdateRemove(path)) {
        addCleanup(paths, path);
    }
}

static void retryCleanup(const struct PortUpdatePaths *paths)
{
    size_t size;
    char *text = portUpdateReadFile(paths->cleanup, TEXT_LIMIT, &size), *line, *next;
    FILE *left;

    if (text == NULL) {
        return;
    }
    portUpdateRemove(paths->cleanup);
    for (line = text; *line != '\0'; line = next) {
        next = strchr(line, '\n');
        next = next != NULL ? (*next = '\0', next + 1) : line + strlen(line);
        if (*line != '\0' && !portUpdateRemove(line) && (left = fopen(paths->cleanup, "ab")) != NULL) {
            fprintf(left, "%s\n", line);
            fclose(left);
        }
    }
    free(text);
}

/* ---- the journal, backwards ---------------------------------------------- */

/* Splits text into lines in place; returns how many, at most max. */
static size_t splitLines(char *text, char **lines, size_t max)
{
    size_t count = 0;
    char *at = text;

    while (*at != '\0' && count < max) {
        char *end = strchr(at, '\n');
        lines[count++] = at;
        if (end == NULL) {
            break;
        }
        *end = '\0';
        at = end + 1;
    }
    return count;
}

static int lineIs(const char *line, const char *word, const char **path)
{
    const size_t n = strlen(word);

    if (!portUpdateStartsWith(line, word) || line[n] != (path != NULL ? ' ' : '\0')) {
        return 0;
    }
    if (path != NULL) {
        *path = line + n + 1;
    }
    return 1;
}

static int journalSays(char **lines, size_t count, const char *word, const char *path)
{
    size_t i;
    const char *named;

    for (i = 0; i < count; i++) {
        if (lineIs(lines[i], word, &named) && strcmp(named, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void undoLines(const struct PortUpdatePaths *paths, char **lines, size_t count)
{
    char other[PORT_UPDATE_PATH];
    const char *path;
    size_t i;

    for (i = count; i-- > 0;) {
        if (lineIs(lines[i], "placed", &path) && portUpdateJoin(other, sizeof(other), path, ".bad")) {
            portUpdateRemove(other);
            if (portUpdateRename(path, other)) {
                discard(paths, other);
            } else {
                discard(paths, path);
            }
        } else if (lineIs(lines[i], "moved", &path) && portUpdateJoin(other, sizeof(other), path, ".old")) {
            portUpdateReplace(other, path);
        }
    }
}

#define MAX_LINES 4096

struct Journal {
    char *text;
    char *lines[MAX_LINES];
    size_t count;
    int installed;
};

static int readJournal(const struct PortUpdatePaths *paths, struct Journal *journal)
{
    size_t size;

    journal->text = portUpdateReadFile(paths->journal, TEXT_LIMIT, &size);
    if (journal->text == NULL) {
        return 0;
    }
    journal->count = splitLines(journal->text, journal->lines, MAX_LINES);
    journal->installed = journal->count > 0 && lineIs(journal->lines[journal->count - 1], "installed", NULL);
    return 1;
}

static long journalBuild(const struct Journal *journal)
{
    size_t i;

    for (i = 0; i < journal->count; i++) {
        if (portUpdateStartsWith(journal->lines[i], "build=")) {
            return strtol(journal->lines[i] + 6, NULL, 10);
        }
    }
    return 0;
}

static void endJournal(const struct PortUpdatePaths *paths)
{
    portUpdateRemove(paths->journal);
    portUpdateRemove(paths->attempts);
}

/* ---- swapping ----------------------------------------------------------- */

static int beginSwap(const struct PortUpdatePaths *paths, const char *tag, long build)
{
    char header[128];

    snprintf(header, sizeof(header), "tag=%s\nbuild=%ld", tag, build);
    return portUpdateMakeParents(paths->journal) && portUpdateJournalOpen(paths->journal, header);
}

/* Moves target aside to target.old (when present), then staged to target. */
static int swapOne(const char *staged, const char *target, const char *oldSums, const char *name)
{
    char old[PORT_UPDATE_PATH], listed[65], actual[65];

    if (!portUpdateJoin(old, sizeof(old), target, ".old")) {
        return 0;
    }
    if (portUpdateExists(target)) {
        if (oldSums != NULL && name != NULL && portUpdateSumsLookup(oldSums, name, listed)) {
            size_t size;
            char *data = portUpdateReadFile(target, 1u << 30, &size);
            if (data != NULL) {
                portSha256Of(data, size, actual);
                free(data);
            }
            if (data != NULL && strcmp(actual, listed) != 0 && !portUpdateJournalLine("edited", target)) {
                return 0;
            }
        }
        portUpdateRemove(old);
        if (!portUpdateRename(target, old) || !portUpdateJournalLine("moved", target)) {
            return 0;
        }
    } else if (!portUpdateMakeParents(target)) {
        return 0;
    }
    return portUpdateRename(staged, target) && portUpdateJournalLine("placed", target);
}

static int failSwap(const struct PortUpdatePaths *paths, char *msg, size_t msgsize, const char *what,
                    const char *name)
{
    struct Journal journal;

    portUpdateJournalClose();
    if (readJournal(paths, &journal)) {
        undoLines(paths, journal.lines, journal.count);
        free(journal.text);
    }
    endJournal(paths);
    say(msg, msgsize, what, name);
    return 0;
}

struct SwapWalk {
    const struct PortUpdateEnv *env;
    const char *staged;
    const char *oldSums;
    const char *newSums;
    int moveOnly;
    const char *failed;
};

static int swapListed(void *user, const char *name, const char *hex)
{
    struct SwapWalk *walk = (struct SwapWalk *) user;
    char from[PORT_UPDATE_PATH], target[PORT_UPDATE_PATH], old[PORT_UPDATE_PATH], listed[65];

    (void) hex;
    walk->failed = name;
    if (!portUpdatePackagePath(name) || !portUpdateJoin(target, sizeof(target), walk->env->exe_dir, name)) {
        return 0;
    }
    if (walk->moveOnly) {
        /* A file only the old package had: aside now, deleted on confirm. */
        if (portUpdateSumsLookup(walk->newSums, name, listed) || !portUpdateExists(target)) {
            return 1;
        }
        return portUpdateJoin(old, sizeof(old), target, ".old") && (portUpdateRemove(old), 1) &&
               portUpdateRename(target, old) && portUpdateJournalLine("moved", target);
    }
    return portUpdateJoin(from, sizeof(from), walk->staged, name) &&
           swapOne(from, target, walk->oldSums, name);
}

int portUpdateSwapPackage(const struct PortUpdateEnv *env, const struct PortUpdatePaths *paths,
                          const char *staged, const char *newSums, const char *oldSums, const char *tag,
                          long build, char *msg, size_t msgsize)
{
    struct SwapWalk walk;
    char from[PORT_UPDATE_PATH], target[PORT_UPDATE_PATH];

    if (!beginSwap(paths, tag, build)) {
        say(msg, msgsize, "cannot write the journal", paths->journal);
        return 0;
    }
    memset(&walk, 0, sizeof(walk));
    walk.env = env;
    walk.staged = staged;
    walk.oldSums = oldSums;
    walk.newSums = newSums;
    if (portUpdateSumsEach(newSums, swapListed, &walk) < 0) {
        return failSwap(paths, msg, msgsize, "cannot replace", walk.failed);
    }
    if (!portUpdateJoin(from, sizeof(from), staged, "SHA256SUMS.txt") ||
        !portUpdateJoin(target, sizeof(target), env->exe_dir, "SHA256SUMS.txt") ||
        !swapOne(from, target, NULL, NULL)) {
        return failSwap(paths, msg, msgsize, "cannot replace", "SHA256SUMS.txt");
    }
    walk.moveOnly = 1;
    if (oldSums != NULL && portUpdateSumsEach(oldSums, swapListed, &walk) < 0) {
        return failSwap(paths, msg, msgsize, "cannot move aside", walk.failed);
    }
    if (!portUpdateJournalLine("installed", "") || !portUpdateJournalClose()) {
        return failSwap(paths, msg, msgsize, "cannot write the journal", paths->journal);
    }
    say(msg, msgsize, "installed", tag);
    return 1;
}

int portUpdateSwapFile(const struct PortUpdatePaths *paths, const char *staged, const char *target,
                       const char *tag, long build, char *msg, size_t msgsize)
{
    if (!beginSwap(paths, tag, build)) {
        say(msg, msgsize, "cannot write the journal", paths->journal);
        return 0;
    }
    if (!swapOne(staged, target, NULL, NULL)) {
        return failSwap(paths, msg, msgsize, "cannot replace", target);
    }
    if (!portUpdateJournalLine("installed", "") || !portUpdateJournalClose()) {
        return failSwap(paths, msg, msgsize, "cannot write the journal", paths->journal);
    }
    say(msg, msgsize, "installed", tag);
    return 1;
}

/* ---- recovery and confirmation ------------------------------------------ */

static int journalPlacedRunning(const struct PortUpdateEnv *env, struct Journal *journal)
{
    return journalSays(journal->lines, journal->count, "placed", env->exe_path) ||
           (env->appimage != NULL && journalSays(journal->lines, journal->count, "placed", env->appimage));
}

/* Undoes the journal; WHY is a format naming the build with %ld. */
static int rollBack(const struct PortUpdateEnv *env, const struct PortUpdatePaths *paths,
                    struct Journal *journal, char *msg, size_t msgsize, const char *why)
{
    struct PortUpdateState state;
    const int restart = journalPlacedRunning(env, journal);
    const long build = journalBuild(journal);

    undoLines(paths, journal->lines, journal->count);
    endJournal(paths);
    if (journal->installed) {
        portUpdateStateRead(paths->state, &state);
        state.bad_build = build;
        snprintf(state.result, sizeof(state.result), "rolled back build %ld", build);
        portUpdateStateWrite(paths->state, &state);
    }
    snprintf(msg, msgsize, why, build);
    return restart;
}

int portUpdateRecover(const struct PortUpdateEnv *env, char *msg, size_t msgsize)
{
    struct PortUpdatePaths paths;
    struct Journal journal;
    char text[24];  /* a 64-bit long and a line break */
    long attempts;
    int restart = 0;

    say(msg, msgsize, "", NULL);
    if (!portUpdatePaths(env, &paths)) {
        return 0;
    }
    retryCleanup(&paths);
    if (!readJournal(&paths, &journal)) {
        return 0;
    }
    if (!journal.installed) {
        restart = rollBack(env, &paths, &journal, msg, msgsize, "undid an interrupted install of build %ld");
        free(journal.text);
        return restart;
    }
    {
        size_t size;
        char *count = portUpdateReadFile(paths.attempts, 64, &size);
        attempts = count != NULL ? strtol(count, NULL, 10) + 1 : 1;
        free(count);
    }
    if (attempts >= PORT_UPDATE_MAX_ATTEMPTS) {
        restart = rollBack(env, &paths, &journal, msg, msgsize,
                           "rolled back build %ld: it never reached the legal screen");
    } else {
        snprintf(text, sizeof(text), "%ld\n", attempts);
        portUpdateWriteFile(paths.attempts, text, strlen(text));
        snprintf(msg, msgsize, "start %ld of build %ld after its install", attempts, journalBuild(&journal));
    }
    free(journal.text);
    return restart;
}

long portUpdateConfirm(const struct PortUpdateEnv *env)
{
    struct PortUpdatePaths paths;
    struct Journal journal;
    char old[PORT_UPDATE_PATH], kept[PORT_UPDATE_PATH];
    const char *path;
    size_t i;
    long build = 0;

    if (!portUpdatePaths(env, &paths) || !readJournal(&paths, &journal)) {
        return 0;
    }
    if (journal.installed) {
        build = journalBuild(&journal);
        for (i = 0; i < journal.count; i++) {
            if (!lineIs(journal.lines[i], "moved", &path) || !portUpdateJoin(old, sizeof(old), path, ".old")) {
                continue;
            }
            if (journalSays(journal.lines, journal.count, "edited", path) &&
                portUpdateJoin(kept, sizeof(kept), path, ".edited") && portUpdateReplace(old, kept)) {
                continue;
            }
            discard(&paths, old);
        }
        endJournal(&paths);
    }
    free(journal.text);
    return build;
}
