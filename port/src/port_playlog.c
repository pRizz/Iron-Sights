/**
 * The play log: `ge: play:` lines for scripted runs, and GE_PLAYKILL
 * (LP64-005 Task 8). port_playlog.h has the contract; this file is empty in a
 * build without PORT_PLAY_HARNESS.
 */
#include <stdio.h>
#include <stdlib.h>
#include "port_playlog.h"

#if PORT_PLAY_HARNESS
#include <bondconstants.h>
#include "game/bondview.h"
#include "game/player.h"

extern u32 portInputRecTickNow(void);
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

/* GE_PLAYKILL (port_playlog.h): player 0's N-th pass kills him. */
static void playKill(void)
{
    static s32 killAt = -1; /* -1 until GE_PLAYKILL is read, 0 when unset */
    static s32 passes;
    s32 wasDead;

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
    if (++passes != killAt)
    {
        return;
    }
    wasDead = g_CurrentPlayer->bonddead;
    bondviewKillCurrentPlayer();
    if (!wasDead && g_CurrentPlayer->bonddead)
    {
        portPlayLog("kill", passes);
    }
}

/* `weapon` (port_playlog.h): player 0's hands after a change, right | left << 8. */
static void playWeapon(void)
{
    static s32 held = -1; /* -1 until the first pass records the starting pair */
    const s32 left = g_CurrentPlayer->hands[GUNLEFT].weaponnum;
    const s32 now = g_CurrentPlayer->hands[GUNRIGHT].weaponnum | ((left > 0 ? left : 0) << 8);

    if (now == held)
    {
        return;
    }
    if (held >= 0)
    {
        portPlayLog("weapon", now);
    }
    held = now;
}

/* `shots`, `moved` and `weapon` (port_playlog.h), from player 0's own counters. */
static void playStats(void)
{
    static s32 shots;
    static s32 moved;
    s32 now;

    if (!sPlayLogOn || g_playerPerm == NULL)
    {
        return;
    }
    playWeapon();
    now = g_playerPerm->shot_count[SHOT_REGISTER_TOTAL];
    if (now != shots)
    {
        shots = now;
        portPlayLog("shots", now);
    }
    if (!moved && g_playerPerm->distance_traveled >= PORT_PLAY_MOVED_UNITS)
    {
        moved = 1;
        portPlayLog("moved", (s32) g_playerPerm->distance_traveled);
    }
}

void portPlayPlayerTick(void)
{
    if (get_cur_playernum() != 0 || g_CurrentPlayer == NULL)
    {
        return;
    }
    playKill();
    playStats();
}

void portPlayWatchPage(s32 page)
{
    static s32 drawn = -1; /* the tab of the last draw with the watch open */
    static s32 draws;      /* consecutive open draws of that tab */

    if (g_CurrentPlayer == NULL || g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x5)
    {
        drawn = -1;
        return;
    }
    if (page != drawn)
    {
        drawn = page;
        draws = 0;
    }
    if (++draws == PORT_PLAY_WATCH_STEADY_DRAWS)
    {
        portPlayLog("watch-page", page);
    }
}

#endif
