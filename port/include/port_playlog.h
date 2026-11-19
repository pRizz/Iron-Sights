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
 * GE_PAUSEDBG line says so) and, from portPlayPlayerTick, `kill`, `shots`
 * and `moved`.
 *
 * portPlayPlayerTick, from lv.c's frame loop over the players, reads player
 * 0's record only (the game's own counters, never changed here):
 * - `shots N` each time player 0's total shot count (shot_count's
 *   SHOT_REGISTER_TOTAL, player.h) changes, N the new count;
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
 *   some runs and not others.
 *
 * PORT_PLAY_HARNESS is 1 in every build the play sets run (headless at
 * either width, and a 64-bit build with video) and 0 in a 32-bit build with
 * video, the shipped kind: there every call expands to nothing and the
 * harness's other addition (GE_MENU=briefing, front.c) is not compiled, so a
 * shipped executable is unchanged.
 */
#ifndef PORT_PLAYLOG_H
#define PORT_PLAYLOG_H

#include <PR/ultratypes.h>

#if !defined(GE_VIDEO) || __SIZEOF_POINTER__ == 8
#define PORT_PLAY_HARNESS 1
#define PORT_PLAY_MOVED_UNITS 100
#define PORT_PLAY_WATCH_STEADY_DRAWS 30
void portPlayLog(const char *event, s32 value);
void portPlayPlayerTick(void);
void portPlayWatchPage(s32 page);
#else
#define PORT_PLAY_HARNESS 0
#define portPlayLog(event, value) ((void) 0)
#define portPlayPlayerTick() ((void) 0)
#define portPlayWatchPage(page) ((void) 0)
#endif

#endif
