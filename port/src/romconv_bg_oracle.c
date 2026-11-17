/**
 * The background oracle (port_romconv.h; LP64-004 Task 12), in the 32-bit
 * oracle build only (-DGE_LP64_ORACLE=ON).
 *
 * romfileBgConvert (romfile_bg.c) calls portBgTablesOracle when it has turned
 * a bg file's front matter to native order, which happens once a file, at its
 * first load, before load_bg_file's own passes: the room and portal tables
 * the 32-bit game then reads in place are compared byte for byte with
 * portBgTablesConvert's, and the file is kept, as it is at that moment, as its
 * pristine copy. At every later load of the file romfileBgConvert calls
 * portBgTablesReloadOracle instead, again before load_bg_file's passes, which
 * compares the tables in the file, field by field, with a fresh conversion of
 * that copy and logs each field that differs: what the 32-bit game's second
 * load sees of what the first load and its play wrote into the tables
 * (decision 6 of the plan; a 64-bit build builds them fresh at each load).
 *
 * portBgOracleSweep converts every bg file levelinfotable names that the ROM
 * serves with data, each in a copy of its own, through romfileBgConvert and so
 * through portBgTablesOracle.
 *
 * Its own file because port_lp64_oracle.c and romconv_bg.c are linked into
 * the ROM-free converter tests, which have no file loader. Compiled with the
 * other romconv_ sources, and empty outside the oracle build.
 */
#include <inttypes.h> /* PRIXPTR: the offsets print at full width */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "bg.h"
#include "ob.h"
#include "romdata.h"
#include "port_romconv.h"
#include "port_lp64_oracle.h"

#ifdef GE_LP64_ORACLE

/* port_console.c: the game's log (stderr plus the tilde console ring). */
extern void portConLogf(const char *fmt, ...);

extern s32 romfileBgConvert(u8 *data, u32 size, const char *name);
extern u8 *obGetBGFileData(u8 *bgname);
extern u32 obGetFileRomSize(s32 index);
extern struct levelentry levelinfotable[];

#define ORACLE_BG_FILES 64

/* Each file's pristine copy, by the address its tables are read at. */
struct OracleBgFile
{
    const u8 *data;
    u8 *copy;
    u32 size;
    u32 loads;
};

static struct OracleBgFile s_Files[ORACLE_BG_FILES];
static const bg_room_data kZeroRoom;
static u32 s_FileCount;
static int s_Sweeping;

/* The fresh tables of FILE (SIZE bytes) in new memory, with their counts;
 * NULL when the converter refuses it or memory cannot be had. */
static u8 *oracleConvert(const u8 *file, u32 size, const char *name, bg_room_data **rooms,
        bg_portal_data_entry **portals, u32 *nrooms, u32 *nportals)
{
    u32 n = portBgTablesSize(file, size, name);
    u8 *out = n != 0 ? (u8 *) malloc(n) : NULL;

    if (out == NULL || !portBgTablesConvert(file, size, out, n, rooms, portals))
    {
        free(out);
        return NULL;
    }
    *nrooms = (u32) ((u8 *) *portals - (u8 *) *rooms) / (u32) sizeof(bg_room_data);
    *nportals = (n - *nrooms * (u32) sizeof(bg_room_data)) / (u32) sizeof(bg_portal_data_entry);
    return out;
}

/* The first byte at which the SIZE bytes of A and B differ, or SIZE. */
static u32 oracleFirstDifference(const void *a, const void *b, u32 size)
{
    const u8 *x = (const u8 *) a;
    const u8 *y = (const u8 *) b;
    u32 i;

    for (i = 0; i < size && x[i] == y[i]; i++)
    {
    }
    return i;
}

static void oracleKeepPristine(const u8 *data, u32 size)
{
    struct OracleBgFile *f;

    if (s_Sweeping || s_FileCount >= ORACLE_BG_FILES)
    {
        return;
    }
    f = &s_Files[s_FileCount];
    f->copy = (u8 *) malloc(size);
    if (f->copy == NULL)
    {
        portConLogf("ge: lp64: oracle: bg tables: no memory for a pristine copy (%u bytes)\n", size);
        return;
    }
    memcpy(f->copy, data, size);
    f->data = data;
    f->size = size;
    f->loads = 1;
    s_FileCount++;
}

void portBgTablesOracle(const u8 *data, u32 size, const char *name)
{
    bg_room_data *rooms;
    bg_portal_data_entry *portals;
    u32 nrooms, nportals, at;
    u8 *out = oracleConvert(data, size, name, &rooms, &portals, &nrooms, &nportals);
    const u8 *inrooms, *inportals;
    char detail[224];

    oracleKeepPristine(data, size);
    if (out == NULL)
    {
        portLp64OracleResult(PORT_LP64_BACKGROUNDS, name, 0, "the converter refuses it");
        return;
    }
    /* The converter has checked both header offsets against the file. */
    inrooms = data + (((const u32 *) data)[1] & 0x00FFFFFFu);
    inportals = data + (((const u32 *) data)[2] & 0x00FFFFFFu);
    at = oracleFirstDifference(inrooms, rooms, nrooms * (u32) sizeof(bg_room_data));
    if (at < nrooms * sizeof(bg_room_data))
    {
        snprintf(detail, sizeof(detail), "room %u differs at +0x%X of its record", at / (u32) sizeof(bg_room_data),
                at % (u32) sizeof(bg_room_data));
    }
    else if ((at = oracleFirstDifference(inportals, portals, nportals * (u32) sizeof(bg_portal_data_entry)))
            < nportals * sizeof(bg_portal_data_entry))
    {
        snprintf(detail, sizeof(detail), "portal %u differs at +0x%X of its entry",
                at / (u32) sizeof(bg_portal_data_entry), at % (u32) sizeof(bg_portal_data_entry));
    }
    else
    {
        /* The game ends the room table at its first record after record 0
         * whose primary offset is 0 and reads nothing else of it; say what
         * the rest of that closing record holds. */
        const bg_room_data *closing = &rooms[nrooms - 1];
        u32 rest = oracleFirstDifference(closing, &kZeroRoom, sizeof(bg_room_data));

        snprintf(detail, sizeof(detail),
                "%u room records and %u portal entries, %u and %u bytes at 64 bits; closing record %s "
                "(point 0x%" PRIXPTR ", secondary 0x%" PRIXPTR ")", nrooms, nportals, nrooms * 40u, nportals * 16u,
                rest == sizeof(bg_room_data) ? "all zeros" : "not all zeros",
                (uintptr_t) closing->pPointTableBin, (uintptr_t) closing->pSecMappingBin);
        portLp64OracleResult(PORT_LP64_BACKGROUNDS, name, 1, detail);
        free(out);
        return;
    }
    portLp64OracleResult(PORT_LP64_BACKGROUNDS, name, 0, detail);
    free(out);
}

/* Logs one differing word of a reload: record INDEX's FIELD. */
static void oracleReloadField(const char *name, const char *kind, u32 index, const char *field, uintptr_t inplace,
        uintptr_t fresh, u32 *count)
{
    portConLogf("ge: lp64: oracle: bg reload %s: %s %u %s in-place 0x%08" PRIXPTR " fresh 0x%08" PRIXPTR "\n", name,
            kind, index, field, inplace, fresh);
    (*count)++;
}

static u32 oracleFloatBits(f32 value)
{
    u32 bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void oracleCompareRooms(const char *name, const bg_room_data *in, const bg_room_data *fresh, u32 nrooms,
        u32 *count)
{
    static const char *const kPos[3] = {"pos.x", "pos.y", "pos.z"};
    u32 i, k;

    for (i = 0; i < nrooms; i++)
    {
        if (in[i].pPointTableBin != fresh[i].pPointTableBin)
        {
            oracleReloadField(name, "room", i, "pPointTableBin", (uintptr_t) in[i].pPointTableBin,
                    (uintptr_t) fresh[i].pPointTableBin, count);
        }
        if (in[i].pPriMappingBin != fresh[i].pPriMappingBin)
        {
            oracleReloadField(name, "room", i, "pPriMappingBin", (uintptr_t) in[i].pPriMappingBin,
                    (uintptr_t) fresh[i].pPriMappingBin, count);
        }
        if (in[i].pSecMappingBin != fresh[i].pSecMappingBin)
        {
            oracleReloadField(name, "room", i, "pSecMappingBin", (uintptr_t) in[i].pSecMappingBin,
                    (uintptr_t) fresh[i].pSecMappingBin, count);
        }
        for (k = 0; k < 3; k++)
        {
            u32 a = oracleFloatBits(in[i].pos.f[k]), b = oracleFloatBits(fresh[i].pos.f[k]);

            if (a != b)
            {
                portConLogf("ge: lp64: oracle: bg reload %s: room %u %s in-place %g fresh %g\n", name, i, kPos[k],
                        (double) in[i].pos.f[k], (double) fresh[i].pos.f[k]);
                (*count)++;
            }
        }
    }
}

/* Portal offsets the first load promoted to addresses in the file are
 * counted, not logged one by one, when each is BASE plus its fresh offset. */
static void oracleComparePortals(const char *name, const u8 *base, const bg_portal_data_entry *in,
        const bg_portal_data_entry *fresh, u32 nportals, u32 *count)
{
    u32 i, promoted = 0;

    for (i = 0; i < nportals; i++)
    {
        const bg_portal_entry *expect = (const bg_portal_entry *) BG_SEG_TO_PTR(base, fresh[i].offset_portal);

        if (in[i].offset_portal != fresh[i].offset_portal)
        {
            if (fresh[i].offset_portal != NULL && in[i].offset_portal == expect)
            {
                promoted++;
            }
            else
            {
                oracleReloadField(name, "portal", i, "offset_portal", (uintptr_t) in[i].offset_portal,
                        (uintptr_t) fresh[i].offset_portal, count);
            }
        }
        if (in[i].connectedRoom1 != fresh[i].connectedRoom1)
        {
            oracleReloadField(name, "portal", i, "connectedRoom1", in[i].connectedRoom1, fresh[i].connectedRoom1,
                    count);
        }
        if (in[i].connectedRoom2 != fresh[i].connectedRoom2)
        {
            oracleReloadField(name, "portal", i, "connectedRoom2", in[i].connectedRoom2, fresh[i].connectedRoom2,
                    count);
        }
        if (in[i].controlbytes1 != fresh[i].controlbytes1)
        {
            oracleReloadField(name, "portal", i, "controlbytes1", in[i].controlbytes1, fresh[i].controlbytes1, count);
        }
        if (in[i].controlbytes2 != fresh[i].controlbytes2)
        {
            oracleReloadField(name, "portal", i, "controlbytes2", in[i].controlbytes2, fresh[i].controlbytes2, count);
        }
    }
    if (promoted != 0)
    {
        portConLogf("ge: lp64: oracle: bg reload %s: %u portal offsets are the first load's promoted addresses "
                    "(file + fresh offset)\n", name, promoted);
        *count += promoted;
    }
}

void portBgTablesReloadOracle(const u8 *data, const char *name)
{
    struct OracleBgFile *f = NULL;
    bg_room_data *rooms;
    bg_portal_data_entry *portals;
    u32 nrooms, nportals, i, count = 0;
    u8 *out;

    for (i = 0; i < s_FileCount; i++)
    {
        if (s_Files[i].data == data)
        {
            f = &s_Files[i];
        }
    }
    if (f == NULL)
    {
        portConLogf("ge: lp64: oracle: bg reload %s: no pristine copy\n", name);
        return;
    }
    f->loads++;
    out = oracleConvert(f->copy, f->size, name, &rooms, &portals, &nrooms, &nportals);
    if (out == NULL)
    {
        portConLogf("ge: lp64: oracle: bg reload %s: the pristine copy does not convert\n", name);
        return;
    }
    oracleCompareRooms(name, (const bg_room_data *) (data + (((const u32 *) data)[1] & 0x00FFFFFFu)), rooms, nrooms,
            &count);
    oracleComparePortals(name, data, (const bg_portal_data_entry *) (data + (((const u32 *) data)[2] & 0x00FFFFFFu)),
            portals, nportals, &count);
    portConLogf("ge: lp64: oracle: bg reload %s: load %u, %u fields differ from a fresh conversion\n", name, f->loads,
            count);
    free(out);
}

/* What the sweep does with bg file NAME, which levelinfotable names: 0 when
 * the ROM's directory lacks it or lists it with no bytes (a cut stage, which
 * romdata does not serve and a 64-bit game stops on by name in
 * portBgTablesLoad), else 1, with *DATA and *SIZE the served file, or -1,
 * counted as a difference, when the ROM holds bytes for it that the game
 * does not serve. */
static int oracleSweepFile(const char *name, const u8 **data, u32 *size)
{
    s32 r = romdataFileIndex(name);
    s32 index = fileGetIndex((u8 *) name);

    if (r < 0 || romdataFile(r)->size == 0)
    {
        return 0;
    }
    *size = (index > 0 && romdataFileServed(index)) ? obGetFileRomSize(index) : 0;
    *data = *size != 0 ? obGetBGFileData((u8 *) name) : NULL;
    if (*data == NULL)
    {
        portLp64OracleResult(PORT_LP64_BACKGROUNDS, name, 0, "the ROM holds it, but it is not served");
        return -1;
    }
    return 1;
}

/* The game's string.h has no strcmp. */
static int oracleSameName(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

void portBgOracleSweep(void)
{
    const char *seen[ORACLE_BG_FILES];
    u8 *copies[ORACLE_BG_FILES];
    u32 nseen = 0, ncopies = 0, absent = 0, i;
    struct levelentry *e;

    s_Sweeping = 1;
    for (e = levelinfotable; e->levelID != LEVELID_MAX; e++)
    {
        const char *name = (const char *) e->bg_seg_filename;
        const u8 *data;
        u32 size;
        int found;

        for (i = 0; i < nseen && !oracleSameName(seen[i], name); i++)
        {
        }
        if (i < nseen)
        {
            continue;
        }
        if (nseen >= ORACLE_BG_FILES)
        {
            portLp64OracleResult(PORT_LP64_BACKGROUNDS, name, 0, "the sweep holds no more files");
            continue;
        }
        seen[nseen++] = name;
        found = oracleSweepFile(name, &data, &size);
        if (found <= 0)
        {
            absent += found == 0;
            continue;
        }
        /* Each copy lives to the sweep's end: romfileBgConvert keys its
         * registry by address, so no two files may share one. */
        copies[ncopies] = (u8 *) malloc(size);
        if (copies[ncopies] == NULL)
        {
            portLp64OracleResult(PORT_LP64_BACKGROUNDS, name, 0, "no memory for a copy");
            continue;
        }
        memcpy(copies[ncopies], data, size);
        romfileBgConvert(copies[ncopies], size, name);
        ncopies++;
    }
    for (i = 0; i < ncopies; i++)
    {
        free(copies[i]);
    }
    s_Sweeping = 0;
    portConLogf("ge: lp64: oracle: backgrounds sweep: levelinfotable names %u files, %u not in the ROM or empty, "
            "%u swept\n", nseen, absent, ncopies);
}

#endif
