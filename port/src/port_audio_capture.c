/* Mixed-output WAV capture for scripted audio checks (FORK-043). */
#include <stdio.h>
#include <stdlib.h>
#include "port_audio_capture.h"

#define CAPTURE_RATE 22050

static FILE *captureFile;
static u32 captureFrames;
static s32 captureState; /* 0 unread, 1 recording, -1 off */

static void captureU16(unsigned value)
{
    fputc(value & 255, captureFile);
    fputc((value >> 8) & 255, captureFile);
}

static void captureU32(unsigned value)
{
    captureU16(value & 0xFFFF);
    captureU16(value >> 16);
}

/* Rewritten after every frame, so a killed run still leaves a valid file. */
static void captureHeader(void)
{
    fseek(captureFile, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, captureFile);
    captureU32(36 + captureFrames * 4);
    fwrite("WAVEfmt ", 1, 8, captureFile);
    captureU32(16);
    captureU16(1);
    captureU16(2);
    captureU32(CAPTURE_RATE);
    captureU32(CAPTURE_RATE * 4);
    captureU16(4);
    captureU16(16);
    fwrite("data", 1, 4, captureFile);
    captureU32(captureFrames * 4);
    fseek(captureFile, 0, SEEK_END);
}

static void captureClose(void)
{
    if (captureFile != NULL) {
        fclose(captureFile);
        captureFile = NULL;
    }
}

static s32 captureOpen(void)
{
    const char *path = getenv("GE_AUDIO_CAPTURE");

    if (path == NULL || path[0] == '\0') return 0;
    captureFile = fopen(path, "wb");
    if (captureFile == NULL) {
        fprintf(stderr, "ge: audio capture: cannot open %s\n", path);
        return 0;
    }
    captureHeader();
    atexit(captureClose);
    fprintf(stderr, "ge: audio capture: recording %s\n", path);
    return 1;
}

void portAudioCaptureFrame(const s16 *stereo, u32 frames)
{
    if (captureState == 0) captureState = captureOpen() ? 1 : -1;
    if (captureState < 0 || captureFile == NULL) return;
    /* The mixer writes native-endian samples; WAV is little-endian like
     * every desktop target this harness runs on. */
    fwrite(stereo, 4, frames, captureFile);
    captureFrames += frames;
    captureHeader();
}

u32 portAudioCaptureFrames(void)
{
    return captureFrames;
}
