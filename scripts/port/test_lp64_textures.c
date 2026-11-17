/* ROM-free test of the external-texture slot (LP64-003 Task 6): the slot
 * loader in port/src/port_textures.c stores the whole pixel address in
 * sImageTableEntry.index, so a 64-bit pointer survives. Also texLoad's slot
 * type (Task 7): a stand-in declared with texLoad's own type (image.h) stores
 * a 64-bit value through it and the slot keeps every byte. Built at both pointer
 * widths by scripts/port/test_lp64_address.sh, which links port_textures.c
 * with the stubs below. And portTexReplaceRom (LP64-004 Task 8): a ROM
 * texture with a t<num>.bmp is rewritten where its record points.
 *
 *   test_lp64_textures DIR   DIR is an empty scratch directory, used as the
 *                            game's root; the driver writes
 *                            Data/Textures/Base/option.bmp under it */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <ultra64.h>
#include <bondtypes.h>
#include "game/image.h"
#include "platform_info.h"
#include "port_hostptr.h"

#define TEST_SIDE 4
#define TEST_HEADER 16 /* port_textures.c's PORT_TEX_HEADER */

static const char *s_root;
static char s_path[1024];
/* The slot's pixels come from here: a static array, so in a position-
 * independent 64-bit executable its address is above 4 GB. */
static u8 s_arena[4096] __attribute__((aligned(16)));
static u8 *s_lastAlloc;
static int s_failures;

struct sImageTableEntry *crosshairimage;

const char *portRootPath(const char *name)
{
    snprintf(s_path, sizeof(s_path), "%s/%s", s_root, name);
    return s_path;
}

void portConLogf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void *portAllocHigh(u32 size)
{
    if (s_lastAlloc != NULL || size > sizeof(s_arena))
    {
        return NULL;
    }
    s_lastAlloc = s_arena;
    return s_arena;
}

/* Reached only by the ROM-texture and crosshair paths, which this test does
 * not drive. */
s32 portTexLodLookup(s32 texturenum, s32 lod, s32 wantheight)
{
    (void) texturenum;
    (void) lod;
    (void) wantheight;
    return -1;
}

struct tex *texFindInPool(s32 texturenum, struct texpool *pool)
{
    (void) texturenum;
    (void) pool;
    return NULL;
}

struct texpool *portModelTexPool(void)
{
    return NULL;
}

void gfx_texture_cache_clear_c(void)
{
}

s32 portCrosshairColor(void)
{
    return -1;
}

static void expect(int condition, const char *what)
{
    if (!condition)
    {
        printf("FAIL: %s\n", what);
        s_failures++;
    }
}

static void putLe(u8 *p, u32 v, int bytes)
{
    int i;

    for (i = 0; i < bytes; i++)
    {
        p[i] = (u8) (v >> (8 * i));
    }
}

/* A 4x4 opaque grey 24-bit BMP named FILE under Data/Textures/Base, drawn
 * here: no game data. */
static int writeTestBmp(const char *file)
{
    enum { ROW = TEST_SIDE * 3, PIXELS = ROW * TEST_SIDE, SIZE = 54 + PIXELS };
    u8 bmp[SIZE];
    char path[64];
    const char *dirs[] = { "Data", "Data/Textures", "Data/Textures/Base" };
    FILE *f;
    size_t i;

    for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
    {
        mkdir(portRootPath(dirs[i]), 0755);
    }
    memset(bmp, 0x80, sizeof(bmp));
    memset(bmp, 0, 54);
    bmp[0] = 'B';
    bmp[1] = 'M';
    putLe(bmp + 2, SIZE, 4);
    putLe(bmp + 10, 54, 4);
    putLe(bmp + 14, 40, 4);
    putLe(bmp + 18, TEST_SIDE, 4);
    putLe(bmp + 22, TEST_SIDE, 4);
    putLe(bmp + 26, 1, 2);
    putLe(bmp + 28, 24, 2);
    putLe(bmp + 34, PIXELS, 4);
    snprintf(path, sizeof(path), "Data/Textures/Base/%s.bmp", file);
    f = fopen(portRootPath(path), "wb");
    if (f == NULL)
    {
        return 0;
    }
    i = fwrite(bmp, 1, sizeof(bmp), f);
    return fclose(f) == 0 && i == sizeof(bmp);
}

static void testSlotHoldsWholePixelAddress(void)
{
    // Arrange
    extern struct sImageTableEntry *portTexExternal(const char *name);
    struct sImageTableEntry *entry;

    expect(writeTestBmp("option"), "the test BMP is written");
#if IS_64_BIT
    expect(PORT_IS_HOST_ADDR(s_arena), "the slot's arena is above 4 GB (a position-independent executable)");
#endif

    // Act
    entry = portTexExternal("option");

    // Assert
    expect(entry != NULL && s_lastAlloc != NULL
           && entry->index == (uintptr_t) (s_lastAlloc + TEST_HEADER),
           "the external texture's index is the whole address of its pixels");
}

/* texLoad writes the texture's data address through its parameter. Declared
 * with texLoad's type, so this definition stops compiling if that parameter
 * stops being a whole pointer-sized slot. */
static __typeof__(texLoad) stubTexLoad;

static void stubTexLoad(uintptr_t *updateword, struct texpool *pool)
{
    (void) pool;
    *updateword = (uintptr_t) 0x0000AAAA12345678ull;
}

static void testTexLoadSlotKeepsWholeWord(void)
{
    // Arrange
    uintptr_t slot = 0;

    // Act
    stubTexLoad(&slot, NULL);

    // Assert
#if IS_64_BIT
    expect(slot == (uintptr_t) 0x0000AAAA12345678ull, "a texLoad slot keeps all 8 bytes of 0x0000AAAA12345678");
#else
    expect(slot == 0x12345678u, "a texLoad slot keeps the 4-byte word 0x12345678");
#endif
}

/* portTexReplaceRom (LP64-004 Task 8), reached from texLoad once the textures
 * class is converted: a ROM texture with a t<num>.bmp has its pool bytes
 * rewritten where its struct tex points, and the record keeps the whole
 * address of those bytes; one without a file is left alone. The pool is a
 * static array, above 4 GB at 64 bits. */
#define REPLACED_TEX 42
#define PLAIN_TEX 43
#define TEX_BYTES (TEST_SIDE * TEST_SIDE * 2) /* RGBA16, one level */

static u8 s_pool[TEX_BYTES] __attribute__((aligned(16)));

extern s32 portTexReplaceRom(struct tex *tex, u8 *data, s32 bytes);

/* A 4x4 RGBA16 record of texture NUM whose data is the zeroed pool. */
static struct tex poolTexture(s32 num)
{
    struct tex t;

    memset(&t, 0, sizeof(t));
    memset(s_pool, 0, sizeof(s_pool));
    t.texturenum = (u16) num;
    t.data = s_pool;
    t.width = TEST_SIDE;
    t.height = TEST_SIDE;
    t.gbiformat = G_IM_FMT_RGBA;
    t.depth = G_IM_SIZ_16b;
    return t;
}

static int poolIsZero(void)
{
    size_t i;

    for (i = 0; i < sizeof(s_pool); i++)
    {
        if (s_pool[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void testReplacementRewritesTheBytesTheRecordAddresses(void)
{
    // Arrange
    struct tex t = poolTexture(REPLACED_TEX);
    s32 replaced;

    expect(writeTestBmp("t00042"), "the replacement BMP is written");

    // Act
    replaced = portTexReplaceRom(&t, t.data, TEX_BYTES);

    // Assert
    expect(replaced == 1 && !poolIsZero(), "a registered replacement rewrites the bytes its record addresses");
}

static void testReplacementKeepsTheRecordsWholeAddress(void)
{
    // Arrange
    struct tex t = poolTexture(REPLACED_TEX);

    expect(writeTestBmp("t00042"), "the replacement BMP is written");
#if IS_64_BIT
    expect(PORT_IS_HOST_ADDR(s_pool), "the pool is above 4 GB (a position-independent executable)");
#endif

    // Act
    portTexReplaceRom(&t, t.data, TEX_BYTES);

    // Assert
    expect(t.data == s_pool && (uintptr_t) t.data == (uintptr_t) s_pool,
           "the replaced texture's record still holds the whole address of its bytes");
}

/* The game's string.h, first on the include path, has no memcmp. */
static int sameBytes(const void *a, const void *b, size_t n)
{
    const u8 *x = (const u8 *) a;
    const u8 *y = (const u8 *) b;
    size_t i;

    for (i = 0; i < n; i++)
    {
        if (x[i] != y[i])
        {
            return 0;
        }
    }
    return 1;
}

static void testTextureWithoutReplacementIsLeftAlone(void)
{
    // Arrange
    struct tex t = poolTexture(PLAIN_TEX);
    struct tex before = t;
    s32 replaced;

    // Act
    replaced = portTexReplaceRom(&t, t.data, TEX_BYTES);

    // Assert
    expect(replaced == 0 && poolIsZero() && sameBytes(&t, &before, sizeof(t)),
           "a texture without a file keeps its record and its bytes");
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: test_lp64_textures DIR\n");
        return 2;
    }
    s_root = argv[1];
    testSlotHoldsWholePixelAddress();
    testTexLoadSlotKeepsWholeWord();
    testReplacementRewritesTheBytesTheRecordAddresses();
    testReplacementKeepsTheRecordsWholeAddress();
    testTextureWithoutReplacementIsLeftAlone();
    return s_failures == 0 ? 0 : 1;
}
