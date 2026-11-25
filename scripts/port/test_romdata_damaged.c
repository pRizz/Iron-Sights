/* LP64-004 fix wave C's two damaged-ROM paths, held by fixtures (LP64-007
 * Task 7): at 64 bits a ROM segment the game needs and the ROM lacks stops the
 * game by name (segments.c's portRomdataSegsApply), and a text bank that
 * arrives still 1172-compressed, its inflate never having run, is zeroed
 * rather than rebased (romdata.c's romdataTextPreprocess). The stock ROM
 * reaches neither. The paths exist only at 64 bits, so test_romconv.sh builds
 * this at 64 bits only, under the sanitizers.
 *
 * romdata.c is included rather than linked, to reach its static
 * romdataTextPreprocess and its ROM image's statics; the game functions it and
 * segments.c call are stubbed below. The "ROM" is 0x100 zero bytes written
 * here, with only the images bank inside it. */
#include "../../port/src/romdata.c"
#include <setjmp.h>
#include <stdarg.h>

static char s_Log[4096];
static char s_Fatal[512];
static jmp_buf s_Stop;

void portConLogf(const char *fmt, ...)
{
    size_t used = strlen(s_Log);
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_Log + used, sizeof(s_Log) - used, fmt, ap);
    va_end(ap);
}

/* The game's stop: recorded, and back to the test that expected it. */
void sysFatalError(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vsnprintf(s_Fatal, sizeof(s_Fatal), format, ap);
    va_end(ap);
    longjmp(s_Stop, 1);
}

/* ==== what romdata.c and segments.c call, never reached here ============ */

static char s_BankName[] = "LtestE";
fileentry file_resource_table[1] = {{0, s_BankName, NULL}};
resource_lookup_data_entry resource_lookup_data_array[1];
s32 file_entry_max = 1;
struct image_entry g_Textures[1];
u8 *port_unknown2, *port_unknown2_end;
u32 imgRAre_0x0020[1], img_raRE_0x0AE0[1], imgWAre_0x15A0[1], imgwaRE_0x2060[1], D_02004FE8[1], D_02005FF0[1];

static void unreached(const char *name)
{
    printf("test_romdata_damaged: %s called\n", name);
    abort();
}

u32 decompressdata(u8 *src, u8 *dst, struct huft *hlist)
{
    (void) src, (void) dst, (void) hlist;
    unreached("decompressdata");
    return 0;
}
void *fileIndexLoadToAddr(s32 index, FILELOADMETHOD loadMethod, void *ptrdata, s32 bytes)
{
    (void) index, (void) loadMethod, (void) ptrdata, (void) bytes;
    unreached("fileIndexLoadToAddr");
    return NULL;
}
void *portAllocHighDedicated(u32 size)
{
    (void) size;
    unreached("portAllocHighDedicated");
    return NULL;
}
const char *portRootPath(const char *name)
{
    (void) name;
    unreached("portRootPath");
    return NULL;
}
FILE *portRootFopen(const char *name, const char *mode)
{
    (void) name, (void) mode;
    unreached("portRootFopen");
    return NULL;
}
void portFogServeFromRom(void)
{
    unreached("portFogServeFromRom");
}
void portShowErrorBox(const char *title, const char *text)
{
    (void) title, (void) text;
    unreached("portShowErrorBox");
}
u32 portSetupConvertedSize(const u8 *file, u32 size, const char *name)
{
    (void) file, (void) size, (void) name;
    unreached("portSetupConvertedSize");
    return 0;
}
s32 portSetupConvert(const u8 *file, u32 size, u8 *out, u32 outsize, const char *name)
{
    (void) file, (void) size, (void) out, (void) outsize, (void) name;
    unreached("portSetupConvert");
    return 0;
}
s32 romfileStanConvert(u8 *data, u32 size, const char *name, const u8 *compiled)
{
    (void) data, (void) size, (void) name, (void) compiled;
    unreached("romfileStanConvert");
    return 0;
}
void *portRamromFdata(s32 index)
{
    (void) index;
    unreached("portRamromFdata");
    return NULL;
}
void portRamromRebase(s32 index, void *fdata)
{
    (void) index, (void) fdata;
    unreached("portRamromRebase");
}

/* ==== the tests ========================================================= */

static int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romdata_damaged: FAIL: %s (fatal: %s; log: %s)\n", what, s_Fatal, s_Log);
        fflush(stdout);
        return 1;
    }
    return 0;
}

static int testMissingSegmentStopsByName(void)
{
    // Arrange: a 0x100-byte ROM serving the images bank at 0x10, and none of
    // the audio banks' segments, which lie past its end
    static u8 rom[0x100];
    volatile int stopped = 0;
    sRom = rom;
    sRomSize = sizeof(rom);
    sImagesBase = 0x10;
    sImagesSize = 0x10;
    s_Fatal[0] = '\0';

    // Act
    if (setjmp(s_Stop) == 0)
    {
        portRomdataSegsApply();
    }
    else
    {
        stopped = 1;
    }

    // Assert
    return expect(stopped && strcmp(s_Fatal, "The ROM segment sfxctl is missing; a 64-bit build has no compiled-in "
            "copy to read") == 0, "a missing ROM segment stops the game, naming it");
}

static int testCompressedTextBankIsZeroed(void)
{
    // Arrange: a bank whose bytes are still a 1172 stream: its header, then data
    static u8 bank[24] = {0x11, 0x72, 0x00, 0x00, 0x01, 0x00, 0x40, 0x00, 0x12, 0x34, 0x56, 0x78};
    u32 i, nonzero = 0;
    s_Log[0] = '\0';

    // Act
    romdataTextPreprocess(0, bank, sizeof(bank));

    // Assert
    for (i = 0; i < sizeof(bank); i++)
    {
        nonzero += bank[i] != 0;
    }
    return expect(nonzero == 0 && __builtin_strstr(s_Log, "text LtestE arrived still 1172-compressed (raw load) - zeroed") != NULL,
            "a text bank that arrives still compressed is zeroed, every slot not loaded");
}

int main(void)
{
    int failures = testMissingSegmentStopsByName() + testCompressedTextBankIsZeroed();

    if (failures != 0)
    {
        return 1;
    }
    printf("test_romdata_damaged: OK\n");
    return 0;
}
