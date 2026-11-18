/* ROM-free check that the water animation writes the mip-map lists' prim
 * colour command by name at each pointer width (LP64-005 Task 4,
 * test_lp64_typed_reads.sh). unk_092E50.c is included whole; --gc-sections
 * drops what sub_GAME_7F092E50 does not reach. */
#include <math.h>
#include <stdio.h>
#include "../../src/game/unk_092E50.c"

s32 g_ClockTimer;

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_mipmap: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

/* One tick sets the LOD fraction byte of each list's gsDPSetPrimColor (the
 * fifth command) and leaves the tile sizes' coordinates to their own stores. */
static void testWaterTickWritesThePrimColourCommand(void)
{
    // Arrange: one tick moves the LOD phase to 0.04 and both scroll offsets to 0.
    const u32 lod = (u32) ((sinf(0.04f) * 127.0f) + 128.0f);
    g_ClockTimer = 1;

    // Act
    sub_GAME_7F092E50();

    // Assert
    expect((MipMap2C_Something_Setup[4].words.w0 & 0xFF) == lod && (MipMap2C_Something_Setup[4].words.w0 >> 24) == G_SETPRIMCOLOR,
           "the first list's gsDPSetPrimColor holds the LOD fraction");
    expect((MipMap2C_Something2_Setup[4].words.w0 & 0xFF) == lod && (MipMap2C_Something2_Setup[4].words.w0 >> 24) == G_SETPRIMCOLOR,
           "the second list's gsDPSetPrimColor holds the LOD fraction");
    expect((MipMap2C_Something_Setup[2].words.w0 & 0xFFFFFF) == 0 && (MipMap2C_Something2_Setup[2].words.w0 & 0xFFFFFF) == 0,
           "the first gsDPSetTileSize keeps its coordinates");
}

int main(void)
{
    testWaterTickWritesThePrimColourCommand();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_mipmap: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
