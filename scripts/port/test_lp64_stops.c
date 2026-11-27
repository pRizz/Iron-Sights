/* ROM-free tests of the 64-bit stops' boxes outside port_lp64.c (LP64-007
 * Task 6): sysFatalError (port/src/sysstubs.c) and the converters' two
 * allocation stops (port/src/romconv_common.c). Built at 64 bits by
 * scripts/port/test_lp64_notice.sh, linked with sysstubs.c,
 * port_lp64_notice.c, port_lp64.c and romconv_common.c, and with
 * -Wl,--wrap=malloc (on macOS -Dmalloc=__wrap_malloc: harness_wrap_flags) so
 * that a request can be refused. Each mode runs one stop;
 * the harness checks its status, its log line and the box it prints.
 *
 *   test_lp64_stops fatal     sysFatalError with a message
 *   test_lp64_stops scratch   portConvScratch when malloc refuses its block
 *   test_lp64_stops map       portConvScratchReserve when malloc refuses the map */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include "PR/ultratypes.h"
#include "port_romconv.h"

extern void sysFatalError(const char *fmt, ...);

/* Requests of this many bytes or more are refused; 0 refuses none. */
static size_t s_RefuseFrom;

void *__real_malloc(size_t bytes);
void *__wrap_malloc(size_t bytes);

#ifdef __APPLE__
/* Apple's linker has no --wrap: the harness compiles every source with
 * -Dmalloc=__wrap_malloc (harness_wrap_flags), and the real one is reached
 * here, where the name is the C library's again. */
#undef malloc
extern void *malloc(size_t bytes);
void *__real_malloc(size_t bytes)
{
    return malloc(bytes);
}
#endif

void *__wrap_malloc(size_t bytes)
{
    if (s_RefuseFrom != 0 && bytes >= s_RefuseFrom)
    {
        return NULL;
    }
    return __real_malloc(bytes);
}

/* port_console.c's logger. */
void portConLogf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

/* video.c's box, which the stops open through port_lp64_notice.c: printed. */
void portShowErrorBox(const char *title, const char *text)
{
    printf("box: %s: %s\n", title, text);
    fflush(stdout);
}

/* True when MODE is the program's argument. Not strcmp: the game's <string.h>
 * (include/), which this driver's include order finds first, lacks it. */
static int isMode(int argc, char **argv, const char *mode)
{
    const char *arg;

    if (argc < 2)
    {
        return 0;
    }
    arg = argv[1];
    while (*arg != '\0' && *arg == *mode)
    {
        arg++;
        mode++;
    }
    return *arg == *mode;
}

int main(int argc, char **argv)
{
    if (isMode(argc, argv, "fatal"))
    {
        sysFatalError("The %s file %s is not served from the ROM", "bg", "bg_test");
    }
    if (isMode(argc, argv, "scratch"))
    {
        s_RefuseFrom = 1u << 20;
        (void) portConvScratch(2u << 20);
    }
    if (isMode(argc, argv, "map"))
    {
        /* The scratch block first, so that only the map's request is refused. */
        (void) portConvScratch(4u << 20);
        s_RefuseFrom = 1u << 20;
        portConvScratchReserve(4u << 20);
    }
    printf("%s: returned\n", argc > 1 ? argv[1] : "no mode");
    return 0;
}
