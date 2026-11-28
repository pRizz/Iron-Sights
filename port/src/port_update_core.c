/* The in-game updater's parsing (port_update.h): settings, flavors, release
 * build-info, tags, paths and checksum lists. No files and no network, so the
 * ROM-free harness (scripts/port/test_update.sh) covers all of it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_update.h"

/* ASCII classes of our own: the game defines isdigit itself (src/str.c), so
 * <ctype.h>'s would collide with it in the Windows link. */
static int isDigit(int c)
{
    return c >= '0' && c <= '9';
}

static int isHexDigit(int c)
{
    return isDigit(c) || (c >= 'a' && c <= 'f');
}

static int isWordChar(int c)
{
    return isDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* scripts/port/package_identity.py's FLAVORS, with each archive's folder. */
static const struct PortUpdateFlavor kFlavors[] = {
    { "windows-x86-zip", "Iron-Sights-windows-x86.zip", "SHA256SUMS.txt", "build-info.json",
      PORT_UPDATE_KIND_ZIP, "" },
    { "windows-x64-zip", "Iron-Sights-windows-x64.zip", "SHA256SUMS-windows-x64.txt",
      "windows-x64-build-info.json", PORT_UPDATE_KIND_ZIP, "" },
    { "linux-i686-tar", "Iron-Sights-linux-i686.tar.gz", "SHA256SUMS-linux.txt", "linux-build-info.json",
      PORT_UPDATE_KIND_TARGZ, "Iron-Sights-linux-i686" },
    { "linux-i686-appimage", "Iron-Sights-linux-i686.AppImage", "SHA256SUMS-linux.txt",
      "linux-build-info.json", PORT_UPDATE_KIND_APPIMAGE, "" },
    { "steamdeck-i686", "Iron-Sights-steamdeck-i686.zip", "SHA256SUMS-linux.txt", "linux-build-info.json",
      PORT_UPDATE_KIND_ZIP, "Iron-Sights-steamdeck-i686" },
    { "linux-x86_64-tar", "Iron-Sights-linux-x86_64.tar.gz", "SHA256SUMS-linux-x86_64.txt",
      "linux-x86_64-build-info.json", PORT_UPDATE_KIND_TARGZ, "Iron-Sights-linux-x86_64" },
    { "linux-x86_64-appimage", "Iron-Sights-linux-x86_64.AppImage", "SHA256SUMS-linux-x86_64.txt",
      "linux-x86_64-build-info.json", PORT_UPDATE_KIND_APPIMAGE, "" },
    { "steamdeck-x86_64", "Iron-Sights-steamdeck-x86_64.zip", "SHA256SUMS-linux-x86_64.txt",
      "linux-x86_64-build-info.json", PORT_UPDATE_KIND_ZIP, "Iron-Sights-steamdeck-x86_64" },
};

const struct PortUpdateFlavor *portUpdateFlavorAt(size_t index)
{
    return index < sizeof(kFlavors) / sizeof(kFlavors[0]) ? &kFlavors[index] : NULL;
}

const struct PortUpdateFlavor *portUpdateFlavorNamed(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof(kFlavors) / sizeof(kFlavors[0]); i++) {
        if (strcmp(kFlavors[i].name, name) == 0) {
            return &kFlavors[i];
        }
    }
    return NULL;
}

/* The next line of text as [start, end) without its line break; NULL at the
 * end. */
static const char *nextLine(const char *at, const char **end)
{
    const char *stop;

    if (at == NULL || *at == '\0') {
        return NULL;
    }
    stop = at;
    while (*stop != '\0' && *stop != '\n') {
        stop++;
    }
    *end = stop;
    return at;
}

static const char *afterLine(const char *end)
{
    return *end == '\n' ? end + 1 : end;
}

/* [start, end) without spaces, tabs and a carriage return at either side. */
static void trim(const char **start, const char **end)
{
    while (*start < *end && (**start == ' ' || **start == '\t')) {
        (*start)++;
    }
    while (*end > *start && ((*end)[-1] == ' ' || (*end)[-1] == '\t' || (*end)[-1] == '\r')) {
        (*end)--;
    }
}

static int copySpan(const char *start, const char *end, char *out, size_t size)
{
    const size_t n = (size_t) (end - start);

    if (size == 0 || n >= size) {
        return 0;
    }
    memcpy(out, start, n);
    out[n] = '\0';
    return 1;
}

static int spanIs(const char *start, const char *end, const char *word, int anyCase)
{
    const size_t n = strlen(word);
    size_t i;

    if ((size_t) (end - start) != n) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        const int a = (unsigned char) start[i], b = (unsigned char) word[i];
        if (anyCase ? lower(a) != lower(b) : a != b) {
            return 0;
        }
    }
    return 1;
}

int portUpdateStartsWith(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

int portUpdateKeyValue(const char *text, const char *key, char *out, size_t size)
{
    const char *line, *end;

    for (line = nextLine(text, &end); line != NULL; line = nextLine(afterLine(end), &end)) {
        const char *eq = memchr(line, '=', (size_t) (end - line));
        const char *keyEnd = eq, *value, *valueEnd = end;
        if (eq == NULL) {
            continue;
        }
        trim(&line, &keyEnd);
        if (!spanIs(line, keyEnd, key, 0)) {
            continue;
        }
        value = eq + 1;
        trim(&value, &valueEnd);
        return copySpan(value, valueEnd, out, size);
    }
    return 0;
}

int portUpdateModeFromIni(const char *ini)
{
    const char *line, *end;
    int inSection = 0;

    for (line = nextLine(ini, &end); line != NULL; line = nextLine(afterLine(end), &end)) {
        const char *start = line, *stop = end, *eq, *keyEnd, *value;
        trim(&start, &stop);
        if (start < stop && *start == '[') {
            inSection = spanIs(start, stop, "[updates]", 1);
            continue;
        }
        eq = inSection ? memchr(start, '=', (size_t) (stop - start)) : NULL;
        if (eq == NULL) {
            continue;
        }
        keyEnd = eq;
        value = eq + 1;
        trim(&start, &keyEnd);
        trim(&value, &stop);
        if (!spanIs(start, keyEnd, "mode", 1)) {
            continue;
        }
        if (spanIs(value, stop, "off", 1)) return PORT_UPDATE_OFF;
        if (spanIs(value, stop, "notify", 1)) return PORT_UPDATE_NOTIFY;
        if (spanIs(value, stop, "automatic", 1)) return PORT_UPDATE_AUTOMATIC;
        return PORT_UPDATE_DEFAULT_MODE;
    }
    return PORT_UPDATE_DEFAULT_MODE;
}

int portUpdateCheckDue(long long now, long long last)
{
    return last <= 0 || now < last || now - last >= PORT_UPDATE_CHECK_INTERVAL;
}

const struct PortUpdateFlavor *portUpdateFlavorFromPackage(const char *text)
{
    char schema[8], flavor[64];

    if (text == NULL || !portUpdateKeyValue(text, "schema", schema, sizeof(schema)) ||
        strcmp(schema, "1") != 0 || !portUpdateKeyValue(text, "flavor", flavor, sizeof(flavor))) {
        return NULL;
    }
    return portUpdateFlavorNamed(flavor);
}

int portUpdateTagValid(const char *tag)
{
    const char *at = tag;
    int digits, hex;

    if (!portUpdateStartsWith(at, "build-")) {
        return 0;
    }
    at += 6;
    for (digits = 0; isDigit(*at); at++) digits++;
    if (digits == 0 || digits > 9 || *at++ != '.') {
        return 0;
    }
    for (digits = 0; isDigit(*at); at++) digits++;
    if (digits == 0 || digits > 4 || *at++ != '-') {
        return 0;
    }
    for (hex = 0; isHexDigit(*at); at++) hex++;
    return hex == 12 && *at == '\0';
}

/* The value after "KEY": in a flat JSON object, as [start, end): a string's
 * contents without quotes, or a bare word or number. */
static int jsonValue(const char *json, const char *key, const char **start, const char **end)
{
    char quoted[64];
    const char *at;

    const int n = snprintf(quoted, sizeof(quoted), "\"%s\"", key);

    if (n < 0 || (size_t) n >= sizeof(quoted)) {
        return 0;
    }
    at = strstr(json, quoted);
    if (at == NULL) {
        return 0;
    }
    at += strlen(quoted);
    while (*at == ' ' || *at == '\t' || *at == '\r' || *at == '\n') at++;
    if (*at++ != ':') {
        return 0;
    }
    while (*at == ' ' || *at == '\t' || *at == '\r' || *at == '\n') at++;
    if (*at == '"') {
        *start = ++at;
        while (*at != '\0' && *at != '"' && *at != '\\') at++;
        if (*at != '"') {
            return 0;
        }
        *end = at;
        return 1;
    }
    *start = at;
    while (isWordChar(*at) || *at == '-' || *at == '.') at++;
    *end = at;
    return *end > *start;
}

int portUpdateParseBuildInfo(const char *json, const char *downloads_url, struct PortUpdateRelease *out)
{
    const char *start, *end;
    char number[16], prefix[256];
    size_t prefixLength;

    memset(out, 0, sizeof(*out));
    if (!jsonValue(json, "build_number", &start, &end) || !copySpan(start, end, number, sizeof(number))) {
        return 0;
    }
    out->build = strtol(number, NULL, 10);
    if (!jsonValue(json, "ci", &start, &end)) {
        return 0;
    }
    out->ci = spanIs(start, end, "true", 0);
    if (!jsonValue(json, "dirty", &start, &end)) {
        return 0;
    }
    out->dirty = !spanIs(start, end, "false", 0);
    if (!jsonValue(json, "release_url", &start, &end)) {
        return 0;
    }
    if (snprintf(prefix, sizeof(prefix), "%s/tag/", downloads_url) >= (int) sizeof(prefix)) {
        return 0;
    }
    prefixLength = strlen(prefix);
    if ((size_t) (end - start) <= prefixLength || !portUpdateStartsWith(start, prefix) ||
        !copySpan(start + prefixLength, end, out->tag, sizeof(out->tag))) {
        return 0;
    }
    return out->build > 0 && portUpdateTagValid(out->tag);
}

int portUpdatePathSafe(const char *path)
{
    const char *part = path, *at;

    if (*path == '\0' || *path == '/') {
        return 0;
    }
    for (at = path;; at++) {
        if (*at == '/' || *at == '\0') {
            const size_t n = (size_t) (at - part);
            if (n == 0 || (n == 1 && part[0] == '.') || (n == 2 && part[0] == '.' && part[1] == '.')) {
                return 0;
            }
            if (*at == '\0') {
                return 1;
            }
            part = at + 1;
        } else if (*at == '\\' || *at == ':' || (unsigned char) *at < 0x20) {
            return 0;
        }
    }
}

int portUpdatePackagePath(const char *path)
{
    if (!portUpdatePathSafe(path) || spanIs(path, path + strlen(path), "midas.ini", 1)) {
        return 0;
    }
    /* Windows and macOS folders ignore case, so DATA/Saves is Data/Saves. */
    if (strlen(path) < 5 || !spanIs(path, path + 5, "data/", 1)) {
        return 1;
    }
    return strcmp(path, "Data/README.txt") == 0 || strcmp(path, "Data/shaders_base.txt") == 0;
}

int portUpdateSumsEach(const char *sums, PortUpdateSumsFn fn, void *user)
{
    const char *line, *end;
    int count = 0;

    for (line = nextLine(sums, &end); line != NULL; line = nextLine(afterLine(end), &end)) {
        char hex[65], name[512];
        const char *nameEnd = end;
        int i;
        trim(&line, &nameEnd);
        if (line == nameEnd) {
            continue;
        }
        if (nameEnd - line < 67 || line[64] != ' ' || line[65] != ' ' ||
            !copySpan(line + 66, nameEnd, name, sizeof(name))) {
            return -1;
        }
        for (i = 0; i < 64; i++) {
            if (!isHexDigit(line[i])) {
                return -1;
            }
            hex[i] = line[i];
        }
        hex[64] = '\0';
        if (fn != NULL && !fn(user, name, hex)) {
            return -1;
        }
        count++;
    }
    return count;
}

struct Lookup {
    const char *name;
    char *hex;
    int found;
};

static int lookupOne(void *user, const char *name, const char *hex)
{
    struct Lookup *lookup = (struct Lookup *) user;

    if (strcmp(name, lookup->name) == 0) {
        memcpy(lookup->hex, hex, 65);
        lookup->found = 1;
    }
    return 1;
}

int portUpdateSumsLookup(const char *sums, const char *name, char hex[65])
{
    struct Lookup lookup;

    lookup.name = name;
    lookup.hex = hex;
    lookup.found = 0;
    return portUpdateSumsEach(sums, lookupOne, &lookup) >= 0 && lookup.found;
}
