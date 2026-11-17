/**
 * The 64-bit startup address check (port_lp64.h; the design's "Addresses").
 *
 * PORT_IS_HOST_ADDR tells a host pointer from a segment word or file offset by
 * its upper 32 bits, so a 64-bit build is only correct while nothing the game
 * addresses sits below 4 GB. A position-independent executable gets that from
 * the system (Linux and Apple's ARM platforms place image, heap, mappings and
 * stack above 4 GB); this file checks it at startup (the executable image, the
 * regions the allocators report, one heap block and one stack address) and
 * stops the game, named, when a check fails. Tools that load programs low (Valgrind) trip it; the
 * design accepts that.
 *
 * It also holds the asset-class guard (port_lp64.h): the table of classes that
 * have a 64-bit converter, and the stop for those that do not. And the pool
 * reports (port_lp64.h): the log line of a pool that ran dry, and the stop for
 * the two whose failure the game cannot survive. Nothing here runs at 32 bits,
 * except the class names in the 32-bit oracle build (-DGE_LP64_ORACLE=ON),
 * where the converters and the oracle print them.
 */
#define _GNU_SOURCE /* dl_iterate_phdr */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "port_lp64.h"

#if IS_64_BIT

#if defined(__linux__)
#include <link.h>
#else
#error "port_lp64.c: this platform's port must supply the main executable's loaded address range for portLp64CheckAddressSpace (Linux reads it from dl_iterate_phdr)"
#endif

/* port_console.c: the game's log (stderr plus the tilde console ring). */
extern void portConLogf(const char *fmt, ...);

#define LP64_FOUR_GB ((uintptr_t) 1 << 32)
#define LP64_EXIT_LOW_ADDRESS 4

/* Regions for the report. Only the report is bounded: a region past the
 * table's end is still checked, and counted. */
#define LP64_MAX_REGIONS 16

typedef struct Lp64Region
{
    const char *name;
    uintptr_t start;
} Lp64Region;

static Lp64Region s_Regions[LP64_MAX_REGIONS];
static int s_RegionCount;
static int s_RegionsUnlisted;

int portLp64RegionIsHigh(const void *start, size_t size)
{
    uintptr_t first = (uintptr_t) start;

    /* A region that wraps past the top of the address space is not "above". */
    return first >= LP64_FOUR_GB && size <= UINTPTR_MAX - first;
}

/* A later name for the same start wins: the ROM image is first a "high
 * allocation" (portAllocHighDedicated), then named by romdata.c. */
static void lp64Remember(const char *name, uintptr_t start)
{
    int i;

    for (i = 0; i < s_RegionCount; i++)
    {
        if (s_Regions[i].start == start)
        {
            s_Regions[i].name = name;
            return;
        }
    }
    if (s_RegionCount == LP64_MAX_REGIONS)
    {
        s_RegionsUnlisted++;
        return;
    }
    s_Regions[s_RegionCount].name = name;
    s_Regions[s_RegionCount].start = start;
    s_RegionCount++;
}

void portLp64CheckRegion(const char *name, const void *start, size_t size)
{
    if (!portLp64RegionIsHigh(start, size))
    {
        portConLogf("ge: lp64: %s at %p is below 4 GB; a 64-bit build needs every game address above 4 GB "
                    "(scripts/port/native_64bit_plan.md, \"Addresses\")\n",
                name, start);
        exit(LP64_EXIT_LOW_ADDRESS);
    }
    lp64Remember(name, (uintptr_t) start);
}

typedef struct Lp64ImageRange
{
    uintptr_t lowest;
    uintptr_t end;
    int found;
} Lp64ImageRange;

/* The first object dl_iterate_phdr reports is the main executable: take the
 * span of its PT_LOAD segments, then stop. */
static int lp64ReadImage(struct dl_phdr_info *info, size_t infosize, void *data)
{
    Lp64ImageRange *range = (Lp64ImageRange *) data;
    ElfW(Half) i;

    (void) infosize;
    for (i = 0; i < info->dlpi_phnum; i++)
    {
        const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
        uintptr_t start;

        if (ph->p_type != PT_LOAD)
        {
            continue;
        }
        start = (uintptr_t) info->dlpi_addr + (uintptr_t) ph->p_vaddr;
        if (!range->found || start < range->lowest)
        {
            range->lowest = start;
        }
        if (!range->found || start + (uintptr_t) ph->p_memsz > range->end)
        {
            range->end = start + (uintptr_t) ph->p_memsz;
        }
        range->found = 1;
    }
    return 1;
}

static void lp64CheckExecutableImage(void)
{
    Lp64ImageRange range = { 0, 0, 0 };

    dl_iterate_phdr(lp64ReadImage, &range);
    if (!range.found)
    {
        portConLogf("ge: lp64: the executable image has no loaded segments to check\n");
        exit(LP64_EXIT_LOW_ADDRESS);
    }
    portLp64CheckRegion("executable image", (const void *) range.lowest, (size_t) (range.end - range.lowest));
}

void portLp64CheckAddressSpace(void)
{
    char line[1024];
    size_t used;
    int i;
    void *block;
    volatile int stackProbe = 0;

    lp64CheckExecutableImage();

    block = malloc(64);
    if (block == NULL)
    {
        portConLogf("ge: lp64: malloc block for the address check could not be allocated\n");
        exit(LP64_EXIT_LOW_ADDRESS);
    }
    portLp64CheckRegion("malloc block", block, 64);
    free(block);

    portLp64CheckRegion("stack", (const void *) &stackProbe, sizeof(stackProbe));

    used = (size_t) snprintf(line, sizeof(line), "ge: lp64: address space OK:");
    for (i = 0; i < s_RegionCount && used < sizeof(line); i++)
    {
        used += (size_t) snprintf(line + used, sizeof(line) - used, "%s %s %p", i == 0 ? "" : ",",
                s_Regions[i].name, (void *) s_Regions[i].start);
    }
    if (s_RegionsUnlisted > 0 && used < sizeof(line))
    {
        snprintf(line + used, sizeof(line) - used, ", and %d more", s_RegionsUnlisted);
    }
    portConLogf("%s\n", line);
}

#endif

#if IS_64_BIT || defined(GE_LP64_ORACLE)

/* ==== the asset-class names ============================================ */

static const char *const s_AssetNames[PORT_LP64_ASSET_COUNT] = {
    "text banks",
    "fonts",
    "textures",
    "animation tables",
    "models",
    "collision tiles",
    "setup files",
    "backgrounds",
    "audio banks",
    "music sequences",
};

static int lp64AssetValid(enum PortLp64Asset asset)
{
    return (unsigned) asset < (unsigned) PORT_LP64_ASSET_COUNT;
}

const char *portLp64AssetName(enum PortLp64Asset asset)
{
    return lp64AssetValid(asset) ? s_AssetNames[asset] : "unknown asset class";
}

#endif

#if IS_64_BIT

/* ==== the asset-class guard ============================================ */

#define LP64_EXIT_NO_CONVERTER 5

/* 1 = the class has a 64-bit converter. All zero in LP64-003; LP64-004 sets
 * a class's entry in the commit that lands its converter. */
static const unsigned char s_AssetConverted[PORT_LP64_ASSET_COUNT] = {
    1, /* text banks (romconv_text.c) */
    1, /* fonts (romconv_font.c) */
    1, /* textures (romconv_gfx.c) */
    1, /* animation tables (romconv_anim.c) */
    0, /* models */
    0, /* collision tiles */
    0, /* setup files */
    0, /* backgrounds */
    1, /* audio banks (romconv_bank.c) */
    1, /* music sequences (romconv_seq.c) */
};

int portLp64AssetConverted(enum PortLp64Asset asset)
{
    return lp64AssetValid(asset) && s_AssetConverted[asset] != 0;
}

void portLp64RequireConverter(enum PortLp64Asset asset)
{
    if (portLp64AssetConverted(asset))
    {
        return;
    }
    portConLogf("ge: lp64: stopping: asset class \"%s\" has no 64-bit converter yet (LP64-004)\n",
            portLp64AssetName(asset));
    exit(LP64_EXIT_NO_CONVERTER);
}

/* ==== the pool reports ================================================= */

#define LP64_EXIT_POOL 6
#define LP64_POOL_LINES_PER_STAGE 8

/* Lines logged this stage, by pool name. Four pools report; a name past the
 * table's end is logged without a limit rather than not at all. */
#define LP64_MAX_POOLS 8

typedef struct Lp64PoolLines
{
    const char *pool;
    int lines;
} Lp64PoolLines;

static Lp64PoolLines s_PoolLines[LP64_MAX_POOLS];
static int s_PoolCount;

/* By text, since one name written in two files need not be one pointer. Not
 * strcmp: the game's <string.h> (include/) does not declare it. */
static int lp64SameName(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

static Lp64PoolLines *lp64PoolLines(const char *pool)
{
    int i;

    for (i = 0; i < s_PoolCount; i++)
    {
        if (lp64SameName(s_PoolLines[i].pool, pool))
        {
            return &s_PoolLines[i];
        }
    }
    if (s_PoolCount == LP64_MAX_POOLS)
    {
        return NULL;
    }
    s_PoolLines[s_PoolCount].pool = pool;
    s_PoolLines[s_PoolCount].lines = 0;
    return &s_PoolLines[s_PoolCount++];
}

static void lp64LogPool(const char *pool, u32 request)
{
    portConLogf("ge: lp64: pool \"%s\" exhausted (request %u bytes)\n", pool, (unsigned) request);
}

void portLp64PoolExhausted(const char *pool, u32 request)
{
    Lp64PoolLines *counted = lp64PoolLines(pool);

    if (counted != NULL)
    {
        if (counted->lines >= LP64_POOL_LINES_PER_STAGE)
        {
            return;
        }
        counted->lines++;
    }
    lp64LogPool(pool, request);
}

void portLp64PoolFatal(const char *pool, u32 request)
{
    lp64LogPool(pool, request);
    exit(LP64_EXIT_POOL);
}

void portLp64PoolStageBegin(void)
{
    s_PoolCount = 0;
}

#endif
