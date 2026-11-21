/**
 * The simulation trace (LP64-006 Task 3; port_simtrace.h has the line's
 * contract). Compiled where PORT_PLAY_HARNESS is 1; CMake leaves this file
 * out of a 32-bit build with video, the shipped kind.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_simtrace.h"

#if PORT_PLAY_HARNESS
/* Before the game's headers: PR/os.h undefines errno for its own fields. */
static const char *lastError(void)
{
    return strerror(errno);
}

#include <bondconstants.h>
#include <random.h>
#include "game/bg.h"
#include "game/bondview.h"
#include "game/chr.h"
#include "game/chrai.h"
#include "game/lv.h"
#include "game/player.h"

#define FNV_OFFSET 0x811c9dc5u
#define FNV_PRIME 0x01000193u
#define SIMTRACE_PLAYERS 4

extern s32 g_portRandDrawCount;

/* A count and a hash over the values of the records counted. */
typedef struct SimTraceSum
{
    u32 count;
    u32 hash;
} SimTraceSum;

static u32 floatBits(f32 value)
{
    u32 bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* FNV-1a over VALUE's four bytes, lowest first, whatever the host's order. */
static void sumValue(SimTraceSum *sum, u32 value)
{
    s32 i;

    for (i = 0; i < 4; i++)
    {
        sum->hash ^= (value >> (8 * i)) & 0xffu;
        sum->hash *= FNV_PRIME;
    }
}

static void sumPos(SimTraceSum *sum, const coord3d *pos)
{
    sumValue(sum, floatBits(pos->x));
    sumValue(sum, floatBits(pos->y));
    sumValue(sum, floatBits(pos->z));
}

static const coord3d *propPos(const PropRecord *prop)
{
    static const coord3d origin = { 0 };

    return prop != NULL ? &prop->pos : &origin;
}

/* Guard slots in slot order (chr.c allocates by index: chrFindFreeSlot). */
static SimTraceSum sumChrs(void)
{
    SimTraceSum sum = { 0, FNV_OFFSET };
    s32 slot;

    for (slot = 0; g_ChrSlots != NULL && slot < g_NumChrSlots; slot++)
    {
        const ChrRecord *chr = &g_ChrSlots[slot];

        if (chr->model == NULL)
        {
            continue;
        }
        sum.count++;
        sumValue(&sum, (u32) slot);
        sumValue(&sum, chr->actiontype);
        sumValue(&sum, (u32) chr->chrflags);
        sumPos(&sum, propPos(chr->prop));
        sumValue(&sum, floatBits(chr->damage));
    }
    return sum;
}

/* The active prop list in its own order (chrprop.c: chrpropActivate appends,
 * chrpropActivateThisFrame prepends, chrpropDelist unlinks). */
static SimTraceSum sumProps(void)
{
    SimTraceSum sum = { 0, FNV_OFFSET };
    const PropRecord *prop;

    for (prop = g_ActivePropsHead; prop != NULL && sum.count < MAX_PROPS; prop = prop->next)
    {
        sumValue(&sum, sum.count);
        sumValue(&sum, prop->type);
        sumValue(&sum, prop->flags);
        sumPos(&sum, &prop->pos);
        sum.count++;
    }
    return sum;
}

static s32 roomCount(void)
{
    return g_MaxNumRooms < MAXROOMCOUNT ? g_MaxNumRooms : MAXROOMCOUNT;
}

/* Loaded rooms in room order from 1, as bg.c's unload_rooms walks them. */
static SimTraceSum sumRooms(void)
{
    SimTraceSum sum = { 0, FNV_OFFSET };
    const s32 rooms = roomCount();
    s32 room;

    for (room = 1; room < rooms; room++)
    {
        if (g_BgRoomInfo[room].model_bin_loaded == 0)
        {
            continue;
        }
        sum.count++;
        sumValue(&sum, (u32) room);
        sumValue(&sum, g_BgRoomInfo[room].model_bin_loaded);
    }
    return sum;
}

/* Appends to OUT at *USED; returns 0 when it does not fit. */
static s32 append(char *out, size_t size, size_t *used, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

static s32 append(char *out, size_t size, size_t *used, const char *format, ...)
{
    va_list args;
    int wrote;

    if (*used >= size)
    {
        return 0;
    }
    va_start(args, format);
    wrote = vsnprintf(out + *used, size - *used, format, args);
    va_end(args);
    if (wrote < 0 || (size_t) wrote >= size - *used)
    {
        return 0;
    }
    *used += (size_t) wrote;
    return 1;
}

static s32 appendPlayers(char *out, size_t size, size_t *used)
{
    s32 count = 0;
    s32 i;
    s32 ok;

    for (i = 0; i < SIMTRACE_PLAYERS; i++)
    {
        count += g_playerPointers[i] != NULL;
    }
    ok = append(out, size, used, " players=%d", (int) count);
    for (i = 0; ok && i < SIMTRACE_PLAYERS; i++)
    {
        const struct player *player = g_playerPointers[i];
        const coord3d *pos;

        if (player == NULL)
        {
            continue;
        }
        pos = propPos(player->prop);
        ok = append(out, size, used, " p%d=%08x,%08x,%08x,%08x,%08x", (int) i, (unsigned) floatBits(pos->x),
                    (unsigned) floatBits(pos->y), (unsigned) floatBits(pos->z),
                    (unsigned) floatBits(player->vv_theta), (unsigned) floatBits(player->bondhealth));
    }
    return ok;
}

s32 portSimTraceFormat(char *out, size_t size, u32 n, s32 draws)
{
    const SimTraceSum chrs = sumChrs();
    const SimTraceSum props = sumProps();
    const SimTraceSum rooms = sumRooms();
    size_t used = 0;
    s32 ok;

    ok = append(out, size, &used, "T n=%u timer=%d seed=%016llx draws=%d", (unsigned) n, (int) g_GlobalTimer,
                (unsigned long long) g_randomSeed, (int) draws);
    ok = ok && appendPlayers(out, size, &used);
    ok = ok && append(out, size, &used, " chrs=%u:%08x props=%u:%08x rooms=%u:%08x\n", (unsigned) chrs.count,
                      (unsigned) chrs.hash, (unsigned) props.count, (unsigned) props.hash, (unsigned) rooms.count,
                      (unsigned) rooms.hash);
    return ok ? (s32) used : -1;
}

static FILE *sFile;
static u32 sCalls;
static s32 sLastDraws;
static u32 sDetailFirst = 1; /* first > last: no detail */
static u32 sDetailLast;
/* The call at which each g_Props slot was last on the active list, and its
 * position bits then, so a detail line can name the props that moved. */
static u32 sPropSeenAt[MAX_PROPS];
static u32 sPropPosBits[MAX_PROPS][3];

static void fail(const char *what, const char *path)
{
    fprintf(stderr, "ge: FATAL: GE_SIMTRACE %s %s: %s\n", what, path, lastError());
    fflush(NULL);
    exit(1);
}

static void readDetailRange(void)
{
    const char *e = getenv("GE_SIMTRACE_DETAIL");
    unsigned first;
    unsigned last;
    char extra;

    if (e == NULL || e[0] == '\0')
    {
        return;
    }
    if (sscanf(e, "%u-%u%c", &first, &last, &extra) != 2 || first > last)
    {
        fprintf(stderr, "ge: GE_SIMTRACE_DETAIL=%s ignored: it takes A-B, A at most B\n", e);
        return;
    }
    sDetailFirst = first;
    sDetailLast = last;
}

/* Opens GE_SIMTRACE's file once; returns NULL when the trace is off. */
static FILE *traceFile(void)
{
    static s32 read;
    const char *path;

    if (read)
    {
        return sFile;
    }
    read = 1;
    path = getenv("GE_SIMTRACE");
    if (path == NULL || path[0] == '\0')
    {
        return NULL;
    }
    sFile = fopen(path, "w");
    if (sFile == NULL)
    {
        fail("cannot open", path);
    }
    readDetailRange();
    return sFile;
}

static void writeLine(const char *line)
{
    if (fputs(line, sFile) == EOF)
    {
        fail("cannot write", getenv("GE_SIMTRACE"));
    }
}

static void writeGuards(u32 n)
{
    char line[PORT_SIMTRACE_LINE_MAX];
    s32 slot;

    for (slot = 0; g_ChrSlots != NULL && slot < g_NumChrSlots; slot++)
    {
        const ChrRecord *chr = &g_ChrSlots[slot];
        const coord3d *pos;

        if (chr->model == NULL)
        {
            continue;
        }
        pos = propPos(chr->prop);
        snprintf(line, sizeof(line), "G n=%u slot=%d chrnum=%d action=%u flags=%08x pos=%08x,%08x,%08x damage=%08x\n",
                 (unsigned) n, (int) slot, (int) chr->chrnum, (unsigned) chr->actiontype, (unsigned) chr->chrflags,
                 (unsigned) floatBits(pos->x), (unsigned) floatBits(pos->y), (unsigned) floatBits(pos->z),
                 (unsigned) floatBits(chr->damage));
        writeLine(line);
    }
}

/* Records where each active prop is, writing an M line for each that moved
 * when DETAIL is set. Runs every call, so call A's lines see call A-1. */
static void trackProps(u32 n, s32 detail)
{
    char line[PORT_SIMTRACE_LINE_MAX];
    const PropRecord *prop;
    u32 index;

    for (prop = g_ActivePropsHead, index = 0; prop != NULL && index < MAX_PROPS; prop = prop->next, index++)
    {
        const ptrdiff_t slot = prop - g_Props;
        const u32 bits[3] = { floatBits(prop->pos.x), floatBits(prop->pos.y), floatBits(prop->pos.z) };
        s32 moved;

        if (slot < 0 || slot >= MAX_PROPS)
        {
            continue; /* not a g_Props record: no slot to remember it by */
        }
        moved = sPropSeenAt[slot] == 0 || sPropSeenAt[slot] + 1 != n || memcmp(sPropPosBits[slot], bits, sizeof(bits)) != 0;
        sPropSeenAt[slot] = n;
        memcpy(sPropPosBits[slot], bits, sizeof(bits));
        if (!detail || !moved)
        {
            continue;
        }
        snprintf(line, sizeof(line), "M n=%u index=%u slot=%d type=%u flags=%02x pos=%08x,%08x,%08x\n", (unsigned) n,
                 (unsigned) index, (int) slot, (unsigned) prop->type, (unsigned) prop->flags, (unsigned) bits[0],
                 (unsigned) bits[1], (unsigned) bits[2]);
        writeLine(line);
    }
}

void portSimTraceTick(void)
{
    char line[PORT_SIMTRACE_LINE_MAX];
    s32 detail;

    if (traceFile() == NULL)
    {
        return;
    }
    sCalls++;
    if (portSimTraceFormat(line, sizeof(line), sCalls, g_portRandDrawCount - sLastDraws) < 0)
    {
        fprintf(stderr, "ge: FATAL: GE_SIMTRACE line %u is longer than %d bytes\n", (unsigned) sCalls,
                PORT_SIMTRACE_LINE_MAX);
        fflush(NULL);
        exit(1);
    }
    sLastDraws = g_portRandDrawCount;
    writeLine(line);
    detail = sCalls >= sDetailFirst && sCalls <= sDetailLast;
    if (detail)
    {
        writeGuards(sCalls);
    }
    trackProps(sCalls, detail);
    if (fflush(sFile) == EOF)
    {
        fail("cannot write", getenv("GE_SIMTRACE"));
    }
}
#endif
