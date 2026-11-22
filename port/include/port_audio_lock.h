/**
 * The sound lock (LP64-006 Task 9): in a comparison build, the mixed sound
 * depends only on the game, so two runs of one build, and runs of two builds,
 * record the same file.
 *
 * With GE_AUDIO_LOCK set (read once), the audio pump (audio_pc.c) mixes one
 * audio frame of the game's frame size (audi.c's g_FrameSize) for each game
 * field, and nothing by the SDL queue's level or the host's clock: no device,
 * no top-up, no trim. A frame music.c's stop waits force
 * (portAudioPumpForce) is mixed ahead of its field, and the next field's pump
 * mixes one fewer. The log then holds `ge: audio: locked to game fields`
 * once, and GE_AUDIO_CAPTURE records frame k (from 0) for field k + 1, counted
 * from the lock's first pump.
 *
 * Only a comparison build (-DGE_COMPARE=ON) compiles port_audio_lock.c.
 * Elsewhere the call below is the constant -1, so a shipped audio_pc.c takes
 * today's path. No SDL header is included, so a test can call it.
 */
#ifndef PORT_AUDIO_LOCK_H
#define PORT_AUDIO_LOCK_H

#include <PR/ultratypes.h>

#ifdef GE_COMPARE
/**
 * The audio frames due at the game field FIELD_NOW: the fields since the last
 * call, 0 on the first (and never below 0); -1 when GE_AUDIO_LOCK is unset.
 */
s32 portAudioLockFrames(s32 field_now);
#else
#define portAudioLockFrames(field_now) (-1)
#endif

#endif
