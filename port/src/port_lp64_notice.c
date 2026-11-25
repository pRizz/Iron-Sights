/**
 * The 64-bit stop notice (port_lp64.h; LP64-007 Task 6).
 *
 * A 64-bit build has stops a 32-bit build does not (a host address below
 * 4 GB, an unconverted asset class, a fatal pool overrun, a file the ROM does
 * not serve). Each writes its line to the log and exits; someone who started
 * the game from a menu sees only the window close. Before the exit, the stop
 * shows that line in a message box, as the no-ROM message does
 * (romdata.c, through video.c's portShowErrorBox).
 *
 * Nothing may wait on a box that no one can dismiss: the test tools run these
 * stops on purpose, with GE_NOINPUT or an offscreen or dummy video driver, and
 * a headless build has no SDL at all. portShowErrorBox also returns at once
 * under GE_FRAMES and GE_NOMSGBOX. The notice adds no log line, so the log and
 * a comparison build's streams are as they were, and it never changes the
 * exit status. CMake compiles this file at 64 bits only, and defines
 * PORT_LP64_NOTICE_HEADLESS for it in a headless build.
 */
#include <stdlib.h>
#include "port_lp64.h"

#if IS_64_BIT

#ifdef PORT_LP64_NOTICE_HEADLESS
#define LP64_NOTICE_HAVE_VIDEO 0
#else
#define LP64_NOTICE_HAVE_VIDEO 1
/* video.c: an SDL error box, which returns at once under GE_FRAMES or GE_NOMSGBOX. */
extern void portShowErrorBox(const char *title, const char *text);
#endif

/* By text. Not strcmp: the game's <string.h> (include/) does not declare it. */
static int lp64NoticeSame(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

s32 portLp64StopNoticeWanted(const char *noInput, const char *videoDriver, s32 haveVideo)
{
    if (!haveVideo)
    {
        return 0;
    }
    if (noInput != NULL && !lp64NoticeSame(noInput, "0"))
    {
        return 0;
    }
    if (videoDriver != NULL && (lp64NoticeSame(videoDriver, "offscreen") || lp64NoticeSame(videoDriver, "dummy")))
    {
        return 0;
    }
    return 1;
}

void portLp64StopNotice(const char *text)
{
#if LP64_NOTICE_HAVE_VIDEO
    /* One box a process: a stop on another thread, or in an exit handler,
     * while the first box is open adds none and goes on to its own exit. */
    static int s_Shown;

    if (!portLp64StopNoticeWanted(getenv("GE_NOINPUT"), getenv("SDL_VIDEODRIVER"), LP64_NOTICE_HAVE_VIDEO))
    {
        return;
    }
    if (__atomic_exchange_n(&s_Shown, 1, __ATOMIC_SEQ_CST) != 0)
    {
        return;
    }
    portShowErrorBox("Iron Sights", text);
#else
    (void) text;
#endif
}

#endif
