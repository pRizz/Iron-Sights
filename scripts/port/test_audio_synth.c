/* The synthesizer and the interpreter together, with no ROM (LP64-005 Task 10).
 *
 * The bank is built here from invented numbers: one raw 16-bit wave of 2,000
 * samples of a square wave, one sound and one instrument. One voice is started
 * through alSynAllocVoice, alSynStartVoiceParams and alSynSetVol, and the
 * frames alAudioFrame builds are run by the port's interpreter (audio_pc.c,
 * interpreter only). At 64 bits the parameter pool once held records of
 * sizeof(ALParam), into which alSynStartVoiceParams writes a larger
 * ALStartParamAlt whose wave pointer lands on the next record's link. */
#define PORT_AUDIO_INTERPRETER_ONLY
#include <string.h> /* before the interpreter, which calls memset and memcpy */
#include "port/src/audio_pc.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "libultra/audio/synthInternals.h"
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

/* include/assert.h reports a failed assert through osSyncPrintf. */
void osSyncPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    abort();
}
/* The interpreter's warnings name the command it refused. */
void portConLogf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
void ge_dbg_vec(const char *tag, const f32 *values, s32 count) { (void)tag; (void)values; (void)count; }
/* The game's own answers to what the synthesizer calls outside its sources. */
OSIntMask osSetIntMask(OSIntMask mask) { return mask; }
uintptr_t osVirtualToPhysical(void *address) { return (uintptr_t)address; }
void portLp64PoolFatal(const char *pool, u32 request)
{
    fprintf(stderr, "test_audio_synth: the %s cannot give %u bytes\n", pool, request);
    exit(1);
}

enum {
    WAVE_SAMPLES = 2000,
    SQUARE_HALF_PERIOD = 50,
    SQUARE_LEVEL = 12000,
    FRAME_SAMPLES = 368, /* audi.c's frame at 22,050 Hz and 60 fields */
    FRAMES = 10,
    MAX_UPDATES = 64,
    NOTE_CYCLES = 64,
    /* An update posted outside a client's callback is stamped with the time of
     * the next callback, up to the idle client's second ahead; 70 frames are
     * 25,760 samples, more than a second at 22,050 Hz, so every update posted
     * before them has been applied and given back. */
    FLUSH_FRAMES = 70,
    HEAP_BYTES = 256 * 1024,
};

/* What the interpreter reads or writes by address (the heap with the filters'
 * states, the samples, the output) lies in one mapped block: a 32-bit image
 * under emulation sits near 0x400000, below the 0x01000000 the interpreter's
 * validator takes for the lowest real address. */
typedef struct {
    u8 heap[HEAP_BYTES] __attribute__((aligned(16)));
    s16 samples[WAVE_SAMPLES] __attribute__((aligned(16)));
    s16 output[FRAME_SAMPLES * 2] __attribute__((aligned(16)));
} AddressedMemory;
static AddressedMemory *s_Memory;
static ALGlobals s_Globals;
static ALWaveTable s_Wave;
static ALEnvelope s_Envelope;
static ALKeyMap s_KeyMap;
static ALSound s_Sound;
static ALInstrument s_Instrument;
static Acmd s_Commands[3072];
/* The parameter records the pool starts with, to tell a record from a stray link. */
static ALParam *s_PoolRecords[MAX_UPDATES];

/* The port's DMA answer (audi.c's amDmaCallback): a wave base is already the data. */
static intptr_t sampleAddress(intptr_t addr, s32 len, void *state)
{
    (void)len; (void)state;
    return addr;
}

static ALDMAproc newDma(void *state)
{
    *(void **)state = NULL;
    return sampleAddress;
}

static ALMicroTime idleClient(void *client)
{
    (void)client;
    return 1000000; /* nothing to do for a second */
}

static void mapMemory(void)
{
    void *block = mmap(NULL, sizeof(AddressedMemory), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    assert(block != MAP_FAILED);
    assert((uintptr_t)block >= 0x01000000u);
    s_Memory = block;
}

static void buildBank(void)
{
    for (s32 i = 0; i < WAVE_SAMPLES; i++) {
        s_Memory->samples[i] = (i / SQUARE_HALF_PERIOD) % 2 == 0 ? SQUARE_LEVEL : -SQUARE_LEVEL;
    }
    s_Wave = (ALWaveTable){.base = (u8 *)s_Memory->samples, .len = (s32)sizeof(s_Memory->samples), .type = AL_RAW16_WAVE};
    s_Wave.waveInfo.rawWave.loop = NULL;
    s_Envelope = (ALEnvelope){.attackTime = 0, .decayTime = 1000000, .releaseTime = 0, .attackVolume = 127,
                              .decayVolume = 127};
    s_KeyMap = (ALKeyMap){.velocityMin = 0, .velocityMax = 127, .keyMin = 0, .keyMax = 127, .keyBase = 60};
    s_Sound = (ALSound){.envelope = &s_Envelope, .keyMap = &s_KeyMap, .wavetable = &s_Wave,
                        .samplePan = AL_PAN_CENTER, .sampleVolume = 127};
    s_Instrument = (ALInstrument){.volume = 127, .pan = AL_PAN_CENTER, .priority = 5, .soundCount = 1};
    s_Instrument.soundArray[0] = &s_Sound;
}

static ALSynth *startSynth(ALPlayer *client, ALFxId effect)
{
    static ALHeap heap;
    ALSynConfig config = {.maxVVoices = 4, .maxPVoices = 4, .maxUpdates = MAX_UPDATES, .maxFXbusses = 1,
                          .dmaproc = (void *)newDma, .heap = &heap, .outputRate = PORT_AUDIO_RATE,
                          .fxType = effect};
    ALParam *record = NULL;

    memset(s_Memory->heap, 0, sizeof(s_Memory->heap));
    alHeapInit(&heap, s_Memory->heap, sizeof(s_Memory->heap));
    alGlobals = NULL;
    alInit(&s_Globals, &config);
    *client = (ALPlayer){.handler = idleClient};
    alSynAddPlayer(&alGlobals->drvr, client);
    record = alGlobals->drvr.paramList;
    for (s32 i = 0; i < MAX_UPDATES; i++, record = record->next) {
        assert(record != NULL);
        s_PoolRecords[i] = record;
    }
    assert(record == NULL);
    buildBank();
    memset(s_Dmem, 0, sizeof(s_Dmem));
    return &alGlobals->drvr;
}

static int isPoolRecord(const ALParam *record)
{
    for (s32 i = 0; i < MAX_UPDATES; i++) {
        if (s_PoolRecords[i] == record) {
            return 1;
        }
    }
    return 0;
}

/* The free list's length, counting only the pool's own records; -1 when a
 * link leaves the pool (a record written past its end). */
static s32 freeParamCount(const ALSynth *synth)
{
    s32 count = 0;

    for (const ALParam *record = synth->paramList; record != NULL; record = record->next) {
        if (!isPoolRecord(record) || count == MAX_UPDATES) {
            return -1;
        }
        count++;
    }
    return count;
}

static void startNote(ALSynth *synth, ALVoice *voice)
{
    ALVoiceConfig config = {.priority = s_Instrument.priority, .fxBus = 0, .unityPitch = 0};
    const ALSound *sound = s_Instrument.soundArray[0];
    s32 allocated = alSynAllocVoice(synth, voice, &config);

    assert(allocated);
    alSynStartVoiceParams(synth, voice, sound->wavetable, 1.0f, 0x7FFF, sound->samplePan, 0, 0);
    alSynSetVol(synth, voice, 0x7FFF, 0);
}

/* One frame built and run; returns the frames the interpreter skipped. */
static u32 runFrame(void)
{
    s32 count = 0;
    u32 skipped = s_StatSkipped;

    alAudioFrame(s_Commands, &count, s_Memory->output, FRAME_SAMPLES);
    assert(count > 0 && count <= (s32)(sizeof(s_Commands) / sizeof(s_Commands[0])));
    portAcmdRun(s_Commands, count);
    return s_StatSkipped - skipped;
}

static void noFrameIsSkipped(void)
{
    /* Arrange */
    ALPlayer client;
    ALVoice voice;
    ALSynth *synth = startSynth(&client, AL_FX_NONE);
    u32 skipped = 0;
    startNote(synth, &voice);
    /* Act */
    for (s32 frame = 0; frame < FRAMES; frame++) {
        skipped += runFrame();
    }
    /* Assert */
    if (skipped != 0) {
        fprintf(stderr, "test_audio_synth: the interpreter skipped %u of %d frames\n", skipped, FRAMES);
        exit(1);
    }
}

/* The reverb (reverb.c) reads its delay line at taps behind its write
 * pointer, `&r->input[-d->output]` with an unsigned tap: at 64 bits the
 * negated tap is a 32-bit number added to a 64-bit pointer, 8 GB ahead. */
static void framesWithAnEffectAreNotSkipped(ALFxId effect, const char *name)
{
    /* Arrange */
    ALPlayer client;
    ALVoice voice;
    ALSynth *synth = startSynth(&client, effect);
    u32 skipped = 0;
    startNote(synth, &voice);
    /* Act */
    for (s32 frame = 0; frame < FRAMES; frame++) {
        skipped += runFrame();
    }
    /* Assert */
    if (skipped != 0) {
        fprintf(stderr, "test_audio_synth: with the %s effect the interpreter skipped %u of %d frames\n", name,
                skipped, FRAMES);
        exit(1);
    }
}

/* The small room's taps go through alFxPull and _loadOutputBuffer's plain read. */
static void noFrameIsSkippedWithTheRoomEffect(void) { framesWithAnEffectAreNotSkipped(AL_FX_SMALLROOM, "small room"); }

/* The chorus modulates its output tap, _loadOutputBuffer's resampled read. */
static void noFrameIsSkippedWithTheChorusEffect(void) { framesWithAnEffectAreNotSkipped(AL_FX_CHORUS, "chorus"); }

static void theOutputIsNotSilence(void)
{
    /* Arrange */
    ALPlayer client;
    ALVoice voice;
    ALSynth *synth = startSynth(&client, AL_FX_NONE);
    s32 peak = 0;
    startNote(synth, &voice);
    /* Act */
    for (s32 frame = 0; frame < FRAMES; frame++) {
        memset(s_Memory->output, 0, sizeof(s_Memory->output));
        runFrame();
        for (s32 i = 0; i < FRAME_SAMPLES * 2; i++) {
            s32 level = s_Memory->output[i] < 0 ? -s_Memory->output[i] : s_Memory->output[i];
            peak = level > peak ? level : peak;
        }
    }
    /* Assert */
    if (peak == 0) {
        fprintf(stderr, "test_audio_synth: %d frames of a square wave played as silence\n", FRAMES);
        exit(1);
    }
}

static void noteStartsAndStopsGiveBackEveryParameter(void)
{
    /* Arrange */
    ALPlayer client;
    ALVoice voice;
    ALSynth *synth = startSynth(&client, AL_FX_NONE);
    s32 before = freeParamCount(synth);
    s32 after;
    /* Act */
    for (s32 note = 0; note < NOTE_CYCLES; note++) {
        startNote(synth, &voice);
        runFrame();
        alSynStopVoice(synth, &voice);
        alSynFreeVoice(synth, &voice);
        runFrame();
    }
    for (s32 frame = 0; frame < FLUSH_FRAMES; frame++) {
        runFrame();
    }
    after = freeParamCount(synth);
    /* Assert */
    assert(before == MAX_UPDATES);
    if (after != before) {
        fprintf(stderr, "test_audio_synth: after %d note starts and stops the parameter free list holds %d of %d"
                        " records (-1: a link leaves the pool)\n", NOTE_CYCLES, after, before);
        exit(1);
    }
}

/* Runs TEST in a child, so a record written past its room fails by name
 * instead of ending the harness wherever the stray write lands. */
static int runTest(const char *name, void (*test)(void))
{
    int status = 0;
    pid_t child;

    fflush(NULL);
    /* Checked without assert, so a build with NDEBUG still forks and waits. */
    child = fork();
    if (child < 0) {
        perror("test_audio_synth: fork");
        exit(2);
    }
    if (child == 0) {
        test();
        exit(0);
    }
    if (waitpid(child, &status, 0) != child) {
        perror("test_audio_synth: waitpid");
        exit(2);
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 1;
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "test_audio_synth: %s: killed by signal %d\n", name, WTERMSIG(status));
    } else {
        fprintf(stderr, "test_audio_synth: %s: exit status %d\n", name, WEXITSTATUS(status));
    }
    return 0;
}

int main(void)
{
    mapMemory();
    int passed = runTest("noFrameIsSkipped", noFrameIsSkipped);
    passed &= runTest("noFrameIsSkippedWithTheRoomEffect", noFrameIsSkippedWithTheRoomEffect);
    passed &= runTest("noFrameIsSkippedWithTheChorusEffect", noFrameIsSkippedWithTheChorusEffect);
    passed &= runTest("theOutputIsNotSilence", theOutputIsNotSilence);
    passed &= runTest("noteStartsAndStopsGiveBackEveryParameter", noteStartsAndStopsGiveBackEveryParameter);
    return passed ? 0 : 1;
}
