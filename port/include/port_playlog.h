/**
 * The play log and the scripted-run harness's additions (LP64-005 Task 8).
 *
 * portPlayLog(EVENT, VALUE) prints `ge: play: tick N` and `ge: play: EVENT
 * VALUE` when GE_PLAYLOG is set, N being the input tick a GE_INPUTPLAY route
 * is replayed by (inputrec.c); otherwise it is one test of a cached flag.
 * scripts/port/lp64_play.py judges its `input` set by these lines and turns
 * a route's "wait for this event, then press" steps into ticks with them.
 * Events: `menu` (the menu id, frontChangeMenu), `stage-load` and
 * `stage-unload` (the level id, lv.c), `watch-page` (the watch tab drawn,
 * WATCH_INDEX_*, options.c: printed once a tab has been drawn
 * PORT_PLAY_WATCH_STEADY_DRAWS times in a row with the watch open and the
 * game paused, WATCH_ANIMATION_0x5, so it names the tab that stays on
 * screen; the watch reaches that state a few draws before bondview2.c's
 * GE_PAUSEDBG line says so) and, from portPlayPlayerTick, `kill`, `shots`,
 * `moved` and `weapon`.
 *
 * portPlayPlayerTick, from lv.c's frame loop over the players, reads player
 * 0's record only (the game's own counters, never changed here):
 * - `shots N` each time player 0's total shot count (shot_count's
 *   SHOT_REGISTER_TOTAL, player.h) changes, N the new count;
 * - `weapon N` each time the weapons in player 0's hands change after the
 *   first pass, N the right hand's item id | the left hand's << 8 (0 when the
 *   left hand holds nothing), so a change to the dual version of the same gun
 *   is a change; the starting pair is recorded silently, and two lines in a
 *   row never hold the same N;
 * - `moved N` once, when the distance player 0 has walked (distance_traveled,
 *   which lv.c's lvlViewMoveTick adds up) first reaches PORT_PLAY_MOVED_UNITS,
 *   N that distance in whole units;
 * - GE_PLAYKILL=N kills player 0 on his N-th pass of the loop, counting only
 *   his passes, so a two-player run dies at the same point whatever order a
 *   frame takes its players in, and logs `kill N` only when the call left
 *   him dead and he was alive before it (bondviewKillCurrentPlayer does
 *   nothing to an invincible or dead player). GE_KILLBOND (bondview2.c)
 *   counts both players' MoveBond calls and kills only when the N-th is
 *   player 0's, which with two players depends on that order and kills in
 *   some runs and not others;
 * - with GE_PLAYLOG=2 (LP64-006 Task 1), portPlayGunLine's line for player
 *   0 at each `weapon` line and on each pass while pad 0 holds Z_TRIG:
 *   `ge: play: gun tick=T clock=C global=G r=ITEM,STATE,ANIM,FIRING,HOLD,
 *   AMMO,NEXT l=...`, T the input tick, C and G g_ClockTimer and
 *   g_GlobalTimer, and each hand's weaponnum, weapon_action_state,
 *   weapon_current_animation, weapon_firing_status, weapon_hold_time,
 *   weapon_ammo_in_magazine and weapon_next_weapon (struct hand,
 *   bondview.h). GE_PLAYLOG=1 prints no gun line;
 * - GE_PLAYSTALL=T:MS sleeps MS milliseconds once, on player 0's first pass
 *   at or after input tick T, through portPlaySleepMs (weak, so a test can
 *   replace it), to make a frame miss retraces on purpose;
 * - GE_PLAYHURT=T hits player 0 once, on his first pass at or after input
 *   tick T, through the game's own record_damage_kills with
 *   PORT_PLAY_HURT_DAMAGE (the health a guard's hit took in the measured
 *   weapons-dam runs, 1.000 to 0.938), as propobj.c's hits that no player
 *   made do: the game then drops the trigger while its damage display runs
 *   (bondview2.c), which is why weapons-dam misses a shot (FORK-089,
 *   LP64-006 Task 1). A value either variable cannot read is reported on
 *   stderr and ignored.
 *
 * PORT_PLAY_HARNESS is 1 in every build the play sets run (headless at
 * either width, and a 64-bit build with video) and in every comparison build
 * (GE_COMPARE, LP64-006 Task 4, a 32-bit one with video included), and 0 in
 * a 32-bit build with video, the shipped kind: there every call expands to
 * nothing and the harness's other addition (GE_MENU=briefing, front.c) is not compiled, so a
 * shipped executable is unchanged.
 */
#ifndef PORT_PLAYLOG_H
#define PORT_PLAYLOG_H

#include <PR/ultratypes.h>

#if !defined(GE_VIDEO) || __SIZEOF_POINTER__ == 8 || defined(GE_COMPARE)
#define PORT_PLAY_HARNESS 1
#define PORT_PLAY_MOVED_UNITS 100
#define PORT_PLAY_WATCH_STEADY_DRAWS 30
#define PORT_PLAY_HURT_DAMAGE 0.0625f
void portPlayLog(const char *event, s32 value);
void portPlayPlayerTick(void);
void portPlayWatchPage(s32 page);
void portPlayGunLine(u32 tick);
void portPlaySleepMs(u32 ms);
#else
#define PORT_PLAY_HARNESS 0
#define portPlayLog(event, value) ((void) 0)
#define portPlayPlayerTick() ((void) 0)
#define portPlayWatchPage(page) ((void) 0)
#endif

#endif
