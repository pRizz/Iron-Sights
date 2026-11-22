/**
 * The simulation trace (LP64-006 Task 3): one line of game state a tick, in
 * its own file, so two runs and two builds can be compared tick by tick
 * (scripts/port/lp64_equiv_streams.py finds the first difference).
 *
 * lvlTick (lv.c) calls portSimTraceTick last. With GE_SIMTRACE=FILE it writes
 * FILE afresh, one line a call, and nothing to the log:
 *
 *   T n=N timer=G seed=S16 draws=D players=P pI=X,Y,Z,FACING,HEALTH ...
 *     chrs=C:H8 props=R:H8 rooms=L:H8 [demo=K]
 *
 * N counts calls from 1; G is g_GlobalTimer; S16 the random seed as 16 hex
 * digits; D the draws since the last line (g_portRandDrawCount); P the
 * players in g_playerPointers, each written as pI with I its slot: its prop's
 * position, vv_theta and bondhealth. A float is always its bits as eight hex
 * digits. Each count:hash is the records' count and a 32-bit FNV-1a hash over
 * their values, each value four bytes from the lowest:
 * - chrs: each occupied guard slot of g_ChrSlots (model set), in slot order:
 *   slot, actiontype, chrflags, prop position bits, damage bits;
 * - props: the active prop list from g_ActivePropsHead, in list order: list
 *   index, type, flags, position bits;
 * - rooms: each loaded room (model_bin_loaded set), in room order from 1:
 *   room number and model_bin_loaded (which also holds the unload age).
 * demo=K ends the line only while a recording replays (LP64-006 Task 5): K is
 * the recording's tick, the one `ge: demo tick` prints (ramromreplay.c's
 * port_demo_ticks, through portRamromReplayTick). A demo is paced, so its timer and tick count are the
 * host's; lp64_equiv_streams.py's `sim-demo` compares a demo by K instead.
 * Outside a replay the line has no such field.
 * No pointer, time, path or other host value reaches the file; a list is
 * walked in the game's own order, never one that follows an address.
 *
 * GE_SIMTRACE_DETAIL=A-B adds, after the T line of each call A to B
 * (inclusive), one line a guard and one a moving prop (one that was not on
 * the active list at the last call, or whose position bits changed):
 *
 *   G n=N slot=I chrnum=C action=A flags=F8 pos=X,Y,Z damage=H
 *   M n=N index=I slot=S type=T flags=F2 pos=X,Y,Z
 *
 * where S is the prop's index in g_Props. A FILE that cannot be opened or
 * written stops the run with `ge: FATAL: GE_SIMTRACE ...` and exit status 1;
 * a GE_SIMTRACE_DETAIL that does not read as A-B is reported on stderr and
 * ignored. The trace reads game state only: it writes none, draws no random
 * number and changes no order.
 *
 * The trace compiles where PORT_PLAY_HARNESS is 1 (port_playlog.h); in a
 * shipped 32-bit build with video, portSimTraceTick expands to nothing and
 * CMake leaves port_simtrace.c out.
 */
#ifndef PORT_SIMTRACE_H
#define PORT_SIMTRACE_H

#include <stddef.h>
#include <PR/ultratypes.h>
#include "port_playlog.h"

#if PORT_PLAY_HARNESS
/** The longest line portSimTraceFormat writes, newline and terminator included. */
#define PORT_SIMTRACE_LINE_MAX 512

/** src/game/ramromreplay.c: the playing recording's tick (`ge: demo tick`'s), -1 when none plays. */
s32 portRamromReplayTick(void);
/** lvlTick's last call: with GE_SIMTRACE set, writes call N's lines. */
void portSimTraceTick(void);
/**
 * Writes the T line of call N with DRAWS draws, from the game's state now,
 * newline included, to OUT. Returns its length, or -1 if SIZE is too small.
 */
s32 portSimTraceFormat(char *out, size_t size, u32 n, s32 draws);
#else
#define portSimTraceTick() ((void) 0)
#endif

#endif
