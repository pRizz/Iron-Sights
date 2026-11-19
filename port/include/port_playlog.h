/**
 * The play log and the scripted-run harness's additions (LP64-005 Task 8).
 *
 * portPlayLog(EVENT, VALUE) prints `ge: play: tick N` and `ge: play: EVENT
 * VALUE` when GE_PLAYLOG is set, N being the input tick a GE_INPUTPLAY route
 * is replayed by (inputrec.c); otherwise it is one test of a cached flag.
 * scripts/port/lp64_play.py judges its `input` set by these lines and turns
 * a route's "wait for this event, then press" steps into ticks with them.
 * Events: `menu` (the menu id, frontChangeMenu), `stage-load` and
 * `stage-unload` (the level id, lv.c), `kill` (the pass below).
 *
 * portPlayKillTick, from lv.c's frame loop over the players: GE_PLAYKILL=N
 * kills player 1 on his N-th pass of it, counting only his passes, so a
 * two-player run dies at the same point whatever order a frame takes its
 * players in. GE_KILLBOND (bondview2.c) counts both players' MoveBond calls
 * and kills only when the N-th is player 1's, which with two players depends
 * on that order and kills in some runs and not others.
 *
 * PORT_PLAY_HARNESS is 1 in every build the play sets run (headless at
 * either width, and a 64-bit build with video) and 0 in a 32-bit build with
 * video, the shipped kind: there both calls expand to nothing and the
 * harness's other addition (GE_MENU=briefing, front.c) is not compiled, so a
 * shipped executable is unchanged.
 */
#ifndef PORT_PLAYLOG_H
#define PORT_PLAYLOG_H

#include <PR/ultratypes.h>

#if !defined(GE_VIDEO) || __SIZEOF_POINTER__ == 8
#define PORT_PLAY_HARNESS 1
void portPlayLog(const char *event, s32 value);
void portPlayKillTick(void);
#else
#define PORT_PLAY_HARNESS 0
#define portPlayLog(event, value) ((void) 0)
#define portPlayKillTick() ((void) 0)
#endif

#endif
