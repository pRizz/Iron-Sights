#ifndef MIDAS_WEB_AUDIO_OUTPUT_H
#define MIDAS_WEB_AUDIO_OUTPUT_H

#include <SDL.h>

/* SDL's queued-audio calls, backed by the render-thread queue in audio_output.js.
 * Without AudioWorklet they fall through to SDL's own device. */
SDL_AudioDeviceID portWebAudioOpen(const char *device, int capture, const SDL_AudioSpec *want,
                                   SDL_AudioSpec *have, int allowedChanges);
void portWebAudioClose(SDL_AudioDeviceID device);
void portWebAudioPause(SDL_AudioDeviceID device, int paused);
int portWebAudioQueue(SDL_AudioDeviceID device, const void *data, Uint32 bytes);
Uint32 portWebAudioQueued(SDL_AudioDeviceID device);
void portWebAudioClear(SDL_AudioDeviceID device);
int portWebAudioUnderruns(void);
int portWebAudioOnRenderThread(void);

#endif
