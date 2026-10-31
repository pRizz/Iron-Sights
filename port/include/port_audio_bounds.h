/* Preflight the interpreter's DMEM spans before executing an audio frame. */
#ifndef PORT_AUDIO_BOUNDS_H
#define PORT_AUDIO_BOUNDS_H
#include <ultra64.h>
#include <PR/abi.h>

typedef struct {
    u32 in, out, count, mainR, auxL, auxR;
} PortAudioBuffers;

static int portAudioSpan(u32 offset, u32 bytes, u32 capacity)
{
    return offset <= capacity && bytes <= capacity - offset;
}

static int portAudioCommandFits(PortAudioBuffers *b, u32 w0, u32 w1)
{
    const u32 op = w0 >> 24, flags = (w0 >> 16) & 255;
    const u32 input = b->in & 0xffe, output = b->out & 0xffe;
    const u32 samples = b->count & ~1u;
    switch (op) {
    case A_SETBUFF:
        if (flags & A_AUX) { b->mainR = w0 & 65535; b->auxL = w1 >> 16; b->auxR = w1 & 65535; }
        else { b->in = w0 & 65535; b->out = w1 >> 16; b->count = w1 & 65535; }
        return 1;
    case A_CLEARBUFF:
        return portAudioSpan(w0 & 0xffffff, w1, 4096);
    case A_LOADBUFF:
        return portAudioSpan(b->in & 4095, (b->count + 7) & ~7u, 4096 + 64);
    case A_SAVEBUFF:
        return portAudioSpan(b->out & 4095, b->count, 4096);
    case A_DMEMMOVE:
        return portAudioSpan(w0 & 4095, w1 & 65535, 4096)
            && portAudioSpan((w1 >> 16) & 4095, w1 & 65535, 4096);
    case A_LOADADPCM:
        return (w0 & 0xffffff) <= 128 * sizeof(s16);
    case A_ADPCM: {
        const u32 rounded = (b->count + 31) & ~31u;
        return portAudioSpan(b->in & 4095, rounded / 32 * 9, 4096)
            && portAudioSpan(output, rounded + 32, 4096);
    }
    case A_RESAMPLE:
        return input >= ((flags & 2) ? 16u : 8u)
            && portAudioSpan(output, (b->count + 15) & ~15u, 4096);
    case A_ENVMIXER:
        return portAudioSpan(input, samples, 4096) && portAudioSpan(output, samples, 4096)
            && portAudioSpan(b->mainR & 0xffe, samples, 4096)
            && (!(flags & A_AUX) || (portAudioSpan(b->auxL & 0xffe, samples, 4096)
                && portAudioSpan(b->auxR & 0xffe, samples, 4096)));
    case A_MIXER:
        return portAudioSpan((w1 >> 16) & 0xffe, (b->count + 15) & ~15u, 4096)
            && portAudioSpan(w1 & 0xffe, (b->count + 15) & ~15u, 4096);
    case A_INTERLEAVE:
        return portAudioSpan((w1 >> 16) & 0xffe, samples, 4096)
            && portAudioSpan(w1 & 0xffe, samples, 4096) && portAudioSpan(output, samples * 2, 4096);
    case A_POLEF:
        return portAudioSpan(input, samples, 4096);
    default:
        return 1;
    }
}
#endif
