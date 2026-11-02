/* Device calls for audio_pc.c's queue; see audio_output.js for the render thread. */
#include <emscripten.h>
#include "audio_output.h"

#define WEB_AUDIO_DEVICE ((SDL_AudioDeviceID)0x7f000001u)

extern void portConLogf(const char *fmt, ...);

static int webAudioOnRenderThread;

SDL_AudioDeviceID portWebAudioOpen(const char *device, int capture, const SDL_AudioSpec *want,
                                   SDL_AudioSpec *have, int allowedChanges)
{
    webAudioOnRenderThread = EM_ASM_INT({ return Module['midasAudioOutput'].open() ? 1 : 0; });
    if (!webAudioOnRenderThread) {
        portConLogf("ge: audio: AudioWorklet is unavailable; using SDL's main-thread output\n");
        return SDL_OpenAudioDevice(device, capture, want, have, allowedChanges);
    }
    if (have != NULL) *have = *want;
    portConLogf("ge: audio: browser output runs on the audio render thread\n");
    return WEB_AUDIO_DEVICE;
}

void portWebAudioClose(SDL_AudioDeviceID device)
{
    if (device != WEB_AUDIO_DEVICE) {
        SDL_CloseAudioDevice(device);
        return;
    }
    EM_ASM({ Module['midasAudioOutput'].close(); });
}

void portWebAudioPause(SDL_AudioDeviceID device, int paused)
{
    /* The pump stops while paused, and the bridge clears this queue. */
    if (device != WEB_AUDIO_DEVICE) SDL_PauseAudioDevice(device, paused);
}

int portWebAudioQueue(SDL_AudioDeviceID device, const void *data, Uint32 bytes)
{
    if (device != WEB_AUDIO_DEVICE) return SDL_QueueAudio(device, data, bytes);
    EM_ASM({
        Module['midasAudioOutput'].push(HEAP16.subarray($0 >>> 1, ($0 + $1) >>> 1));
    }, data, bytes);
    return 0;
}

Uint32 portWebAudioQueued(SDL_AudioDeviceID device)
{
    if (device != WEB_AUDIO_DEVICE) return SDL_GetQueuedAudioSize(device);
    return (Uint32)EM_ASM_INT({ return Module['midasAudioOutput'].queuedBytes(); });
}

void portWebAudioClear(SDL_AudioDeviceID device)
{
    if (device != WEB_AUDIO_DEVICE) {
        SDL_ClearQueuedAudio(device);
        return;
    }
    EM_ASM({ Module['midasAudioOutput'].clear(); });
}

int portWebAudioUnderruns(void)
{
    return EM_ASM_INT({ return Module['midasAudioOutput'].underruns(); });
}

int portWebAudioOnRenderThread(void)
{
    return webAudioOnRenderThread;
}
