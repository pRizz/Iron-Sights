#ifndef MIDAS_WEB_AUDIO_CONFIG_H
#define MIDAS_WEB_AUDIO_CONFIG_H

/* SDL's browser backend runs ScriptProcessor on the game/render thread at the
 * AudioContext rate (often 48 kHz), not the 22050 Hz synthesizer rate. A 256
 * sample period gives only 5.3 ms to service audio. Restore the pre-1.0.2b web
 * period while leaving upstream's native low-latency device setting intact. */
#define PORT_AUDIO_DEV_SAMPLES 512

#endif
