/* Zip, gzip and tar readers for the in-game updater (port_update.h), over
 * puff (port/fast3d/third_party/puff). They read only what the release
 * packagers write: stored or deflated zip entries without encryption or
 * zip64, one gzip member, and ustar or pax tar headers. Anything else fails
 * rather than being guessed at; every file is then checked against the
 * package's SHA256SUMS.txt by the caller. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_update.h"
#include "../fast3d/third_party/puff/puff.h"

#define MAX_NAME 512

static int fail(char *err, size_t errsize, const char *what, const char *name)
{
    if (errsize > 0) {
        snprintf(err, errsize, "%s%s%s", what, name != NULL ? ": " : "", name != NULL ? name : "");
    }
    return 0;
}

static unsigned int le16(const unsigned char *p)
{
    return (unsigned int) p[0] | ((unsigned int) p[1] << 8);
}

static unsigned long le32(const unsigned char *p)
{
    return (unsigned long) p[0] | ((unsigned long) p[1] << 8) | ((unsigned long) p[2] << 16) |
           ((unsigned long) p[3] << 24);
}

/* Inflates SIZE bytes of raw deflate data into exactly OUTSIZE bytes. */
static int inflateExact(const unsigned char *in, size_t size, unsigned char *out, size_t outsize)
{
    unsigned long inLength, outLength;

    inLength = size;
    outLength = outsize;
    if (inLength != size || outLength != outsize) {
        return 0;  /* past unsigned long (a 32-bit long on Windows) */
    }
    return puff(out, &outLength, in, &inLength) == 0 && outLength == outsize;
}

/* ---- zip ---------------------------------------------------------------- */

static const unsigned char *findEndRecord(const unsigned char *zip, size_t size)
{
    size_t at;

    if (size < 22) {
        return NULL;
    }
    /* The record is at most a 64 KB comment from the end. */
    for (at = size - 22;; at--) {
        if (le32(zip + at) == 0x06054b50ul) {
            return zip + at;
        }
        if (at == 0 || size - at > 22 + 0xffff) {
            return NULL;
        }
    }
}

struct ZipEntry {
    char name[MAX_NAME];
    unsigned int method;
    size_t packed, unpacked, local;
    int directory, link, executable;
};

/* Reads the central directory header at *at into entry and moves past it. */
static int readCentral(const unsigned char *zip, size_t size, size_t *at, struct ZipEntry *entry, char *err,
                       size_t errsize)
{
    const unsigned char *h;
    unsigned int nameLength, mode;

    if (*at > size || size - *at < 46 || le32(zip + *at) != 0x02014b50ul) {
        return fail(err, errsize, "zip: bad central directory", NULL);
    }
    h = zip + *at;
    nameLength = le16(h + 28);
    if (size - *at - 46 < (size_t) nameLength + le16(h + 30) + le16(h + 32) || nameLength >= MAX_NAME) {
        return fail(err, errsize, "zip: bad central directory", NULL);
    }
    memcpy(entry->name, h + 46, nameLength);
    entry->name[nameLength] = '\0';
    if (le16(h + 8) & 1u) {
        return fail(err, errsize, "zip: encrypted entry", entry->name);
    }
    if (le32(h + 20) == 0xfffffffful || le32(h + 24) == 0xfffffffful || le32(h + 42) == 0xfffffffful) {
        return fail(err, errsize, "zip: zip64 entry", entry->name);
    }
    entry->method = le16(h + 10);
    entry->packed = le32(h + 20);
    entry->unpacked = le32(h + 24);
    entry->local = le32(h + 42);
    mode = (le16(h + 4) >> 8) == 3 ? (unsigned int) (le32(h + 38) >> 16) : 0;
    entry->directory = nameLength > 0 && entry->name[nameLength - 1] == '/';
    entry->link = (mode & 0170000u) == 0120000u;
    entry->executable = (mode & 0111u) != 0;
    *at += 46 + (size_t) nameLength + le16(h + 30) + le16(h + 32);
    return 1;
}

static int zipData(const unsigned char *zip, size_t size, const struct ZipEntry *entry, unsigned char **out,
                   char *err, size_t errsize)
{
    const unsigned char *h;
    size_t start;

    if (entry->local > size || size - entry->local < 30 || le32(zip + entry->local) != 0x04034b50ul) {
        return fail(err, errsize, "zip: bad local header", entry->name);
    }
    h = zip + entry->local;
    start = entry->local + 30 + le16(h + 26) + le16(h + 28);
    if (start > size || size - start < entry->packed) {
        return fail(err, errsize, "zip: entry runs past the end", entry->name);
    }
    *out = malloc(entry->unpacked > 0 ? entry->unpacked : 1);
    if (*out == NULL) {
        return fail(err, errsize, "zip: out of memory", entry->name);
    }
    if (entry->method == 0 && entry->packed == entry->unpacked) {
        memcpy(*out, zip + start, entry->unpacked);
        return 1;
    }
    if (entry->method == 8 && inflateExact(zip + start, entry->packed, *out, entry->unpacked)) {
        return 1;
    }
    free(*out);
    *out = NULL;
    return fail(err, errsize, "zip: cannot unpack", entry->name);
}

int portUpdateZipEach(const unsigned char *zip, size_t size, PortUpdateEntryFn fn, void *user, char *err,
                      size_t errsize)
{
    const unsigned char *end = findEndRecord(zip, size);
    size_t at, count, i;

    if (end == NULL) {
        return fail(err, errsize, "zip: no end record", NULL);
    }
    count = le16(end + 10);
    at = le32(end + 16);
    for (i = 0; i < count; i++) {
        struct ZipEntry entry;
        unsigned char *data = NULL;
        int keep;
        if (!readCentral(zip, size, &at, &entry, err, errsize)) {
            return 0;
        }
        if (entry.directory) {
            continue;
        }
        if (entry.link) {
            return fail(err, errsize, "zip: link entry", entry.name);
        }
        if (!zipData(zip, size, &entry, &data, err, errsize)) {
            return 0;
        }
        keep = fn(user, entry.name, data, entry.unpacked, entry.executable);
        free(data);
        if (!keep) {
            return fail(err, errsize, "refused entry", entry.name);
        }
    }
    return 1;
}

/* ---- gzip --------------------------------------------------------------- */

/* Moves *at past a zero-terminated field. */
static int skipString(const unsigned char *gz, size_t size, size_t *at)
{
    while (*at < size && gz[*at] != 0) (*at)++;
    if (*at >= size) {
        return 0;
    }
    (*at)++;
    return 1;
}

int portUpdateGunzip(const unsigned char *gz, size_t size, unsigned char **out, size_t *outsize, char *err,
                     size_t errsize)
{
    size_t at = 10, length;
    unsigned int flags;

    *out = NULL;
    *outsize = 0;
    if (size < 18 || gz[0] != 0x1f || gz[1] != 0x8b || gz[2] != 8) {
        return fail(err, errsize, "gzip: not a gzip file", NULL);
    }
    flags = gz[3];
    if (flags & 4u) {
        at += 2 + (size_t) le16(gz + 10);
    }
    if (((flags & 8u) && !skipString(gz, size, &at)) || ((flags & 16u) && !skipString(gz, size, &at))) {
        return fail(err, errsize, "gzip: bad header", NULL);
    }
    if (flags & 2u) {
        at += 2;
    }
    if (at + 8 > size) {
        return fail(err, errsize, "gzip: bad header", NULL);
    }
    length = le32(gz + size - 4);
    *out = malloc(length > 0 ? length : 1);
    if (*out == NULL) {
        return fail(err, errsize, "gzip: out of memory", NULL);
    }
    if (!inflateExact(gz + at, size - at - 8, *out, length)) {
        free(*out);
        *out = NULL;
        return fail(err, errsize, "gzip: cannot unpack", NULL);
    }
    *outsize = length;
    return 1;
}

/* ---- tar ---------------------------------------------------------------- */

static int octal(const unsigned char *field, size_t width, size_t *value)
{
    size_t i, v = 0;
    int seen = 0;

    for (i = 0; i < width && field[i] != 0 && field[i] != ' '; i++) {
        if (field[i] < '0' || field[i] > '7' || v > (((size_t) -1) >> 3)) {
            return 0;
        }
        v = (v << 3) | (size_t) (field[i] - '0');
        seen = 1;
    }
    *value = v;
    return seen;
}

/* The path= record of a pax extended header, into name. */
static int paxPath(const unsigned char *data, size_t size, char *name)
{
    size_t at = 0;

    while (at < size) {
        size_t length = 0, i = at;
        while (i < size && data[i] >= '0' && data[i] <= '9') length = length * 10 + (size_t) (data[i++] - '0');
        if (length == 0 || i >= size || data[i] != ' ' || at + length > size) {
            return 0;
        }
        i++;
        if (at + length - i > 5 && memcmp(data + i, "path=", 5) == 0) {
            const size_t n = at + length - i - 6;
            if (n == 0 || n >= MAX_NAME) {
                return 0;
            }
            memcpy(name, data + i + 5, n);
            name[n] = '\0';
        }
        at += length;
    }
    return 1;
}

/* The header's own name: ustar's prefix and name, or a pending long name. */
static void headerName(const unsigned char *h, char *pending, char *name)
{
    char part[101], prefix[156];

    if (pending[0] != '\0') {
        snprintf(name, MAX_NAME, "%s", pending);
        pending[0] = '\0';
        return;
    }
    memcpy(part, h, 100);
    part[100] = '\0';
    prefix[0] = '\0';
    if (memcmp(h + 257, "ustar", 5) == 0) {
        memcpy(prefix, h + 345, 155);
        prefix[155] = '\0';
    }
    snprintf(name, MAX_NAME, "%s%s%s", prefix, prefix[0] != '\0' ? "/" : "", part);
}

int portUpdateTarEach(const unsigned char *tar, size_t size, PortUpdateEntryFn fn, void *user, char *err,
                      size_t errsize)
{
    char pending[MAX_NAME] = "", name[MAX_NAME];
    size_t at = 0;

    while (at + 512 <= size) {
        const unsigned char *h = tar + at;
        size_t length, mode, blocks;
        const char type = (char) h[156];
        if (h[0] == 0) {
            return 1;  /* the end blocks */
        }
        if (!octal(h + 124, 12, &length) || !octal(h + 100, 8, &mode)) {
            return fail(err, errsize, "tar: bad header", NULL);
        }
        blocks = (length + 511) / 512 * 512;
        if (size - at - 512 < blocks) {
            return fail(err, errsize, "tar: entry runs past the end", NULL);
        }
        if (type == 'x' || type == 'L') {
            if (type == 'x' ? !paxPath(h + 512, length, pending)
                            : (length == 0 || length > MAX_NAME || h[512 + length - 1] != 0)) {
                return fail(err, errsize, "tar: bad long name", NULL);
            }
            if (type == 'L') {
                memcpy(pending, h + 512, length);
            }
        } else if (type == 'g' || type == '5') {
            headerName(h, pending, name);
        } else if (type == '0' || type == '\0') {
            headerName(h, pending, name);
            if (!fn(user, name, h + 512, length, (mode & 0111u) != 0)) {
                return fail(err, errsize, "refused entry", name);
            }
        } else {
            headerName(h, pending, name);
            return fail(err, errsize, "tar: link or device entry", name);
        }
        at += 512 + blocks;
    }
    return fail(err, errsize, "tar: no end blocks", NULL);
}
