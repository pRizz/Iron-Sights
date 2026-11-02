#ifndef MIDAS_WEB_AUDIO_CONFIG_H
#define MIDAS_WEB_AUDIO_CONFIG_H

/* The fallback SDL backend runs ScriptProcessor on the game/render thread at the
 * AudioContext rate (often 48 kHz), not the 22050 Hz synthesizer rate. A 256
 * sample period gives only 5.3 ms to service audio. Restore the pre-1.0.2b web
 * period while leaving upstream's native low-latency device setting intact. */
#define PORT_AUDIO_DEV_SAMPLES 512

/* The queue plays on the audio thread, so its depth is how long the game thread
 * can stall (at 88200 bytes/s). Gameplay keeps sound effects prompt; menus and
 * cutscenes, where level loads happen, hold more. Levels stay at the target
 * rather than half of it, so the margin is the same after every top-up. */
#define PORT_AUDIO_TARGET      7056      /* 80 ms in gameplay */
#define PORT_AUDIO_TARGET_IDLE 17640     /* 200 ms outside gameplay */
#define PORT_AUDIO_TARGET_MAX  (12 * 1024)
#define PORT_AUDIO_QUEUE_MAX   (48 * 1024)
#define PORT_AUDIO_FLOOR_DIV   1

#ifndef PORT_AUDIO_INTERPRETER_ONLY
#include "audio_output.h"
#define SDL_OpenAudioDevice portWebAudioOpen
#define SDL_CloseAudioDevice portWebAudioClose
#define SDL_PauseAudioDevice portWebAudioPause
#define SDL_QueueAudio portWebAudioQueue
#define SDL_GetQueuedAudioSize portWebAudioQueued
#define SDL_ClearQueuedAudio portWebAudioClear
#endif

#endif
