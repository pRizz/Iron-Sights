/**
 * The setup file converter's references and table writers
 * (port_romconv_stage.h; LP64-004 Task 14). Every reference goes through one
 * of three lookups in the converter's PortConvMap, each of which refuses what
 * names nothing of the kind it must; a 0 is kept only where the game reads it
 * as "none" (a terminator record's field, an absent name table). Each writer
 * builds its records through the C types, so a record's size and field places
 * are the compiler's. Compiled only at 64 bits and in the 32-bit oracle build.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "bondconstants.h"
#include "chrai.h" /* chraiitemsize: the AI list walk */
#include "port_romconv.h"
#include "port_romlayout_stage.h"
#include "romconv_setup_walk.h"

/* The pad words are copied as one run of nine floats, and a bound pad is a
 * pad with its box after it. */
_Static_assert(offsetof(PadRecord, up) == 12 && offsetof(PadRecord, look) == 24 && sizeof(coord3d) == 12,
               "a pad's three vectors are nine floats from its start");
_Static_assert(offsetof(BoundPadRecord, plink) == offsetof(PadRecord, plink)
                   && offsetof(BoundPadRecord, stan) == offsetof(PadRecord, stan) && sizeof(bbox) == 24,
               "a bound pad is a pad, then six floats");

/* ==== the lookups ====================================================== */

static int lookup(const struct SetupWalk *w, u32 slotStart, const char *what, u32 where, u32 *to)
{
    if (!portConvMapGet(&w->map, slotStart, to))
    {
        setupRefuse(w, "the %s at 0x%X names 0x%X, which has no place in the converted file", what, where, slotStart);
        return 0;
    }
    return 1;
}

int portSetupRefTable(const struct SetupWalk *w, u32 value, u8 kind, const char *what, u32 where, u32 *to)
{
    if (value % 4 != 0 || value >= w->size || w->kinds[value / 4] != kind)
    {
        setupRefuse(w, "the %s at 0x%X names 0x%X, which is not the start of a %s", what, where, value,
                portSetupKindName(kind));
        return 0;
    }
    return lookup(w, value, what, where, to);
}

int portSetupRefList(const struct SetupWalk *w, u32 value, const char *what, u32 where, u32 *to)
{
    if (value % 4 != 0 || value >= w->size || w->kinds[value / 4] != SETUP_LIST)
    {
        setupRefuse(w, "the %s at 0x%X names 0x%X, which is not an s32 list", what, where, value);
        return 0;
    }
    return lookup(w, value, what, where, to);
}

/* The first AT in [FROM, RUN_END) holding BYTE, or RUN_END. */
static u32 byteBefore(const struct SetupWalk *w, u32 from, u32 run_end, u8 byte)
{
    u32 at;

    for (at = from; at < run_end && w->file[at] != byte; at++)
    {
    }
    return at;
}

/* Where the AI list at FROM ends, walked command by command as the
 * interpreter steps (chrai.c's chraiitemsize), or RUN_END when no AI_EndList
 * command starts before it: an AI_EndList byte that is another command's
 * argument does not end the list. AI_PRINT, a NUL-terminated string, is
 * sized here, so that its scan stays before RUN_END. */
static u32 aiListEnd(const struct SetupWalk *w, u32 from, u32 run_end)
{
    u32 at = from;

    while (at < run_end && w->file[at] != (u8) AI_EndList)
    {
        s32 size;

        if (w->file[at] == (u8) AI_PRINT)
        {
            at = byteBefore(w, at + 1, run_end, 0) + 1;
            continue;
        }
        size = chraiitemsize((u8 *) w->file, (s32) at);
        if (size < 1 || (u32) size >= run_end - at)
        {
            return run_end;
        }
        at += (u32) size;
    }
    return at < run_end ? at : run_end;
}

int portSetupRefBytes(const struct SetupWalk *w, u32 value, enum SetupBytes what, const char *field, u32 where,
        u32 *to)
{
    u32 slot, run_end, at, base;

    if (value >= w->size || w->kinds[value / 4] != SETUP_RAW)
    {
        setupRefuse(w, "the %s at 0x%X names 0x%X, which is %s", field, where, value,
                value >= w->size ? "outside the file" : "inside a table, not among the names and AI lists");
        return 0;
    }
    /* The unowned run VALUE is in ends at the next owned slot or the file's end. */
    for (slot = value / 4 + 1; slot < w->slots && w->kinds[slot] == SETUP_RAW; slot++)
    {
    }
    run_end = (u64) slot * 4 < w->size ? slot * 4 : w->size;
    at = what == SB_NAME ? byteBefore(w, value, run_end, 0) : aiListEnd(w, value, run_end);
    if (at == run_end)
    {
        setupRefuse(w, "the %s at 0x%X names 0x%X, whose %s is not before 0x%X, where its unowned bytes end", field,
                where, value, what == SB_NAME ? "NUL" : "AI_EndList command", run_end);
        return 0;
    }
    if (!lookup(w, value & ~3u, field, where, &base))
    {
        return 0;
    }
    *to = base + (value & 3u);
    return 1;
}

/* ==== the header ======================================================= */

static const u8 kFieldKinds[SF_COUNT] = {SETUP_WAYPOINTS, SETUP_WAYGROUPS, SETUP_INTRO, SETUP_PROPS, SETUP_PATHS,
    SETUP_AILISTS, SETUP_PADS, SETUP_BOUNDPADS, SETUP_PADNAMES, SETUP_BOUNDPADNAMES};

int portSetupWriteHeader(const struct SetupWalk *w, const struct SetupItem *item, u32 to)
{
    uintptr_t field[SF_COUNT];
    struct stagesetup s;
    u32 i, place;

    for (i = 0; i < SF_COUNT; i++)
    {
        field[i] = 0;
        if (w->header[i] == 0) /* only the two name tables: claiming refused a 0 elsewhere */
        {
            continue;
        }
        if (!portSetupRefTable(w, w->header[i], kFieldKinds[i], "header offset", item->at + i * 4, &place))
        {
            return 0;
        }
        field[i] = place;
    }
    if (w->out == NULL)
    {
        return 1;
    }
    memset(&s, 0, sizeof(s));
    s.pathwaypoints = (waypoint *) field[SF_WAYPOINTS];
    s.waypointgroups = (waygroup *) field[SF_WAYGROUPS];
    s.intro = (s32 *) field[SF_INTRO];
    s.propDefs = (PropDefHeaderRecord *) field[SF_PROPS];
    s.patrolpaths = (PathRecord *) field[SF_PATHS];
    s.ailists = (AIListRecord *) field[SF_AILISTS];
    s.pads = (PadRecord *) field[SF_PADS];
    s.boundpads = (BoundPadRecord *) field[SF_BOUNDPADS];
    s.padnames = (pname *) field[SF_PADNAMES];
    s.boundpadnames = (pname *) field[SF_BOUNDPADNAMES];
    memcpy(w->out + to, &s, sizeof(s));
    return 1;
}

/* ==== the tables ======================================================= */

/* N file words from AT, as native u32s (romfile_setup.c's K32), into DST. */
static void words(const struct SetupWalk *w, u32 at, u32 n, void *dst)
{
    u32 i, v;

    for (i = 0; i < n; i++)
    {
        v = setupBe32(w, at + i * 4);
        memcpy((u8 *) dst + i * 4, &v, 4);
    }
}

static int writePad(const struct SetupWalk *w, u32 from, u8 *dst, int bound)
{
    const u32 plink = setupBe32(w, from + offsetof(RomPad, plink));
    BoundPadRecord r;
    u32 name = 0;

    if (plink != 0 && !portSetupRefBytes(w, plink, SB_NAME, "pad name", from + offsetof(RomPad, plink), &name))
    {
        return 0;
    }
    memset(&r, 0, sizeof(r));
    words(w, from, 9, &r.pos);
    r.plink = (char *) (uintptr_t) name;
    r.stan = (StandTile *) (uintptr_t) setupBe32(w, from + offsetof(RomPad, stan));
    if (bound)
    {
        words(w, from + offsetof(RomBoundPad, bbox), 6, &r.bbox);
    }
    if (dst != NULL)
    {
        memcpy(dst, &r, bound ? sizeof(BoundPadRecord) : sizeof(PadRecord));
    }
    return 1;
}

static int writeWaypoint(const struct SetupWalk *w, u32 from, u8 *dst)
{
    const u32 list = setupBe32(w, from + offsetof(RomWaypoint, neighbours));
    waypoint r;
    u32 place = list; /* a terminator's word, kept as the in-place pass keeps it */

    memset(&r, 0, sizeof(r));
    r.padID = (s32) setupBe32(w, from + offsetof(RomWaypoint, padID));
    if (r.padID >= 0 && !portSetupRefList(w, list, "waypoint's neighbours", from + 4, &place))
    {
        return 0;
    }
    r.neighbours = (s32 *) (uintptr_t) place;
    r.groupNum = (s32) setupBe32(w, from + offsetof(RomWaypoint, groupNum));
    r.dist = (s32) setupBe32(w, from + offsetof(RomWaypoint, dist));
    if (dst != NULL)
    {
        memcpy(dst, &r, sizeof(r));
    }
    return 1;
}

static int writeWaygroup(const struct SetupWalk *w, u32 from, u8 *dst)
{
    const u32 neighbours = setupBe32(w, from + offsetof(RomWaygroup, neighbours));
    const u32 points = setupBe32(w, from + offsetof(RomWaygroup, waypoints));
    u32 n = 0, p = points; /* a terminator's waypoints word is kept */
    waygroup r;

    if (neighbours != 0
            && (!portSetupRefList(w, neighbours, "waygroup's neighbours", from, &n)
                || !portSetupRefList(w, points, "waygroup's waypoints", from + 4, &p)))
    {
        return 0;
    }
    memset(&r, 0, sizeof(r));
    r.neighbours = (s32 *) (uintptr_t) n;
    r.waypoints = (s32 *) (uintptr_t) p;
    r.dist = (s32) setupBe32(w, from + offsetof(RomWaygroup, dist));
    if (dst != NULL)
    {
        memcpy(dst, &r, sizeof(r));
    }
    return 1;
}

static int writePath(const struct SetupWalk *w, u32 from, u8 *dst)
{
    const u32 list = setupBe32(w, from + offsetof(RomPath, waypoints));
    PathRecord r;
    u32 place = 0;

    if (list != 0 && !portSetupRefList(w, list, "path's waypoints", from, &place))
    {
        return 0;
    }
    memset(&r, 0, sizeof(r));
    r.waypoints = (s32 *) (uintptr_t) place;
    r.ID = w->file[from + offsetof(RomPath, ID)];
    r.isLoop = w->file[from + offsetof(RomPath, isLoop)];
    r.len = setupBe16(w, from + offsetof(RomPath, len));
    if (dst != NULL)
    {
        memcpy(dst, &r, sizeof(r));
    }
    return 1;
}

static int writeAiList(const struct SetupWalk *w, u32 from, u8 *dst)
{
    const u32 code = setupBe32(w, from + offsetof(RomAiList, ailist));
    AIListRecord r;
    u32 place = 0;

    if (code != 0 && !portSetupRefBytes(w, code, SB_AILIST, "AI list's code", from, &place))
    {
        return 0;
    }
    memset(&r, 0, sizeof(r));
    r.ailist = (AIRecord *) (uintptr_t) place;
    r.ID = (int) setupBe32(w, from + offsetof(RomAiList, ID));
    if (dst != NULL)
    {
        memcpy(dst, &r, sizeof(r));
    }
    return 1;
}

static int writeName(const struct SetupWalk *w, u32 from, u8 *dst)
{
    const u32 value = setupBe32(w, from);
    pname r;
    u32 place = 0;

    if (value != 0 && !portSetupRefBytes(w, value, SB_NAME, "name table entry", from, &place))
    {
        return 0;
    }
    memset(&r, 0, sizeof(r));
    r.p = (char *) (uintptr_t) place;
    if (dst != NULL)
    {
        memcpy(dst, &r, sizeof(r));
    }
    return 1;
}

int portSetupWriteTable(const struct SetupWalk *w, const struct SetupItem *item, u32 to)
{
    const u8 kind = SETUP_KIND(item->kind);
    const u32 fileRecord = portSetupRecordFileSize(kind), outRecord = portSetupRecordOutSize(kind);
    const u32 count = item->bytes / fileRecord;
    u32 i;
    int ok = 1;

    for (i = 0; ok && i < count; i++)
    {
        const u32 from = item->at + i * fileRecord;
        u8 *dst = w->out == NULL ? NULL : w->out + to + i * outRecord;

        switch (kind)
        {
        case SETUP_PADS: ok = writePad(w, from, dst, 0); break;
        case SETUP_BOUNDPADS: ok = writePad(w, from, dst, 1); break;
        case SETUP_WAYPOINTS: ok = writeWaypoint(w, from, dst); break;
        case SETUP_WAYGROUPS: ok = writeWaygroup(w, from, dst); break;
        case SETUP_PATHS: ok = writePath(w, from, dst); break;
        case SETUP_AILISTS: ok = writeAiList(w, from, dst); break;
        default: ok = writeName(w, from, dst); break; /* the two name tables */
        }
    }
    return ok;
}

int portSetupWriteList(const struct SetupWalk *w, const struct SetupItem *item, u32 to)
{
    if (w->out != NULL)
    {
        words(w, item->at, item->bytes / 4, w->out + to);
    }
    return 1;
}
