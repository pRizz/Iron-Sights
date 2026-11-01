/* Offline, ROM-backed music rendering. Set GE_MUSIC_EXPORT=dam|facility and
 * GE_MUSIC_EXPORT_WAV=<path>; run from an isolated game build/runtime. */
#if defined(GE_VIDEO) && !defined(__EMSCRIPTEN__)
#include <PR/libaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "bondconstants.h"
#include "music.h"

extern ALCSeq g_musicXTrack1Seq;
extern s32 portAudioRenderOffline(s16 *output, s32 samples);

static void musicExportU16(FILE *file, unsigned value)
{
    fputc(value & 255, file);
    fputc((value >> 8) & 255, file);
}

static void musicExportU32(FILE *file, unsigned value)
{
    musicExportU16(file, value);
    musicExportU16(file, value >> 16);
}

static void musicExportWavHeader(FILE *file, unsigned frames)
{
    rewind(file);
    fwrite("RIFF", 1, 4, file);
    musicExportU32(file, 36 + frames * 4);
    fwrite("WAVEfmt ", 1, 8, file);
    musicExportU32(file, 16);
    musicExportU16(file, 1);
    musicExportU16(file, 2);
    musicExportU32(file, 22050);
    musicExportU32(file, 22050 * 4);
    musicExportU16(file, 4);
    musicExportU16(file, 16);
    fwrite("data", 1, 4, file);
    musicExportU32(file, frames * 4);
}

static int musicExportTrack(const char *name)
{
    if (strcmp(name, "dam") == 0) return M_DAM;
    if (strcmp(name, "facility") == 0) return M_FACILITY;
    if (strcmp(name, "damx") == 0) return M_DAMX;
    if (strcmp(name, "facilityx") == 0) return M_FACILITYX;
    if (strcmp(name, "wind") == 0) return M_WIND;
    return -1;
}

static int musicExportReplace(const char *partial, const char *path)
{
#ifdef _WIN32
    return MoveFileExA(partial, path, MOVEFILE_REPLACE_EXISTING) ? 0 : 1;
#else
    return rename(partial, path);
#endif
}

static int musicExportRender(const char *path, int track)
{
    enum { SAMPLES = 368, MAX_SECONDS = 600 };
    s16 output[SAMPLES * 2];
    u8 *before[16];
    u32 pending;
    unsigned channelCount = 0;
    unsigned frames = 0;
    unsigned i;
    int complete = 0;
    char *partial = malloc(strlen(path) + 6);
    FILE *file;

    if (partial == NULL) return 1;
    sprintf(partial, "%s.part", path);
    file = fopen(partial, "wb");
    if (file == NULL) {
        fprintf(stderr, "ge: music export: cannot open %s\n", path);
        free(partial);
        return 1;
    }
    musicExportWavHeader(file, 0);
    musicTrack1Play(track);
    pending = g_musicXTrack1Seq.validTracks;
    if (pending == 0) {
        fprintf(stderr, "ge: music export: track %d has no sequence channels\n", track);
        goto fail;
    }
    fprintf(stderr, "ge: music export: track=%d channels=0x%04x rate=22050\n",
            track, pending);
    for (i = 0; i < 16; i++) {
        if (pending & (1u << i)) channelCount++;
    }

    while (frames < 22050u * MAX_SECONDS) {
        int rendered;
        unsigned wrapped = 0;

        for (i = 0; i < 16; i++) before[i] = g_musicXTrack1Seq.curLoc[i];
        rendered = portAudioRenderOffline(output, SAMPLES);
        if (rendered != SAMPLES ||
            fwrite(output, sizeof(output), 1, file) != 1) {
            fprintf(stderr, "ge: music export: rendering/write failed at %.1f s\n",
                    (double)frames / 22050.0);
            goto fail;
        }
        frames += SAMPLES;
        for (i = 0; i < 16; i++) {
            u32 bit = 1u << i;
            int didWrap;

            if (!(pending & bit)) continue;
            didWrap = before[i] != NULL &&
                g_musicXTrack1Seq.curLoc[i] < before[i];
            if (didWrap) wrapped++;
            if (!(g_musicXTrack1Seq.validTracks & bit) || didWrap) {
                pending &= ~bit;
                fprintf(stderr, "ge: music export: channel %u completed at %.2f s\n",
                        i, (double)frames / 22050.0);
            }
        }
        /* A few channels may hold ambience without a matching loop marker.
         * A synchronized repeat in at least 3/4 of the original channels is
         * the full theme's first repeat; individual earlier loops are ignored. */
        if (pending == 0 || wrapped * 4 >= channelCount * 3) {
            complete = 1;
            break;
        }
        if (frames % (SAMPLES * 1800) == 0) {
            fprintf(stderr, "ge: music export: %.0f s, pending=0x%04x\n",
                    (double)frames / 22050.0, pending);
        }
    }
    if (!complete) {
        fprintf(stderr, "ge: music export: no complete first pass within %d s\n",
                MAX_SECONDS);
        goto fail;
    }
    musicExportWavHeader(file, frames);
    if (fclose(file) != 0) {
        fprintf(stderr, "ge: music export: failed to close %s\n", path);
        file = NULL;
        goto fail;
    }
    file = NULL;
    if (musicExportReplace(partial, path) != 0) {
        fprintf(stderr, "ge: music export: failed to replace %s\n", path);
        goto fail;
    }
    free(partial);
    fprintf(stderr, "ge: music export: wrote %s (%.2f s, %u frames)\n",
            path, (double)frames / 22050.0, frames);
    return 0;

fail:
    if (file != NULL) fclose(file);
    remove(partial);
    free(partial);
    return 1;
}

void portMusicExportMaybe(void)
{
    const char *name = getenv("GE_MUSIC_EXPORT");
    const char *path;
    int track;

    if (name == NULL || *name == '\0') return;
    path = getenv("GE_MUSIC_EXPORT_WAV");
    track = musicExportTrack(name);
    if (path == NULL || *path == '\0' || track < 0) {
        fprintf(stderr, "ge: music export: set GE_MUSIC_EXPORT=dam|facility|damx|facilityx|wind and GE_MUSIC_EXPORT_WAV=<path>\n");
        exit(2);
    }
    exit(musicExportRender(path, track));
}
#else
void portMusicExportMaybe(void) {}
#endif
