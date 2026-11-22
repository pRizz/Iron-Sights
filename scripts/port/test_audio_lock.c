/* ROM-free checks of the comparison build's sound lock (LP64-006 Task 9,
 * test_lp64_compare.sh): portAudioLockFrames answers the audio frames due,
 * one a game field, and -1 when the lock is off.
 *
 * test_lp64_compare.sh builds this file twice. With -DGE_COMPARE it links
 * port/src/port_audio_lock.c and runs the scenarios that read GE_AUDIO_LOCK,
 * each in its own child, since the lock reads the variable once a process.
 * Without GE_COMPARE it checks the header's stand-in, which a shipped build's
 * audio_pc.c compiles. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "port_audio_lock.h"

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_audio_lock: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

#ifdef GE_COMPARE
static int oneFrameAField(void)
{
    s32 answers[4];

    /* Arrange */
    setenv("GE_AUDIO_LOCK", "1", 1);

    /* Act */
    answers[0] = portAudioLockFrames(5);
    answers[1] = portAudioLockFrames(6);
    answers[2] = portAudioLockFrames(6);
    answers[3] = portAudioLockFrames(9);

    /* Assert */
    return answers[0] == 0 && answers[1] == 1 && answers[2] == 0 && answers[3] == 3 ? 0 : 1;
}

static int offWithoutTheVariable(void)
{
    /* Arrange */
    unsetenv("GE_AUDIO_LOCK");

    /* Act, Assert */
    return portAudioLockFrames(5) == -1 ? 0 : 1;
}

static void inChild(const char *name, int (*scenario)(void))
{
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    if (child < 0)
    {
        perror("fork");
        failures++;
        return;
    }
    if (child == 0)
    {
        status = scenario();
        fflush(NULL); /* _exit skips stdio */
        _exit(status);
    }
    expect(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0, name);
}
#else
static void offOutsideAComparisonBuild(void)
{
    /* Arrange */
    setenv("GE_AUDIO_LOCK", "1", 1);

    /* Act, Assert */
    expect(portAudioLockFrames(5) == -1, "off_outside_a_comparison_build");
}
#endif

int main(void)
{
#ifdef GE_COMPARE
    inChild("one_frame_a_field", oneFrameAField);
    inChild("off_without_the_variable", offWithoutTheVariable);
    const char *build = "comparison build";
#else
    offOutsideAComparisonBuild();
    const char *build = "without GE_COMPARE";
#endif
    if (failures)
    {
        return 1;
    }
    printf("test_audio_lock: PASS (%d-bit, %s)\n", (int) (sizeof(void *) * 8), build);
    return 0;
}
