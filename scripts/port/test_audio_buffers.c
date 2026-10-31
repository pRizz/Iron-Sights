/* Test the real interpreter, including diagnostic rejection of short lists. */
#define PORT_AUDIO_INTERPRETER_ONLY
#include "port/src/audio_pc.c"
#include "test_buffer_helpers.h"
void ge_dbg_vec(const char *tag, const f32 *values, s32 count) { (void)tag; (void)values; (void)count; }

static void rejected(Acmd *commands, s32 count)
{
    u8 before[sizeof(s_Dmem)]; memcpy(before, s_Dmem, sizeof(before));
    portAcmdRun(commands, count);
    assert(!memcmp(before, s_Dmem, sizeof(before)));
}

int main(void)
{
    /* Arrange: edge addresses and a short command list with no spare records. */
    Acmd commands[2];
    memset(s_Dmem, 0x55, sizeof(s_Dmem));
    aDMEMMove(commands, 4090, 4090, 128);
    /* Act/assert: reject the complete frame before an out-of-range copy. */
    rejected(commands, 1);
    commands[0].words.w0 = A_LOADBUFF << 24; commands[0].words.w1 = 0;
    rejected(commands, 1); /* Diagnostic output must not read records 1..6. */
    aClearBuffer(commands, 4095, 0xffffffffu); rejected(commands, 1);

    /* Arrange/act/assert: valid copies/clears preserve interpreter semantics. */
    aClearBuffer(commands, 32, 16); portAcmdRun(commands, 1);
    for (s32 i = 32; i < 48; i++) assert(s_Dmem[i] == 0);
    aDMEMMove(commands, 0, 4090, 6); portAcmdRun(commands, 1);
    for (s32 i = 4090; i < 4096; i++) assert(s_Dmem[i] == 0x55);

    /* Act/assert: every command family's derived spans have edge coverage. */
    PortAudioBuffers b = {4090, 4090, 128, 4090, 4090, 4090};
    const u32 ops[] = {A_LOADBUFF, A_SAVEBUFF, A_ADPCM, A_RESAMPLE, A_ENVMIXER, A_MIXER, A_INTERLEAVE, A_POLEF};
    for (u32 i = 0; i < sizeof(ops)/sizeof(ops[0]); i++) {
        u32 w1 = ops[i] == A_MIXER || ops[i] == A_INTERLEAVE ? 0x0ffa0ffa : 0;
        assert(!portAudioCommandFits(&b, (ops[i] << 24) | (A_AUX << 16), w1));
    }
    assert(!portAudioCommandFits(&b, (A_LOADADPCM << 24) | 257, 0));

    /* Malformed ADPCM predictors and resampler history cannot escape DMEM. */
    s16 state[16] = {0};
    s_BufIn = 32; s_BufOut = 128; s_BufCount = 32;
    s_Dmem[32] = 15;
    portAcmdAdpcm(A_INIT, state);
    s_BufIn = 16; state[5] = 32767;
    portAcmdResample(2, 65535, state);
    puts("Audio buffer and short-list checks passed");
    return 0;
}
