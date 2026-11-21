/**
 * Lockstep (LP64-006 Task 2): a run that depends only on its seed, its route
 * and its tick, so two runs of one build, and runs of two builds, can be
 * compared.
 *
 * With GE_LOCKSTEP set (read once; the log then holds `ge: lockstep: on`):
 * - the pump (pcsched.c, portPumpEvents) never sleeps and delivers a retrace
 *   only to a client queue that holds none, so each wait of the game's frame
 *   queue gets exactly one retrace however often a frame pumps; GE_FRAMES
 *   counts those retraces, and a pump that delivers none (a wait inside a
 *   frame, such as joy.c's poll handshake) still runs the pump's duties;
 * - osGetTime and osGetCount (libultra_shim.c) answer from
 *   portLockstepMicroseconds instead of the host's clock;
 * - a frame advances one field (frametiming.c, portFixedTick), the tick rate
 *   is 60 whatever GE_TICKHZ or midas.ini say (portTickHz), GE_NOSLEEP has
 *   nothing left to turn off, and the render thread stays off (video.c);
 * - a demo stops the run with `ge: FATAL: GE_LOCKSTEP cannot replay a demo`,
 *   since a demo's recording paces it by the vsyncs it counted.
 *
 * Lockstep compiles where PORT_PLAY_HARNESS is 1 (port_playlog.h). Elsewhere,
 * in a shipped 32-bit build with video, portLockstep is 0 and the other calls
 * do not exist, so that executable is unchanged.
 */
#ifndef PORT_LOCKSTEP_H
#define PORT_LOCKSTEP_H

#include <PR/ultratypes.h>
#include "port_playlog.h"

#if PORT_PLAY_HARNESS
/** True when GE_LOCKSTEP is set, read once. */
s32 portLockstep(void);
/** The retraces delivered times 1,000,000 / 60, in whole microseconds. */
u64 portLockstepMicroseconds(void);
/** pcsched.c: the pump delivered one retrace. Stops the run during a demo. */
void portLockstepRetrace(void);
#else
#define portLockstep() 0
#endif

#endif
