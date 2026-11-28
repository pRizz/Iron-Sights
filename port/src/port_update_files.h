#ifndef PORT_UPDATE_FILES_H
#define PORT_UPDATE_FILES_H
/* The in-game updater's file primitives (port_update_files.c), shared by
 * port_update.c and the ROM-free harness. */
#include <stddef.h>

#define PORT_UPDATE_PATH 1200

int portUpdateExists(const char *path);
int portUpdateIsExecutable(const char *path);
int portUpdateMarkExecutable(const char *path);
/* The whole file, NUL-terminated, if it is at most limit bytes; free() it. */
void *portUpdateReadFile(const char *path, size_t limit, size_t *size);
/* Writes path.part, then renames it over path. */
int portUpdateWriteFile(const char *path, const void *data, size_t size);
int portUpdateJoin(char *out, size_t size, const char *a, const char *b);
int portUpdateMakeDir(const char *path);
/* Every folder above path's last separator. */
int portUpdateMakeParents(const char *path);
/* Fails when to exists (Windows) or replaces it (POSIX): callers clear it. */
int portUpdateRename(const char *from, const char *to);
/* Replaces to on every platform. */
int portUpdateReplace(const char *from, const char *to);
/* 1 when path is gone afterwards. */
int portUpdateRemove(const char *path);
void portUpdateRemoveDir(const char *path);
int portUpdateFolderWritable(const char *dir);
/* One install at a time: 0 when another process holds it. */
int portUpdateLock(const char *path);
void portUpdateUnlock(void);
/* The swap's log: a header line, then one "WORD path" line each, flushed. */
int portUpdateJournalOpen(const char *path, const char *header);
int portUpdateJournalLine(const char *word, const char *path);
int portUpdateJournalClose(void);

/* ---- port_update_swap.c: state, swap, undo ------------------------------ */

struct PortUpdateEnv;

/* Data/Updates/ and the files in it. */
struct PortUpdatePaths {
    char dir[PORT_UPDATE_PATH];
    char state[PORT_UPDATE_PATH];
    char journal[PORT_UPDATE_PATH];
    char attempts[PORT_UPDATE_PATH];
    char lock[PORT_UPDATE_PATH];
    char cleanup[PORT_UPDATE_PATH];
};

int portUpdatePaths(const struct PortUpdateEnv *env, struct PortUpdatePaths *paths);

struct PortUpdateState {
    long long last_check;  /* when the last check finished, 0 = never */
    long newest;           /* the newest build a check saw */
    long bad_build;        /* a build rolled back; never offered again */
    char result[160];      /* what the last check or install did */
};

void portUpdateStateRead(const char *path, struct PortUpdateState *state);
int portUpdateStateWrite(const char *path, const struct PortUpdateState *state);

/* Moves each file the new package lists (newSums, plus SHA256SUMS.txt) from
 * staged/ into exe_dir, renaming the file it replaces to NAME.old first, and
 * moves aside the files only oldSums lists. Journals every step; undoes them
 * all and returns 0 with msg on a failure. */
int portUpdateSwapPackage(const struct PortUpdateEnv *env, const struct PortUpdatePaths *paths,
                          const char *staged, const char *newSums, const char *oldSums, const char *tag,
                          long build, char *msg, size_t msgsize);
/* The same for one file (an AppImage): staged replaces target. */
int portUpdateSwapFile(const struct PortUpdatePaths *paths, const char *staged, const char *target,
                       const char *tag, long build, char *msg, size_t msgsize);

#endif
