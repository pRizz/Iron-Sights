/**
 * The converters' shared helpers (port_romconv.h): ROM byte order, the
 * scratch buffer a converter copies its input into, the map of where each
 * record moved, and the refusal line.
 *
 * Compiled only at 64 bits and in the 32-bit oracle build (CMakeLists.txt's
 * romconv_ rule), never into a shipped 32-bit executable.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_romconv.h"

/* port_console.c: the game's log (stderr plus the tilde console ring). */
extern void portConLogf(const char *fmt, ...);

u16 portConvBe16(const u8 *p)
{
    return (u16) (((u32) p[0] << 8) | p[1]);
}

u32 portConvBe32(const u8 *p)
{
    return ((u32) p[0] << 24) | ((u32) p[1] << 16) | ((u32) p[2] << 8) | p[3];
}

/* ==== scratch ========================================================== */

static u8 *s_Scratch;
static u32 s_ScratchSize;

u8 *portConvScratch(u32 bytes)
{
    u32 size = bytes == 0 ? 1 : bytes;
    u8 *grown;

    if (size <= s_ScratchSize)
    {
        return s_Scratch;
    }
    /* The contents need not survive a request, so the old block goes first. */
    free(s_Scratch);
    s_Scratch = NULL;
    s_ScratchSize = 0;
    grown = (u8 *) malloc(size);
    if (grown == NULL)
    {
#if IS_64_BIT /* LP64-007 Task 6: the line in a box too; the oracle build has none */
        portLp64Stop(EXIT_FAILURE, "ge: lp64: converter scratch: cannot allocate %u bytes\n", size);
#else
        portConLogf("ge: lp64: converter scratch: cannot allocate %u bytes\n", size);
        exit(EXIT_FAILURE);
#endif
    }
    portLp64CheckRegion("converter scratch", grown, size);
    s_Scratch = grown;
    s_ScratchSize = size;
    return s_Scratch;
}

/* ==== the offset map ===================================================
 *
 * Every map uses one grow-only store, so that once the reserve has sized it
 * converting a file allocates nothing. Converters run one file at a time, so
 * only one map may be live; a second Init while one is refuses, by name.
 */

static u32 *s_MapOffsets;
static u8 *s_MapPresent;
static u32 s_MapCapacity; /* slots */
static int s_MapLive;

/* The slots a file of FILESIZE bytes needs, or 0 with *SLOTS untouched when
 * their arrays' byte sizes would not fit a size_t (only at 32 bits). */
static int convMapSlots(u32 filesize, u32 *slots)
{
    u32 needed = filesize / 4 + (filesize % 4 != 0);

#if SIZE_MAX / 4 < 0x40000000u /* a 4 GiB file's slots, in bytes, overflow */
    if ((size_t) needed > SIZE_MAX / sizeof(u32))
    {
        return 0;
    }
#endif
    *slots = needed;
    return 1;
}

static int convMapGrow(u32 slots)
{
    u32 *offsets;
    u8 *present;
    u32 size = slots == 0 ? 1 : slots;

    if (size <= s_MapCapacity)
    {
        return 1;
    }
    free(s_MapOffsets);
    free(s_MapPresent);
    s_MapOffsets = NULL;
    s_MapPresent = NULL;
    s_MapCapacity = 0;
    offsets = (u32 *) malloc((size_t) size * sizeof(u32));
    present = (u8 *) malloc(size);
    if (offsets == NULL || present == NULL)
    {
        free(offsets);
        free(present);
        return 0;
    }
    s_MapOffsets = offsets;
    s_MapPresent = present;
    s_MapCapacity = size;
    return 1;
}

void portConvScratchReserve(u32 bytes)
{
    u32 slots;

    (void) portConvScratch(bytes);
    if (!convMapSlots(bytes, &slots) || !convMapGrow(slots))
    {
#if IS_64_BIT
        portLp64Stop(EXIT_FAILURE, "ge: lp64: converter map: cannot reserve %u bytes\n", bytes);
#else
        portConLogf("ge: lp64: converter map: cannot reserve %u bytes\n", bytes);
        exit(EXIT_FAILURE);
#endif
    }
}

int portConvMapInit(struct PortConvMap *map, u32 filesize)
{
    u32 slots;

    map->newOffset = NULL;
    map->present = NULL;
    map->slots = 0;
    if (s_MapLive)
    {
        portConLogf("ge: lp64: converter map: a second map while one is in use\n");
        return 0;
    }
    if (!convMapSlots(filesize, &slots) || !convMapGrow(slots))
    {
        return 0;
    }
    memset(s_MapPresent, 0, slots);
    map->newOffset = s_MapOffsets;
    map->present = s_MapPresent;
    map->slots = slots;
    s_MapLive = 1;
    return 1;
}

/* The slot of OLD, or -1 when OLD has none (unaligned or past the end). */
static long convMapSlot(const struct PortConvMap *map, u32 old)
{
    if (old % 4 != 0 || old / 4 >= map->slots)
    {
        return -1;
    }
    return (long) (old / 4);
}

void portConvMapPut(struct PortConvMap *map, u32 old, u32 to)
{
    long slot = convMapSlot(map, old);

    if (slot < 0)
    {
        return;
    }
    map->newOffset[slot] = to;
    map->present[slot] = 1;
}

int portConvMapGet(const struct PortConvMap *map, u32 old, u32 *to)
{
    long slot = convMapSlot(map, old);

    if (slot < 0 || !map->present[slot])
    {
        return 0;
    }
    *to = map->newOffset[slot];
    return 1;
}

void portConvMapFree(struct PortConvMap *map)
{
    /* The store stays for the next map; only the live one gives it back. */
    if (map->present != NULL && map->present == s_MapPresent)
    {
        s_MapLive = 0;
    }
    map->newOffset = NULL;
    map->present = NULL;
    map->slots = 0;
}

/* ==== refusals ========================================================= */

void portConvRefuse(enum PortLp64Asset asset, const char *file, const char *why, ...)
{
    char reason[512];
    va_list ap;

    va_start(ap, why);
    vsnprintf(reason, sizeof(reason), why, ap);
    va_end(ap);
    portConLogf("ge: lp64: cannot convert %s \"%s\": %s\n", portLp64AssetName(asset), file, reason);
}
