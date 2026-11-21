/**
 * The play log: `ge: play:` lines for scripted runs, GE_PLAYKILL (LP64-005
 * Task 8), the gun line, GE_PLAYSTALL and GE_PLAYHURT (LP64-006 Task 1).
 * port_playlog.h has the contract; this file is empty in a build without
 * PORT_PLAY_HARNESS.
 */
#include <stdio.h>
#include <stdlib.h>
#include "port_playlog.h"

#if PORT_PLAY_HARNESS
#include <bondconstants.h>
#include "game/bondview.h"
#include "game/lv.h"
#include "game/player.h"
#include "joy.h"
#ifdef _WIN32
__declspec(dllimport) void __stdcall Sleep(unsigned long ms);
#else
#include <errno.h>
#include <time.h>
#endif

extern u32 portInputRecTickNow(void);
extern void bondviewKillCurrentPlayer(void);

/* GE_PLAYLOG's level: -1 until read, 0 off, 1 the events, 2 the events and
 * the gun line. Any value but empty or one starting with 0 is at least 1, as
 * before the gun line. */
static s32 sPlayLogLevel = -1;

static s32 playLogLevel(void)
{
    if (sPlayLogLevel < 0)
    {
        const char *e = getenv("GE_PLAYLOG");

        if (e == NULL || e[0] == '\0' || e[0] == '0')
        {
            sPlayLogLevel = 0;
        }
        else
        {
            sPlayLogLevel = atoi(e) >= 2 ? 2 : 1;
        }
    }
    return sPlayLogLevel;
}

void portPlayLog(const char *event, s32 value)
{
    char line[128];

    /* the cached level first, so a run without the log pays one test */
    if (!sPlayLogLevel || !playLogLevel())
    {
        return;
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

/* `weapon` (port_playlog.h): player 0's hands after a change, right | left << 8.
 * Returns 1 when it logged a change. */
static s32 playWeapon(void)
{
    static s32 held = -1; /* -1 until the first pass records the starting pair */
    const s32 left = g_CurrentPlayer->hands[GUNLEFT].weaponnum;
    const s32 now = g_CurrentPlayer->hands[GUNRIGHT].weaponnum | ((left > 0 ? left : 0) << 8);
    const s32 logged = (now != held && held >= 0);

    if (logged)
    {
        portPlayLog("weapon", now);
    }
    held = now;
    return logged;
}

/* One hand's fields of the gun line, in the order port_playlog.h gives. */
static void gunHand(char *out, size_t size, const struct hand *hand)
{
    snprintf(out, size, "%d,%d,%d,%d,%d,%d,%d", (int) hand->weaponnum, (int) hand->weapon_action_state,
             (int) hand->weapon_current_animation, (int) hand->weapon_firing_status, (int) hand->weapon_hold_time,
             (int) hand->weapon_ammo_in_magazine, (int) hand->weapon_next_weapon);
}

void portPlayGunLine(u32 tick)
{
    char right[96];
    char left[96];
    char line[288];

    if (playLogLevel() < 2 || g_CurrentPlayer == NULL)
    {
        return;
    }
    gunHand(right, sizeof(right), &g_CurrentPlayer->hands[GUNRIGHT]);
    gunHand(left, sizeof(left), &g_CurrentPlayer->hands[GUNLEFT]);
    snprintf(line, sizeof(line), "ge: play: gun tick=%u clock=%d global=%d r=%s l=%s\n", (unsigned) tick,
             (int) g_ClockTimer, (int) g_GlobalTimer, right, left);
    fflush(stdout);
    fputs(line, stdout);
    fflush(stdout);
}

/* Weak, so test_playlog_gun.c can count the calls instead of sleeping. */
__attribute__((weak)) void portPlaySleepMs(u32 ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec left = { (time_t) (ms / 1000), (long) (ms % 1000) * 1000000L };

    while (nanosleep(&left, &left) != 0 && errno == EINTR)
    {
    }
#endif
}

/* A harness action done once, on player 0's first pass at or after an input
 * tick its variable names: GE_PLAYSTALL=T:MS and GE_PLAYHURT=T. */
typedef struct PlayOnce
{
    const char *variable;
    s32 wantsValue; /* 1 when the variable is TICK:VALUE, VALUE above 0 */
    s32 armed;      /* -1 until the variable is read, 0 when unset or spent */
    u32 tick;
    u32 value;
} PlayOnce;

/* 1 on the pass ONCE is due, after which it is spent. */
static s32 playOnceDue(PlayOnce *once)
{
    if (!once->armed)
    {
        return 0;
    }
    if (once->armed < 0)
    {
        const char *e = getenv(once->variable);
        const s32 fields = (e != NULL) ? sscanf(e, "%u:%u", &once->tick, &once->value) : 0;

        once->armed = 0;
        if (e == NULL || e[0] == '\0')
        {
            return 0;
        }
        if (once->wantsValue ? (fields != 2 || once->value == 0) : fields != 1)
        {
            fprintf(stderr, "ge: %s=%s ignored: it takes %s\n", once->variable, e,
                    once->wantsValue ? "TICK:VALUE, VALUE above 0" : "TICK");
            return 0;
        }
        once->armed = 1;
    }
    if (portInputRecTickNow() < once->tick)
    {
        return 0;
    }
    once->armed = 0;
    return 1;
}

/* GE_PLAYSTALL and GE_PLAYHURT (port_playlog.h). */
static void playOnceActions(void)
{
    static PlayOnce stall = { "GE_PLAYSTALL", 1, -1, 0, 0 };
    static PlayOnce hurt = { "GE_PLAYHURT", 0, -1, 0, 0 };

    if (playOnceDue(&stall))
    {
        portPlaySleepMs(stall.value);
    }
    if (playOnceDue(&hurt))
    {
        record_damage_kills(PORT_PLAY_HURT_DAMAGE, 0.0f, 0.0f, -1, 0);
    }
}

/* `shots`, `moved` and `weapon` (port_playlog.h), from player 0's own counters. */
static void playStats(void)
{
    static s32 shots;
    static s32 moved;
    s32 now;
    s32 changed;

    if (!playLogLevel() || g_playerPerm == NULL)
    {
        return;
    }
    changed = playWeapon();
    if (sPlayLogLevel >= 2 && (changed || joyGetButtons(0, Z_TRIG)))
    {
        portPlayGunLine(portInputRecTickNow());
    }
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
    playOnceActions();
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
