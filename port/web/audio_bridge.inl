/* Private SDL bridge: included by audio_pc.c after its pacing state. */
#ifdef __EMSCRIPTEN__
void portAudioWebPause(s32 paused)
{
    portAudioStateReset();
    if (!s_AudioDev) return;
    SDL_PauseAudioDevice(s_AudioDev, paused != 0);
    if (paused) SDL_ClearQueuedAudio(s_AudioDev);
}
/* Hold more outside gameplay, where the long stalls are; the drift trim returns
 * to the gameplay target by rendering fewer frames, so no sound is cut. */
static void portAudioWebUpdateTarget(void)
{
    extern s32 portWebControlsLive(void);
    static s32 s_WasLive;
    const s32 live = portWebControlsLive() != 0;

    if (live != s_WasLive) {
        s_WasLive = live;
        s_Target = PORT_AUDIO_TARGET;
    }
    if (!live && s_Target < PORT_AUDIO_TARGET_IDLE) {
        s_Target = PORT_AUDIO_TARGET_IDLE;
    }
}
s32 portAudioWebMetric(s32 index)
{
    switch (index) {
    case 0: return webAudioSignal;
    case 1: return s_AudioDev ? SDL_GetQueuedAudioSize(s_AudioDev) : 0;
    case 2: return webAudioFrames;
    case 3: return webAudioPumps;
    case 4: return s_AudioFails ? -1 : s_AudioDev != 0;
    case 5: return alGlobals && alGlobals->drvr.head != NULL;
    case 6: return webAudioCommands;
    case 7: return webAudioBadOp;
    case 8: return webAudioBadPointer;
    case 9: return webAudioExecuted;
    case 10: return portWebAudioUnderruns();
    case 11: return portWebAudioOnRenderThread();
    case 12: return (s32)s_Target;
    default: return 0;
    }
}
#endif
