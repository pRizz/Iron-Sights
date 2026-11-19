/**
 * The play log: `ge: play:` lines for scripted runs, and GE_PLAYKILL
 * (LP64-005 Task 8). port_playlog.h has the contract; this file is empty in a
 * build without PORT_PLAY_HARNESS.
 */
#include <stdio.h>
#include <stdlib.h>
#include "port_playlog.h"

#if PORT_PLAY_HARNESS

extern u32 portInputRecTickNow(void);
extern s32 get_cur_playernum(void);
extern void bondviewKillCurrentPlayer(void);

/* -1 until the first call reads GE_PLAYLOG; nonzero tests true, so a run
 * without it pays one test a call after the first. */
static s32 sPlayLogOn = -1;

void portPlayLog(const char *event, s32 value)
{
    char line[128];

    if (!sPlayLogOn)
    {
        return;
    }
    if (sPlayLogOn < 0)
    {
        const char *e = getenv("GE_PLAYLOG");

        sPlayLogOn = (e != NULL && e[0] != '\0' && e[0] != '0');
        if (!sPlayLogOn)
        {
            return;
        }
    }
    /* Both lines in one write, after whatever stdout holds: lp64_play.py
     * matches whole lines, and a line cut by the other stream would read as
     * missing. */
    snprintf(line, sizeof(line), "ge: play: tick %u\nge: play: %s %d\n", (unsigned) portInputRecTickNow(), event,
             (int) value);
    fflush(stdout);
    fputs(line, stdout);
    fflush(stdout);
}

void portPlayKillTick(void)
{
    static s32 killAt = -1; /* -1 until GE_PLAYKILL is read, 0 when unset */
    static s32 passes;

    if (!killAt)
    {
        return;
    }
    if (killAt < 0)
    {
        const char *e = getenv("GE_PLAYKILL");

        killAt = (e != NULL) ? atoi(e) : 0;
        if (killAt <= 0)
        {
            killAt = 0;
            return;
        }
    }
    if (get_cur_playernum() != 0 || ++passes != killAt)
    {
        return;
    }
    portPlayLog("kill", passes);
    bondviewKillCurrentPlayer();
}

#endif
