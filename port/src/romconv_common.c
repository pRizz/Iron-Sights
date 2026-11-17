/**
 * The converters' shared helpers (port_romconv.h): ROM byte order, the
 * scratch buffer a converter copies its input into, the map of where each
 * record moved, and the refusal line.
 *
 * Compiled only at 64 bits and in the 32-bit oracle build (CMakeLists.txt's
 * romconv_ rule), never into a shipped 32-bit executable.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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
        portConLogf("ge: lp64: converter scratch: cannot allocate %u bytes\n", size);
        exit(EXIT_FAILURE);
    }
    portLp64CheckRegion("converter scratch", grown, size);
    s_Scratch = grown;
    s_ScratchSize = size;
    return s_Scratch;
}

void portConvScratchReserve(u32 bytes)
{
    (void) portConvScratch(bytes);
}

/* ==== the offset map =================================================== */

int portConvMapInit(struct PortConvMap *map, u32 filesize)
{
    u32 slots = filesize / 4 + (filesize % 4 != 0);

    map->slots = 0;
    map->newOffset = (u32 *) malloc((size_t) (slots == 0 ? 1 : slots) * sizeof(u32));
    map->present = (u8 *) calloc(slots == 0 ? 1 : slots, 1);
    if (map->newOffset == NULL || map->present == NULL)
    {
        portConvMapFree(map);
        return 0;
    }
    map->slots = slots;
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
    free(map->newOffset);
    free(map->present);
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
