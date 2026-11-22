/* ROM-free tests of the pool peaks' two kinds of use that may be given back
 * at once (LP64-005 Task 13, port/src/port_lp64.c): a stage bank block that
 * took all the bank had left (portLp64PoolHold) and a room's first block,
 * which takes mema's largest free space (portLp64RoomTake). Each counts as
 * what was kept, not as the pool it briefly held. Built at 64 bits by
 * scripts/port/test_lp64_address.sh, linked with port_lp64.c; at 32 bits the
 * calls are macros that print nothing, which the 32-bit build checks. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "PR/ultratypes.h"
#include "platform_info.h"
#include "port_lp64.h"

static int s_failures;
static int s_peakLines;
static char s_peaks[1024]; /* every peak line since the last reset, in order */

/* port_console.c's logger, which port_lp64.c prints through. */
void portConLogf(const char *fmt, ...)
{
    char line[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    if (strncmp(line, "ge: lp64: peak ", 15) == 0)
    {
        s_peakLines++;
        strncat(s_peaks, line, sizeof(s_peaks) - strlen(s_peaks) - 1);
    }
    fputs(line, stdout);
}

static void resetPeaks(void)
{
    s_peakLines = 0;
    s_peaks[0] = '\0';
}

static void expect(int condition, const char *what)
{
    if (!condition)
    {
        printf("FAIL: %s\n", what);
        s_failures++;
    }
}

#if IS_64_BIT
static void testHeldUseGivenBackCountsTheLaterUse(void)
{
    // Arrange: a block takes the rest of a 1000-byte bank, then is shrunk to leave 400 in use
    portLp64PoolPeaks();
    resetPeaks();
    portLp64PoolUse("stage bank", 300, 1000);
    portLp64PoolHold("stage bank", 1000, 1000);
    portLp64PoolUse("stage bank", 400, 1000);

    // Act
    portLp64PoolPeaks();

    // Assert
    expect(strcmp(s_peaks, "ge: lp64: peak stage bank 400 of 1000 bytes\n") == 0,
           "a held use followed by another use counts the later one");
}

static void testHeldUseKeptCountsAtThePrint(void)
{
    // Arrange: nothing gives the block back before the print
    portLp64PoolPeaks();
    resetPeaks();
    portLp64PoolUse("stage bank", 300, 1000);
    portLp64PoolHold("stage bank", 1000, 1000);

    // Act
    portLp64PoolPeaks();

    // Assert
    expect(strcmp(s_peaks, "ge: lp64: peak stage bank 1000 of 1000 bytes\n") == 0,
           "a held use that nothing followed counts at the print");
}

static void testRoomGrabCountsWhatItKept(void)
{
    // Arrange: 50 bytes in use, then a room's first load takes the longest free (900) and gives back 600
    portLp64PoolPeaks();
    portLp64RoomReset(1000);
    portLp64RoomTake(0x100, 50);
    portLp64RoomLongest(900);
    portLp64RoomTake(0x200, 900);
    portLp64RoomGive(0x200 + 300, 600);
    resetPeaks();

    // Act
    portLp64PoolPeaks();

    // Assert
    expect(strcmp(s_peaks, "ge: lp64: peak room memory largest request 300 of 1000 bytes\n"
                           "ge: lp64: peak room memory 350 of 1000 bytes\n") == 0,
           "a room's first block counts as the 300 bytes it kept, beside the 50 in use");
}

static void testRoomGrabKeptWholeCountsAtThePrint(void)
{
    // Arrange: the room needs all 900 bytes, and no other call comes before the print
    portLp64PoolPeaks();
    portLp64RoomReset(1000);
    portLp64RoomTake(0x100, 50);
    portLp64RoomLongest(900);
    portLp64RoomTake(0x200, 900);
    resetPeaks();

    // Act
    portLp64PoolPeaks();

    // Assert
    expect(strcmp(s_peaks, "ge: lp64: peak room memory largest request 900 of 1000 bytes\n"
                           "ge: lp64: peak room memory 950 of 1000 bytes\n") == 0,
           "a room block held whole at the print counts whole");
}
static void testSameNameFromTwoPointersSharesOneEntry(void)
{
    // Arrange: two arrays holding the same name, as two call sites spelling it would
    static const char first[] = "shared pool", second[] = "shared pool";
    portLp64PoolPeaks();
    resetPeaks();
    portLp64PoolUse(first, 200, 1000);
    portLp64PoolUse(second, 500, 1000);
    portLp64PoolUse(first, 300, 1000);

    // Act
    portLp64PoolPeaks();

    // Assert
    expect(strcmp(s_peaks, "ge: lp64: peak shared pool 500 of 1000 bytes\n") == 0,
           "one name at two addresses is one pool, printed once with its largest use");
}
static void testThirteenPoolsEachPrintOnceAtThePrint(void)
{
    // Arrange: the thirteen names port_lp64.h lists (LP64-006 Task 11), each used twice
    static const char *const names[] = {
        "stage bank", "room memory", "room memory largest request", "graphics buffer", "vertex pool",
        "music heap", "texture pool", "permanent bank", "gun room left hand", "gun room trigger",
        "gun room weapon", "wallet room", "synthesizer parameters",
    };
    int i, linesAtUse;
    portLp64PoolPeaks();
    resetPeaks();
    for (i = 0; i < (int) (sizeof(names) / sizeof(names[0])); i++)
    {
        portLp64PoolUse(names[i], 10, 100);
        portLp64PoolUse(names[i], 20, 100);
    }
    linesAtUse = s_peakLines;

    // Act
    portLp64PoolPeaks();

    // Assert
    expect(linesAtUse == 0 && s_peakLines == 13, "thirteen pools fit the table: one line each, at the print");
}
#else
static void testPeaksAreNoOpsAtThirtyTwoBits(void)
{
    // Arrange
    resetPeaks();

    // Act
    portLp64PoolHold("stage bank", 1000, 1000);
    portLp64RoomReset(1000);
    portLp64RoomLongest(900);
    portLp64RoomTake(0x200, 900);
    portLp64RoomGive(0x200, 900);
    portLp64PoolPeaks();

    // Assert
    expect(s_peakLines == 0, "the peaks print nothing at 32 bits");
}
#endif

int main(void)
{
#if IS_64_BIT
    testHeldUseGivenBackCountsTheLaterUse();
    testHeldUseKeptCountsAtThePrint();
    testRoomGrabCountsWhatItKept();
    testRoomGrabKeptWholeCountsAtThePrint();
    testSameNameFromTwoPointersSharesOneEntry();
    testThirteenPoolsEachPrintOnceAtThePrint();
#else
    testPeaksAreNoOpsAtThirtyTwoBits();
#endif
    printf("test_lp64_peaks: %s\n", s_failures == 0 ? "PASS" : "FAIL");
    return s_failures == 0 ? 0 : 1;
}
