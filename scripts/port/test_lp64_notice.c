/* ROM-free tests of the 64-bit stop notice (LP64-007 Task 6,
 * port/src/port_lp64_notice.c): when a 64-bit stop shows its line in a
 * message box, and that it never shows one with no one there to dismiss it.
 * Built by scripts/port/test_lp64_notice.sh at 64 bits only, where the file
 * compiles: once as a video build, with a stand-in for video.c's
 * portShowErrorBox that counts the boxes, and once as a headless build
 * (-DPORT_LP64_NOTICE_HEADLESS), which links without any box function. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PR/ultratypes.h"
#include "platform_info.h"
#include "port_lp64.h"

static int s_failures;

static void expect(int condition, const char *what)
{
    if (!condition)
    {
        printf("FAIL: %s\n", what);
        s_failures++;
    }
}

#ifndef PORT_LP64_NOTICE_HEADLESS
static int s_boxes;
static char s_boxTitle[64];
static char s_boxText[256];

/* video.c's box, which the notice opens. */
void portShowErrorBox(const char *title, const char *text)
{
    s_boxes++;
    snprintf(s_boxTitle, sizeof(s_boxTitle), "%s", title);
    snprintf(s_boxText, sizeof(s_boxText), "%s", text);
}
#endif

typedef struct NoticeCase
{
    const char *noInput;
    const char *videoDriver;
    s32 haveVideo;
    s32 wanted;
} NoticeCase;

static void testWantedOverEveryCombination(void)
{
    // Arrange: the eight combinations of GE_NOINPUT, SDL_VIDEODRIVER and a
    // video build, then the values on either side of each rule
    static const NoticeCase cases[] = {
        { NULL, NULL, 1, 1 },
        { NULL, NULL, 0, 0 },
        { NULL, "offscreen", 1, 0 },
        { NULL, "offscreen", 0, 0 },
        { "1", NULL, 1, 0 },
        { "1", NULL, 0, 0 },
        { "1", "offscreen", 1, 0 },
        { "1", "offscreen", 0, 0 },
        { "0", NULL, 1, 1 },
        { "", NULL, 1, 0 },
        { NULL, "dummy", 1, 0 },
        { NULL, "x11", 1, 1 },
        { NULL, "wayland", 1, 1 },
        { NULL, "", 1, 1 },
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        char what[160];
        s32 got;

        // Act
        got = portLp64StopNoticeWanted(cases[i].noInput, cases[i].videoDriver, cases[i].haveVideo);

        // Assert
        snprintf(what, sizeof(what), "wanted(noInput=%s, videoDriver=%s, haveVideo=%d) is %d, got %d",
                 cases[i].noInput ? cases[i].noInput : "unset", cases[i].videoDriver ? cases[i].videoDriver : "unset",
                 (int) cases[i].haveVideo, (int) cases[i].wanted, (int) got);
        expect(got == cases[i].wanted, what);
    }
}

#ifndef PORT_LP64_NOTICE_HEADLESS
static void testNoBoxUnderNoInput(void)
{
    // Arrange
    s_boxes = 0;
    setenv("GE_NOINPUT", "1", 1);
    unsetenv("SDL_VIDEODRIVER");

    // Act
    portLp64StopNotice("ge: lp64: a stop under GE_NOINPUT");

    // Assert
    expect(s_boxes == 0, "no box opens under GE_NOINPUT=1");
}

static void testNoBoxUnderOffscreenDriver(void)
{
    // Arrange
    s_boxes = 0;
    unsetenv("GE_NOINPUT");
    setenv("SDL_VIDEODRIVER", "offscreen", 1);

    // Act
    portLp64StopNotice("ge: lp64: a stop offscreen");

    // Assert
    expect(s_boxes == 0, "no box opens under SDL_VIDEODRIVER=offscreen");
}

static void testBoxShowsTheLineOnce(void)
{
    // Arrange: someone is there; a second stop follows the first
    s_boxes = 0;
    unsetenv("GE_NOINPUT");
    unsetenv("SDL_VIDEODRIVER");

    // Act
    portLp64StopNotice("ge: lp64: the first stop");
    portLp64StopNotice("ge: lp64: a stop during the first");

    // Assert
    expect(s_boxes == 1, "one box opens, and a later stop does not open another");
    expect(strcmp(s_boxTitle, "Iron Sights") == 0, "the box's title is Iron Sights");
    expect(strcmp(s_boxText, "ge: lp64: the first stop") == 0, "the box shows the first stop's line");
}
#else
static void testHeadlessDoesNothing(void)
{
    // Arrange: a headless build, with nothing to suppress the box
    unsetenv("GE_NOINPUT");
    unsetenv("SDL_VIDEODRIVER");

    // Act
    portLp64StopNotice("ge: lp64: a headless stop");

    // Assert: the link is the assertion (this program defines no box
    // function, so a headless notice that called one would not link), and
    // reaching here shows that it returned
}
#endif

int main(void)
{
    testWantedOverEveryCombination();
#ifndef PORT_LP64_NOTICE_HEADLESS
    /* The once-only test last: after it no box opens in this process. */
    testNoBoxUnderNoInput();
    testNoBoxUnderOffscreenDriver();
    testBoxShowsTheLineOnce();
#else
    testHeadlessDoesNothing();
#endif
    if (s_failures != 0)
    {
        printf("test_lp64_notice: %d failure(s)\n", s_failures);
        return 1;
    }
    return 0;
}
