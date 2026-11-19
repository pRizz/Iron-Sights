/* ROM-free check that a headless build replays recorded pads (LP64-005 Task 8,
 * test_inputrec_headless.sh). libultra_shim.c's pad read and inputrec.c are
 * included whole, as the typed-read tests include their game files, and built
 * without GE_VIDEO; --gc-sections drops the shim's other functions, so only
 * what osContGetReadData reaches needs a stub. Each scenario runs in its own
 * child, since inputrec.c reads GE_INPUTPLAY once a process. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../port/src/libultra_shim.c"
#include "../../port/src/inputrec.c"

#define CALLS 5

void portConLogf(const char *fmt, ...)
{
    (void) fmt;
}

void ge_dbg_vec(const char *tag, const f32 *v, s32 n)
{
    (void) tag;
    (void) v;
    (void) n;
}

static int failures;

static void expect(int ok, const char *what, int call)
{
    if (!ok)
    {
        printf("test_inputrec_headless: FAIL (%d-bit): %s (call %d)\n", (int) (sizeof(void *) * 8), what,
               call);
        failures++;
    }
}

/* Reads pad 0 CALLS times; the call numbered PRESSED (1-based) must hold
 * BUTTON and every other call nothing. PRESSED 0 expects all calls neutral. */
static int readPads(int pressed, u16 button)
{
    OSContPad pads[MAXCONTROLLERS];
    int call;

    for (call = 1; call <= CALLS; call++)
    {
        memset(pads, 0xA5, sizeof(pads));
        osContGetReadData(pads);
        expect(pads[0].button == (call == pressed ? button : 0), "pad 0's buttons", call);
        expect(pads[0].stick_x == 0 && pads[0].stick_y == 0, "pad 0's stick", call);
        expect(pads[1].button == 0 && pads[1].stick_x == 0 && pads[1].stick_y == 0, "pad 1", call);
    }
    return failures != 0;
}

/* Runs SCENARIO in a child with GE_INPUTPLAY set to PLAY (unset when NULL). */
static int inChild(const char *name, const char *play, int pressed, u16 button)
{
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    if (child < 0)
    {
        perror("fork");
        return 1;
    }
    if (child == 0)
    {
        if (play != NULL)
        {
            setenv("GE_INPUTPLAY", play, 1);
        }
        else
        {
            unsetenv("GE_INPUTPLAY");
        }
        status = readPads(pressed, button);
        fflush(stdout); /* _exit skips stdio */
        _exit(status);
    }
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        printf("test_inputrec_headless: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), name);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    char path[] = "/tmp/midas-inputrec-XXXXXX";
    int fd;
    int bad = 0;
    FILE *route;

    (void) argc;
    (void) argv;
    // Arrange: one START press recorded at tick 3, a line of each ignored kind
    fd = mkstemp(path);
    if (fd < 0 || (route = fdopen(fd, "w")) == NULL)
    {
        perror("mkstemp");
        return 1;
    }
    fprintf(route, "P 3 1000 0 0\nA 2 1 0\nM 2 0 1 1 0 0\nD 2 5 5\nT 2 0 0 0 0\n");
    fclose(route);

    // Act and Assert: the third read has START, the others nothing
    bad |= inChild("a replayed START at tick 3", path, 3, START_BUTTON);
    // Act and Assert: with GE_INPUTPLAY unset all five reads are neutral
    bad |= inChild("no replay", NULL, 0, 0);

    unlink(path);
    if (bad)
    {
        return 1;
    }
    printf("test_inputrec_headless: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
