/* The in-game updater's file primitives (port_update.h): whole-file reads and
 * writes, folders, renames, the install lock and the journal's line log. The
 * Windows calls take the same ANSI paths as the rest of the port (main.c's
 * root comes from GetModuleFileNameA). */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#include "port_update_files.h"

int portUpdateExists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0;
}

int portUpdateIsExecutable(const char *path)
{
#ifdef _WIN32
    (void) path;
    return 0;
#else
    struct stat st;

    return stat(path, &st) == 0 && (st.st_mode & 0111) != 0;
#endif
}

int portUpdateMarkExecutable(const char *path)
{
#ifdef _WIN32
    (void) path;
    return 1;
#else
    return chmod(path, 0755) == 0;
#endif
}

void *portUpdateReadFile(const char *path, size_t limit, size_t *size)
{
    FILE *f = fopen(path, "rb");
    char *data = NULL;
    size_t used = 0, capacity = 0;

    *size = 0;
    if (f == NULL) {
        return NULL;
    }
    for (;;) {
        size_t got;
        if (used == capacity) {
            char *grown;
            capacity = capacity == 0 ? 65536 : capacity * 2;
            if (capacity > limit + 1) {
                capacity = limit + 1;
            }
            if (used == capacity || (grown = realloc(data, capacity + 1)) == NULL) {
                free(data);
                fclose(f);
                return NULL;
            }
            data = grown;
        }
        got = fread(data + used, 1, capacity - used, f);
        used += got;
        if (got == 0) {
            break;
        }
    }
    fclose(f);
    if (used > limit) {
        free(data);
        return NULL;
    }
    data[used] = '\0';
    *size = used;
    return data;
}

int portUpdateWriteFile(const char *path, const void *data, size_t size)
{
    char temporary[PORT_UPDATE_PATH];
    FILE *f;
    int ok;

    if (!portUpdateJoin(temporary, sizeof(temporary), path, ".part")) {
        return 0;
    }
    f = fopen(temporary, "wb");
    if (f == NULL) {
        return 0;
    }
    ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        remove(temporary);
        return 0;
    }
    return portUpdateReplace(temporary, path);
}

int portUpdateJoin(char *out, size_t size, const char *a, const char *b)
{
    const int n = snprintf(out, size, "%s%s", a, b);

    return n >= 0 && (size_t) n < size;
}

int portUpdateMakeDir(const char *path)
{
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

int portUpdateMakeParents(const char *path)
{
    char dir[PORT_UPDATE_PATH];
    size_t i;

    if (!portUpdateJoin(dir, sizeof(dir), path, "")) {
        return 0;
    }
    for (i = 1; dir[i] != '\0'; i++) {
        if (dir[i] == '/' || dir[i] == '\\') {
            const char sep = dir[i];
            dir[i] = '\0';
            if (!(i == 2 && dir[1] == ':') && !portUpdateMakeDir(dir)) {
                return 0;
            }
            dir[i] = sep;
        }
    }
    return 1;
}

int portUpdateRename(const char *from, const char *to)
{
    return rename(from, to) == 0;
}

int portUpdateReplace(const char *from, const char *to)
{
#ifdef _WIN32
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return rename(from, to) == 0;
#endif
}

int portUpdateRemove(const char *path)
{
    return remove(path) == 0 || !portUpdateExists(path);
}

void portUpdateRemoveDir(const char *path)
{
#ifdef _WIN32
    _rmdir(path);
#else
    rmdir(path);
#endif
}

int portUpdateFolderWritable(const char *dir)
{
    char probe[PORT_UPDATE_PATH];
    FILE *f;

    if (!portUpdateJoin(probe, sizeof(probe), dir, ".update-write-test")) {
        return 0;
    }
    f = fopen(probe, "wb");
    if (f == NULL) {
        return 0;
    }
    fclose(f);
    remove(probe);
    return 1;
}

/* ---- the lock ----------------------------------------------------------- */

#ifdef _WIN32
static HANDLE sLock = INVALID_HANDLE_VALUE;
#else
static int sLock = -1;
#endif

int portUpdateLock(const char *path)
{
#ifdef _WIN32
    sLock = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    return sLock != INVALID_HANDLE_VALUE;
#else
    sLock = open(path, O_RDWR | O_CREAT, 0644);
    if (sLock < 0) {
        return 0;
    }
    if (flock(sLock, LOCK_EX | LOCK_NB) != 0) {
        close(sLock);
        sLock = -1;
        return 0;
    }
    return 1;
#endif
}

void portUpdateUnlock(void)
{
#ifdef _WIN32
    if (sLock != INVALID_HANDLE_VALUE) {
        CloseHandle(sLock);
        sLock = INVALID_HANDLE_VALUE;
    }
#else
    if (sLock >= 0) {
        close(sLock);
        sLock = -1;
    }
#endif
}

/* ---- the journal -------------------------------------------------------- */

static FILE *sJournal;

int portUpdateJournalOpen(const char *path, const char *header)
{
    sJournal = fopen(path, "wb");
    if (sJournal == NULL) {
        return 0;
    }
    return portUpdateJournalLine(header, "");
}

int portUpdateJournalLine(const char *word, const char *path)
{
    if (sJournal == NULL) {
        return 0;
    }
    if (fprintf(sJournal, "%s%s%s\n", word, path[0] != '\0' ? " " : "", path) < 0 || fflush(sJournal) != 0) {
        return 0;
    }
#ifndef _WIN32
    fsync(fileno(sJournal));
#endif
    return 1;
}

int portUpdateJournalClose(void)
{
    int ok;

    if (sJournal == NULL) {
        return 0;
    }
    ok = fclose(sJournal) == 0;
    sJournal = NULL;
    return ok;
}
