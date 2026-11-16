/* ROM-free test of the external-texture slot (LP64-003 Task 6): the slot
 * loader in port/src/port_textures.c stores the whole pixel address in
 * sImageTableEntry.index, so a 64-bit pointer survives. Also texLoad's slot
 * type (Task 7): a stand-in declared with texLoad's own type (image.h) stores
 * a 64-bit value through it and the slot keeps every byte. Built at both pointer
 * widths by scripts/port/test_lp64_address.sh, which links port_textures.c
 * with the stubs below.
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

/* A 4x4 opaque 24-bit BMP, drawn here: no game data. */
static int writeTestBmp(void)
{
    enum { ROW = TEST_SIDE * 3, PIXELS = ROW * TEST_SIDE, SIZE = 54 + PIXELS };
    u8 file[SIZE];
    const char *dirs[] = { "Data", "Data/Textures", "Data/Textures/Base" };
    FILE *f;
    size_t i;

    for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
    {
        mkdir(portRootPath(dirs[i]), 0755);
    }
    memset(file, 0x80, sizeof(file));
    memset(file, 0, 54);
    file[0] = 'B';
    file[1] = 'M';
    putLe(file + 2, SIZE, 4);
    putLe(file + 10, 54, 4);
    putLe(file + 14, 40, 4);
    putLe(file + 18, TEST_SIDE, 4);
    putLe(file + 22, TEST_SIDE, 4);
    putLe(file + 26, 1, 2);
    putLe(file + 28, 24, 2);
    putLe(file + 34, PIXELS, 4);
    f = fopen(portRootPath("Data/Textures/Base/option.bmp"), "wb");
    if (f == NULL)
    {
        return 0;
    }
    i = fwrite(file, 1, sizeof(file), f);
    return fclose(f) == 0 && i == sizeof(file);
}

static void testSlotHoldsWholePixelAddress(void)
{
    // Arrange
    extern struct sImageTableEntry *portTexExternal(const char *name);
    struct sImageTableEntry *entry;

    expect(writeTestBmp(), "the test BMP is written");
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
    return s_failures == 0 ? 0 : 1;
}
