/* ROM-free checks of ob.c's host-file load, resource_load_from_indy, when the
 * host reports no file (pc_size 0), as the index-0 load does on PC (LP64-006
 * Task 16, test_lp64_typed_reads.sh). The game file is included whole, as
 * test_lp64_typed_chraction.c does, with stubs for the functions the tested
 * one calls; --gc-sections drops everything else. A 64-bit build reads nothing
 * at or past the allocation's end; the 32-bit program keeps the inherited read
 * of two bytes there (decision 10; the owner's patch is
 * t16-index0-32bit.patch), so its expectations are the inherited behaviour. */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../src/game/ob.c"

enum
{
    ALLOCATION = 64,
    INFLATED_SIZE = 0x1234
};

static int failures;
static int decompressCalls;
static s32 hostSize;
static u8 hostFile[16];

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_ob: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

/* The host's answer: no file when hostSize is 0, which leaves pc_size as the
 * PC's indycommHostCheckFileExists leaves it (it writes nothing). */
u8 *indycommHostCheckFileExists(char *name, s32 *size)
{
    (void) name;
    if (hostSize == 0)
    {
        return NULL;
    }
    *size = hostSize;
    return hostFile;
}

void indycommHostLoadFile(char *filename, u8 *targetloc)
{
    (void) filename;
    if (hostSize != 0)
    {
        memcpy(targetloc, hostFile, (size_t) hostSize);
    }
}

u32 decompressdata(u8 *src, u8 *dst, struct huft *hlist)
{
    (void) src;
    (void) dst;
    (void) hlist;
    decompressCalls++;
    return INFLATED_SIZE;
}

static fileentry missingFile = {0, "missing", NULL};

/* An allocation of ALLOCATION bytes ending at a page that no access may
 * touch, so a read past its end ends the child by SIGSEGV. */
static void loadBeforeGuardPage(void)
{
    long page = sysconf(_SC_PAGESIZE);
    u8 *pages = mmap(NULL, (size_t) page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    resource_lookup_data_entry lookup = {0};

    if (pages == MAP_FAILED || mprotect(pages + page, (size_t) page, PROT_NONE) != 0)
    {
        _exit(2);
    }
    resource_load_from_indy(pages + page - ALLOCATION, ALLOCATION, &missingFile, &lookup);
    _exit(lookup.poolRemaining == 0 ? 0 : 3);
}

static int childEnding(void (*call)(void))
{
    int status;
    pid_t child;

    fflush(stdout);
    child = fork();
    if (child == 0)
    {
        call();
        _exit(0);
    }
    if (child < 0 || waitpid(child, &status, 0) != child)
    {
        return -1;
    }
    if (WIFSIGNALED(status))
    {
        return WTERMSIG(status);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void testNoReadPastTheAllocation(void)
{
    // Arrange
    hostSize = 0;

    // Act
    int ending = childEnding(loadBeforeGuardPage);

    // Assert
    char line[160];
    snprintf(line, sizeof(line), "a missing host file reads nothing past its allocation (ended by %d: 0 returned, %d SIGSEGV)",
             ending, SIGSEGV);
#if IS_64_BIT
    expect(ending == 0, line);
#else
    expect(ending == SIGSEGV, line);
#endif
}

static void testHeaderBytesPastTheAllocationAreNotInflated(void)
{
    // Arrange
    u8 arena[ALLOCATION + 8] = {0};
    resource_lookup_data_entry lookup = {0};
    arena[ALLOCATION] = 0x11;
    arena[ALLOCATION + 1] = 0x72;
    hostSize = 0;
    decompressCalls = 0;

    // Act
    resource_load_from_indy(arena, ALLOCATION, &missingFile, &lookup);

    // Assert
#if IS_64_BIT
    expect(decompressCalls == 0, "a missing host file does not enter the decompressor");
    expect(lookup.poolRemaining == 0, "a missing host file loads 0 bytes");
#else
    expect(decompressCalls == 1, "the 32-bit program inflates the header bytes past its allocation (inherited)");
    expect(lookup.poolRemaining == INFLATED_SIZE, "the 32-bit program keeps the inflated size (inherited)");
#endif
}

static void testAPresentHostFileStillLoads(void)
{
    // Arrange
    u8 arena[ALLOCATION] = {0};
    resource_lookup_data_entry lookup = {0};
    for (size_t i = 0; i < sizeof(hostFile); i++)
    {
        hostFile[i] = (u8) (0xA0 + i);
    }
    hostSize = sizeof(hostFile);
    decompressCalls = 0;

    // Act
    resource_load_from_indy(arena, ALLOCATION, &missingFile, &lookup);

    // Assert
    expect(decompressCalls == 0 && lookup.poolRemaining == sizeof(hostFile)
               && memcmp(arena, hostFile, sizeof(hostFile)) == 0,
           "a present uncompressed host file is copied to the allocation's start");
}

int main(void)
{
    testNoReadPastTheAllocation();
    testHeaderBytesPastTheAllocationAreNotInflated();
    testAPresentHostFileStillLoads();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_ob: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
