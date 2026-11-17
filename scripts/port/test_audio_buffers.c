/* Test the real interpreter, including diagnostic rejection of short lists. */
#define PORT_AUDIO_INTERPRETER_ONLY
#include "port/src/audio_pc.c"
#include "test_buffer_helpers.h"
#include <sys/mman.h>
#include <unistd.h>
void ge_dbg_vec(const char *tag, const f32 *values, s32 count) { (void)tag; (void)values; (void)count; }

static void rejected(Acmd *commands, s32 count)
{
    u8 before[sizeof(s_Dmem)]; memcpy(before, s_Dmem, sizeof(before));
    portAcmdRun(commands, count);
    assert(!memcmp(before, s_Dmem, sizeof(before)));
}

/* A zeroed page for an address command. At 64 bits it lies above 4 GB, so a
 * command word cut to 32 bits cannot name it. It is mapped, not malloced: a
 * 32-bit heap under emulation starts near 0x400000, below the 0x01000000 the
 * validator takes for the lowest real address. */
static void *addressBlock(void)
{
    void *block = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(block != MAP_FAILED);
#if UINTPTR_MAX > 0xFFFFFFFFu
    assert((uintptr_t)block > 0xFFFFFFFFu);
#else
    assert((uintptr_t)block >= 0x01000000u);
#endif
    return block;
}

static void releaseBlock(void *block)
{
    int status = munmap(block, (size_t)sysconf(_SC_PAGESIZE));
    assert(status == 0);
}

static void loadBufferReadsTheWholeAddress(void)
{
    /* Arrange */
    u8 *block = addressBlock();
    Acmd commands[2];
    for (s32 i = 0; i < 64; i++) block[i] = (u8)(i * 3 + 1);
    memset(s_Dmem, 0, sizeof(s_Dmem));
    aSetBuffer(&commands[0], 0, 256, 0, 64);
    aLoadBuffer(&commands[1], block);
    /* Act */
    s32 accepted = portAcmdValidate(commands, 2);
    portAcmdRun(commands, 2);
    /* Assert */
    assert(accepted);
    assert(!memcmp(&s_Dmem[256], block, 64));
    releaseBlock(block);
}

#if UINTPTR_MAX > 0xFFFFFFFFu
/* A page whose address has a zero low word: cut to 32 bits it reads as 0,
 * which the validator refuses as a garbage address. */
static u8 *lowWordZeroPage(size_t bytes)
{
    static const uintptr_t kCandidates[] = {0x7e0000000000ull, 0x3e0000000000ull, 0x7000000000ull, 0x700000000ull};
    for (u32 i = 0; i < sizeof(kCandidates) / sizeof(kCandidates[0]); i++) {
        void *page = mmap((void *)kCandidates[i], bytes, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (page == (void *)kCandidates[i]) return page;
        if (page != MAP_FAILED) {
            int status = munmap(page, bytes); /* a kernel without MAP_FIXED_NOREPLACE took it as a hint */
            assert(status == 0);
        }
    }
    fprintf(stderr, "no page with a zero low word could be mapped\n");
    abort();
}

static void validatorAcceptsAnAddressWhoseLowWordIsZero(void)
{
    /* Arrange */
    const size_t bytes = (size_t)sysconf(_SC_PAGESIZE);
    u8 *page = lowWordZeroPage(bytes);
    Acmd commands[2];
    aSetBuffer(&commands[0], 0, 256, 0, 64);
    aLoadBuffer(&commands[1], page);
    /* Act */
    s32 accepted = portAcmdValidate(commands, 2);
    /* Assert */
    assert(accepted);
    int status = munmap(page, bytes);
    assert(status == 0);
}
#endif

static void saveBufferWritesTheWholeAddress(void)
{
    /* Arrange */
    u8 *block = addressBlock();
    Acmd commands[2];
    for (s32 i = 0; i < 64; i++) s_Dmem[512 + i] = (u8)(200 - i);
    aSetBuffer(&commands[0], 0, 0, 512, 64);
    aSaveBuffer(&commands[1], block);
    /* Act */
    portAcmdRun(commands, 2);
    /* Assert */
    assert(!memcmp(block, &s_Dmem[512], 64));
    releaseBlock(block);
}

static void loadAdpcmReadsTheBookAtTheWholeAddress(void)
{
    /* Arrange */
    s16 *book = addressBlock();
    Acmd command;
    for (s32 i = 0; i < 16; i++) book[i] = (s16)(i * 101 - 700);
    memset(s_Book, 0, sizeof(s_Book));
    aLoadADPCM(&command, 16 * sizeof(s16), book);
    /* Act */
    portAcmdRun(&command, 1);
    /* Assert */
    assert(!memcmp(s_Book, book, 16 * sizeof(s16)));
    releaseBlock(book);
}

static void setLoopKeepsTheWholeAddress(void)
{
    /* Arrange */
    s16 *loop = addressBlock();
    Acmd command;
    s_LoopState = NULL;
    aSetLoop(&command, loop);
    /* Act */
    portAcmdRun(&command, 1);
    /* Assert */
    assert(s_LoopState == loop);
    releaseBlock(loop);
}

static void adpcmDecodesThroughTheWholeStateAddress(void)
{
    /* Arrange: zero input bytes and book decode to zeros after the prefix. */
    s16 *state = addressBlock();
    Acmd commands[2];
    s16 prefix[16];
    for (s32 i = 0; i < 16; i++) state[i] = (s16)(i + 40);
    memcpy(prefix, state, sizeof(prefix));
    memset(s_Dmem, 0, sizeof(s_Dmem));
    memset(s_Book, 0, sizeof(s_Book));
    aSetBuffer(&commands[0], 0, 0, 1024, 32);
    aADPCMdec(&commands[1], A_CONTINUE, state);
    /* Act */
    portAcmdRun(commands, 2);
    /* Assert: the old state became the output prefix; the new state is the last 16 samples. */
    assert(!memcmp(&s_Dmem[1024], prefix, sizeof(prefix)));
    for (s32 i = 0; i < 16; i++) assert(state[i] == 0);
    releaseBlock(state);
}

static void resampleCarriesItsHistoryThroughTheWholeStateAddress(void)
{
    /* Arrange: unity pitch steps one input sample per output sample. */
    s16 *state = addressBlock();
    Acmd commands[2];
    s16 *input = (s16 *)&s_Dmem[256];
    for (s32 i = 0; i < 16; i++) input[i] = (s16)(100 + i);
    aSetBuffer(&commands[0], 0, 256, 1024, 16);
    aResample(&commands[1], A_CONTINUE, 0x8000, state);
    /* Act */
    portAcmdRun(commands, 2);
    /* Assert: the next frame's four history samples are input 4..7. */
    for (s32 i = 0; i < 4; i++) assert(state[i] == 104 + i);
    releaseBlock(state);
}

static void envMixerCarriesItsRampThroughTheWholeStateAddress(void)
{
    /* Arrange: a ramp of one volume step a sample, read from the state. */
    PortEnvState *state = addressBlock();
    Acmd commands[3];
    state->rate[0] = state->rate[1] = 1 << 16;
    state->tgt[0] = state->tgt[1] = 100;
    aSetBuffer(&commands[0], A_AUX, 2048, 2304, 2560);
    aSetBuffer(&commands[1], 0, 256, 1024, 16);
    aEnvMixer(&commands[2], A_CONTINUE, state);
    /* Act */
    portAcmdRun(commands, 3);
    /* Assert: eight samples later the ramp is written back at eight steps. */
    assert(state->t[0] == 8 << 16 && state->t[1] == 8 << 16);
    releaseBlock(state);
}

static void poleFilterCarriesItsOutputThroughTheWholeStateAddress(void)
{
    /* Arrange: unit gain and feedback add each input sample to the carried output. */
    s16 *state = addressBlock();
    Acmd commands[2];
    s16 *buffer = (s16 *)&s_Dmem[1536];
    state[0] = 10;
    for (s32 i = 0; i < 8; i++) buffer[i] = 1;
    memset(s_Book, 0, sizeof(s_Book));
    s_Book[8] = 0x4000;
    aSetBuffer(&commands[0], 0, 1536, 0, 16);
    aPoleFilter(&commands[1], A_CONTINUE, 0x4000, state);
    /* Act */
    portAcmdRun(commands, 2);
    /* Assert */
    assert(state[0] == 18);
    releaseBlock(state);
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

    /* Every address command the interpreter runs carries the whole address
     * (aPan's A_PAN is defined nowhere, so it cannot be expanded). */
    loadBufferReadsTheWholeAddress();
#if UINTPTR_MAX > 0xFFFFFFFFu
    validatorAcceptsAnAddressWhoseLowWordIsZero();
#endif
    saveBufferWritesTheWholeAddress();
    loadAdpcmReadsTheBookAtTheWholeAddress();
    setLoopKeepsTheWholeAddress();
    adpcmDecodesThroughTheWholeStateAddress();
    resampleCarriesItsHistoryThroughTheWholeStateAddress();
    envMixerCarriesItsRampThroughTheWholeStateAddress();
    poleFilterCarriesItsOutputThroughTheWholeStateAddress();
    puts("Audio buffer and short-list checks passed");
    return 0;
}
