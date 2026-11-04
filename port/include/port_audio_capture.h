#ifndef PORT_AUDIO_CAPTURE_H
#define PORT_AUDIO_CAPTURE_H
#include <ultra64.h>

/* GE_AUDIO_CAPTURE=<path.wav> (test harness): record the mixed output,
 * 22050 Hz stereo, before the unfocused-window mute. */
void portAudioCaptureFrame(const s16 *stereo, u32 frames);
/* Stereo frames recorded so far; a harness logs it to locate events. */
u32 portAudioCaptureFrames(void);

#endif
