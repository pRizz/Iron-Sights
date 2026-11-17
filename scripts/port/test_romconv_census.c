/* The load census's lines (LP64-004 Task 16): at 64 bits the model and stan
 * converters log the `ge: romdata:` line their in-place passes log, with the
 * same counts, and the model converter the census line of its opcodes; at 32
 * bits (the oracle build, where the in-place pass logs) they log nothing.
 * Built and run at each pointer width by test_romconv.sh, linked with
 * romfile_model.c. Every input is written here. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include "bondconstants.h"
#include "platform_info.h"
#include "port_romconv.h"
#include "port_romconv_stage.h"
#include "port/src/romconv_census.h"

#define CAP 0x400u
#define SEG(off) (0x05000000u | (u32) (off))
#define sameText(a, b) (__builtin_strcmp((a), (b)) == 0)

/* The game's <stdlib.h>, first on the include path, declares no setenv. */
extern int setenv(const char *name, const char *value, int overwrite);
s32 romfileModelConvert(u8 *data, u32 size, s32 numSwitches, s32 numtextures, const char *name, const u8 *compiled);

static u8 g_File[CAP];
static u8 g_Copy[CAP];
static u8 g_Out[4 * CAP] __attribute__((aligned(16)));
static char g_Log[2048];

/* port_console.c's log, every line kept in order. */
void portConLogf(const char *fmt, ...)
{
    size_t used = __builtin_strlen(g_Log);
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(g_Log + used, sizeof(g_Log) - used, fmt, ap);
    va_end(ap);
}

/* romfile_model.c's 64-bit load stops the game through it; no test reaches it. */
void sysFatalError(const char *format, ...)
{
    printf("test_romconv_census: FAIL: sysFatalError(%s)\n", format);
    exit(1);
}

static int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_census: FAIL: %s\nlog:\n%s", what, g_Log);
        return 1;
    }
    return 0;
}

static void put16(u32 at, u32 v)
{
    g_File[at] = (u8) (v >> 8);
    g_File[at + 1] = (u8) v;
}

static void put32(u32 at, u32 v)
{
    put16(at, v >> 16);
    put16(at + 2, v & 0xFFFF);
}

/* A header node over a DL node, whose record has 3 vertices, a primary list
 * that calls a second list and that second list as its secondary: 2 nodes,
 * 3 vertices and 2 lists, the called list counted once. Returns its size. */
static u32 buildModel(void)
{
    __builtin_memset(g_File, 0, sizeof(g_File));
    put16(0x00, MODELNODE_OPCODE_HEADER); /* node 0 at 0x00, its record at 0x30 */
    put32(0x04, SEG(0x30));
    put32(0x14, SEG(0x18));
    put16(0x18, MODELNODE_OPCODE_DL);     /* node 1 at 0x18, its record at 0x40 */
    put32(0x1C, SEG(0x40));
    put32(0x20, SEG(0x00));
    put32(0x40, SEG(0x88));               /* primary: list A */
    put32(0x44, SEG(0x98));               /* secondary: list B */
    put32(0x4C, SEG(0x58));               /* 3 vertices at 0x58 */
    put16(0x50, 3);
    put32(0x88, 0x06000000u);             /* list A: call list B, end */
    put32(0x8C, SEG(0x98));
    put32(0x90, 0xB8000000u);
    put32(0x98, 0xB8000000u);             /* list B: end */
    return 0xA0;
}

static int testModelLine(void)
{
    // Arrange
    const u32 size = buildModel();
    u32 n = portModelConvertedSize(g_File, size, 0, 0, "Ptest");
    char inplace[sizeof(g_Log)];

    __builtin_memcpy(g_Copy, g_File, size);
    g_Log[0] = '\0';
    romfileModelConvert(g_Copy, size, 0, 0, "Ptest", NULL);
    __builtin_memcpy(inplace, g_Log, sizeof(g_Log));
    g_Log[0] = '\0';

    // Act
    s32 ok = n != 0 && n <= sizeof(g_Out) && portModelConvert(g_File, size, g_Out, n, 0, 0, "Ptest");

    // Assert
#if IS_64_BIT
    return expect(ok && sameText(inplace, "ge: romdata: model Ptest            converted (2 nodes, 3 verts, 2 DLs)\n")
            && sameText(g_Log, "ge: romdata: model Ptest            converted (2 nodes, 3 verts, 2 DLs)\n"
                               "ge: census: model Ptest opcodes 1=1 4=1\n"),
            "the converter logs the in-place line, then the opcodes");
#else
    return expect(ok && g_Log[0] == '\0', "at 32 bits the converter logs nothing");
#endif
}

/* A stan file of one room and two tiles (3 and 4 points), then the zero
 * word at 0x58: 2 tiles, 1 room, 88 bytes. Returns its size. */
static u32 buildStan(void)
{
    __builtin_memset(g_File, 0, sizeof(g_File));
    put32(0x04, 0x10); /* first tile */
    put32(0x08, 0x10); /* room 0's first tile, then the zero slot */
    put32(0x10, 0x00000101u);
    put16(0x16, 0x3000);
    put32(0x30, 0x00000201u);
    put16(0x36, 0x4000);
    return 0x5C;
}

static int testStanLine(void)
{
    // Arrange
    const u32 size = buildStan();
    g_Log[0] = '\0';

    // Act
    int ok = portStanConvert(g_File, size, (uintptr_t) g_File, "Tteststan");

    // Assert
#if IS_64_BIT
    return expect(ok && sameText(g_Log, "ge: romdata: stan  Tteststan          converted (2 tiles, 1 rooms, 88 bytes)\n"),
            "the stan converter logs the in-place line's counts");
#else
    return expect(ok && g_Log[0] == '\0', "at 32 bits the stan converter logs nothing");
#endif
}

static int testCensusLineListsTheCountsThatAreNotZero(void)
{
    // Arrange
    const u32 counts[5] = {0, 7, 0, 0, 12};
    g_Log[0] = '\0';

    // Act
    portCensusLine("setup", "Usetuptest", "props", counts, 5);

    // Assert
    return expect(sameText(g_Log, "ge: census: setup Usetuptest props 1=7 4=12\n"), "the census line");
}

int main(void)
{
    int failures;

    setenv("GE_LP64_CENSUS", "1", 1);
    failures = testModelLine() + testStanLine() + testCensusLineListsTheCountsThatAreNotZero();
    if (failures != 0)
    {
        printf("test_romconv_census: %d failed\n", failures);
        return 1;
    }
    printf("test_romconv_census: OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
