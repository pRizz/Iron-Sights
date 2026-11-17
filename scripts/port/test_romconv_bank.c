/* The audio bank converter (LP64-004 Task 5): portBankConvert builds the
 * run-time graph of a big-endian bank file in new memory, each distinct bank,
 * instrument, sound and wave table once, and turns the in-file records to
 * native order once; portBankConvertedSize measures it first, and a refusal
 * writes nothing. Built and run at each pointer width by test_romconv.sh; the
 * 32-bit half also tests the oracle's graph comparison. Every input is
 * written here. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <PR/libaudio.h>
#include "platform_info.h"
#include "port_romconv.h"

/* The file's records, each at a multiple of 4. */
enum
{
    F_BANK = 0x08,     /* 2 instruments */
    F_INST0 = 0x1C,    /* 2 sounds */
    F_INST1 = 0x34,    /* 1 sound */
    F_SND0 = 0x48,     /* env0, key0, wave0 */
    F_SND1 = 0x58,     /* env0, key1, wave0 */
    F_SND2 = 0x68,     /* env1, key1, wave1 */
    F_ENV0 = 0x78,
    F_ENV1 = 0x88,
    F_KEY0 = 0x98,
    F_KEY1 = 0xA0,
    F_WAVE0 = 0xA8,    /* ADPCM, the book, the ADPCM loop */
    F_WAVE1 = 0xBC,    /* 16-bit raw, the raw loop */
    F_BOOK = 0xD0,     /* order 2, 1 predictor: 16 entries */
    F_RAWLOOP = 0xF8,
    F_SPARE = 0x104,   /* room for one more wave table, unreached */
    F_ADPCMLOOP = 0x118,
    FILE_SIZE = 0x144,
    TABLE_SIZE = 0x100,
};

/* The records' bytes: file 16 + 8, bank 16 + 16, instruments 16 + 16 and
 * 16 + 8, three sounds of 32, two wave tables of 32 (64 bits); 8, 20, 24, 20,
 * 3 x 16, 2 x 20 at 32 bits, each rounded up to 8. */
#if IS_64_BIT
#define RECORD_BYTES 264u
#else
#define RECORD_BYTES 176u
#endif

static u8 s_File[FILE_SIZE];
static u8 s_Table[TABLE_SIZE];
static u8 s_Out[RECORD_BYTES + 64] __attribute__((aligned(8)));
static char s_Log[512];

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_Log, sizeof(s_Log), fmt, ap);
    va_end(ap);
}

static int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_bank: FAIL: %s (log: %s)\n", what, s_Log);
        fflush(stdout); /* before a sanitizer stops a later test */
        return 1;
    }
    return 0;
}

/* The game's <string.h>, first on the include path, declares no memcmp or
 * strstr; the compiler's own are called by name. */
#define sameBytes(a, b, size) (__builtin_memcmp((a), (b), (size)) == 0)
#define contains(text, part) (__builtin_strstr((text), (part)) != NULL)

static int allBytes(const u8 *p, size_t size, u8 value)
{
    size_t i;

    for (i = 0; i < size && p[i] == value; i++)
    {
    }
    return i == size;
}

static void put16(u32 at, u32 v)
{
    s_File[at] = (u8) (v >> 8);
    s_File[at + 1] = (u8) v;
}

static void put32(u32 at, u32 v)
{
    put16(at, v >> 16);
    put16(at + 2, v & 0xFFFF);
}

static void putSound(u32 at, u32 env, u32 key, u32 wave, u8 pan, u8 vol)
{
    put32(at, env);
    put32(at + 4, key);
    put32(at + 8, wave);
    s_File[at + 12] = pan;
    s_File[at + 13] = vol;
}

static void putEnvelope(u32 at, u32 attack, u32 decay, u32 release, u8 attackvol, u8 decayvol)
{
    put32(at, attack);
    put32(at + 4, decay);
    put32(at + 8, release);
    s_File[at + 12] = attackvol;
    s_File[at + 13] = decayvol;
}

/* One bank at 22050 Hz with two instruments; the first has two sounds sharing
 * one wave table and one envelope, the second one sound with a 16-bit raw wave
 * and a raw loop; one ADPCM book of order 2 with 1 predictor. The table holds
 * the raw wave's two samples, big-endian, at 0x80. */
static void buildBank(void)
{
    u32 i;

    memset(s_File, 0, sizeof(s_File));
    memset(s_Table, 0, sizeof(s_Table));
    memset(s_Out, 0xAA, sizeof(s_Out));
    put16(0, AL_BANK_VERSION);
    put16(2, 1);
    put32(4, F_BANK);
    put16(F_BANK, 2);
    put32(F_BANK + 4, 22050);
    put32(F_BANK + 12, F_INST0);
    put32(F_BANK + 16, F_INST1);
    for (i = 0; i < 12; i++)
    {
        s_File[F_INST0 + i] = (u8) (0x70 + i); /* volume 0x70 ... vibDelay 0x7B; flags 0x73 is not read */
    }
    put16(F_INST0 + 12, 200);
    put16(F_INST0 + 14, 2);
    put32(F_INST0 + 16, F_SND0);
    put32(F_INST0 + 20, F_SND1);
    s_File[F_INST1] = 0x60;
    put16(F_INST1 + 12, 100);
    put16(F_INST1 + 14, 1);
    put32(F_INST1 + 16, F_SND2);
    putSound(F_SND0, F_ENV0, F_KEY0, F_WAVE0, 64, 127);
    putSound(F_SND1, F_ENV0, F_KEY1, F_WAVE0, 10, 100);
    putSound(F_SND2, F_ENV1, F_KEY1, F_WAVE1, 127, 90);
    putEnvelope(F_ENV0, 5000, 6000, 7000, 127, 100);
    putEnvelope(F_ENV1, 1000, 2000, 3000, 90, 80);
    s_File[F_KEY0 + 4] = 60; /* keyBase */
    s_File[F_KEY1 + 4] = 72;
    put32(F_WAVE0, 0x40);
    put32(F_WAVE0 + 4, 0x20);
    s_File[F_WAVE0 + 8] = AL_ADPCM_WAVE;
    put32(F_WAVE0 + 12, F_ADPCMLOOP);
    put32(F_WAVE0 + 16, F_BOOK);
    put32(F_WAVE1, 0x80);
    put32(F_WAVE1 + 4, 4);
    s_File[F_WAVE1 + 8] = AL_RAW16_WAVE;
    put32(F_WAVE1 + 12, F_RAWLOOP);
    put32(F_BOOK, 2);
    put32(F_BOOK + 4, 1);
    for (i = 0; i < 16; i++)
    {
        put16(F_BOOK + 8 + i * 2, i * 100);
    }
    put16(F_BOOK + 8 + 3 * 2, (u16) -1234);
    put32(F_ADPCMLOOP, 10);
    put32(F_ADPCMLOOP + 4, 20);
    put32(F_ADPCMLOOP + 8, 0xFFFFFFFF);
    put16(F_ADPCMLOOP + 12, 0x1234);
    put16(F_ADPCMLOOP + 12 + 15 * 2, (u16) -5);
    put32(F_RAWLOOP + 4, 2);
    put32(F_RAWLOOP + 8, 1);
    memcpy(s_Table + 0x80, "\x01\x02\x03\x04", 4);
}

static ALBankFile *convert(void)
{
    return portBankConvert(s_File, FILE_SIZE, s_Table, s_Out, portBankConvertedSize(s_File, FILE_SIZE));
}

static ALBank *convertedBank(void)
{
    buildBank();
    return convert()->bankArray[0];
}

/* Whether P is a whole record of BYTES inside the bytes written to s_Out. */
static int inOut(const void *p, size_t bytes)
{
    uintptr_t at = (uintptr_t) p, out = (uintptr_t) s_Out;

    return at >= out && at - out + bytes <= RECORD_BYTES;
}

#define REFUSAL "ge: lp64: cannot convert audio banks \""

/* The file as built and then changed by the caller, which must be refused: the
 * size is 0, the conversion NULL, nothing written, and the line says WHY. */
static int expectRefused(const char *why, const char *what)
{
    u8 file[FILE_SIZE], table[TABLE_SIZE];
    u32 size;
    ALBankFile *f;

    memcpy(file, s_File, FILE_SIZE);
    memcpy(table, s_Table, TABLE_SIZE);
    s_Log[0] = '\0';
    size = portBankConvertedSize(s_File, FILE_SIZE);
    f = portBankConvert(s_File, FILE_SIZE, s_Table, s_Out, sizeof(s_Out));
    return expect(size == 0 && f == NULL && allBytes(s_Out, sizeof(s_Out), 0xAA) && sameBytes(file, s_File, FILE_SIZE)
            && sameBytes(table, s_Table, TABLE_SIZE) && contains(s_Log, REFUSAL) && contains(s_Log, why),
            what);
}

static int testMeasuresEachRecordOnce(void)
{
    buildBank();
    return expect(portBankConvertedSize(s_File, FILE_SIZE) == RECORD_BYTES, "the size counts each distinct record once");
}

static int testWritesExactlyTheMeasuredBytes(void)
{
    ALBank *b = convertedBank();
    ALInstrument *i0 = b->instArray[0], *i1 = b->instArray[1];
    return expect(inOut(b, sizeof(ALBank)) && inOut(i0, sizeof(ALInstrument)) && inOut(i1, sizeof(ALInstrument))
            && inOut(i0->soundArray[1], sizeof(ALSound)) && inOut(i1->soundArray[0]->wavetable, sizeof(ALWaveTable))
            && allBytes(s_Out + RECORD_BYTES, sizeof(s_Out) - RECORD_BYTES, 0xAA),
            "every record lies in the measured bytes and nothing past them is written");
}

static int testBrief(void)
{
    // Arrange
    u32 n;
    ALBankFile *f;
    ALBank *b;
    buildBank();

    // Act
    n = portBankConvertedSize(s_File, FILE_SIZE);
    f = portBankConvert(s_File, FILE_SIZE, s_Table, s_Out, n);
    b = f->bankArray[0];

    // Assert
    return expect(f == (ALBankFile *) s_Out && f->revision == AL_BANK_VERSION && f->bankCount == 1
            && b->instCount == 2 && b->sampleRate == 22050 && b->instArray[0]->soundCount == 2
            && b->instArray[0]->soundArray[0]->wavetable == b->instArray[0]->soundArray[1]->wavetable
            && b->instArray[0]->soundArray[0]->wavetable->base == s_Table + 0x40
            && b->instArray[0]->soundArray[0]->envelope->attackTime == 5000
            && b->instArray[0]->soundArray[0]->wavetable->waveInfo.adpcmWave.book->book[3] == -1234
            && b->instArray[1]->soundArray[0]->wavetable->type == AL_RAW16_WAVE
            && ((s16 *) (s_Table + 0x80))[0] == 0x0102,
            "the brief's graph");
}

static int testBankFields(void)
{
    ALBank *b = convertedBank();
    return expect(b->flags == 1 && b->pad == 0 && b->percussion == NULL, "the bank's flags 1, no percussion");
}

static int testInstrumentFields(void)
{
    ALInstrument *i = convertedBank()->instArray[0];
    return expect(i->volume == 0x70 && i->pan == 0x71 && i->priority == 0x72 && i->flags == 1 && i->tremType == 0x74
            && i->tremDelay == 0x77 && i->vibType == 0x78 && i->vibDelay == 0x7B && i->bendRange == 200,
            "the instrument's bytes, bend range and flags");
}

static int testSoundFields(void)
{
    ALSound *s = convertedBank()->instArray[0]->soundArray[1];
    return expect(s->samplePan == 10 && s->sampleVolume == 100 && s->flags == 1 && s->keyMap == (ALKeyMap *) (s_File + F_KEY1)
            && s->keyMap->keyBase == 72,
            "the sound's pan, volume, flags and key map in the file");
}

static int testEnvelopeSwappedOnceInTheFile(void)
{
    ALBank *b = convertedBank();
    ALEnvelope *e = b->instArray[0]->soundArray[1]->envelope;
    return expect(e == b->instArray[0]->soundArray[0]->envelope && e == (ALEnvelope *) (s_File + F_ENV0)
            && e->attackTime == 5000 && e->decayTime == 6000 && e->releaseTime == 7000 && e->attackVolume == 127
            && e->decayVolume == 100,
            "a shared envelope stays in the file, native once");
}

static int testAdpcmWave(void)
{
    ALWaveTable *w = convertedBank()->instArray[0]->soundArray[0]->wavetable;
    ALADPCMBook *book = w->waveInfo.adpcmWave.book;
    return expect(w->len == 0x20 && w->type == AL_ADPCM_WAVE && w->flags == 1 && book == (ALADPCMBook *) (s_File + F_BOOK)
            && book->order == 2 && book->npredictors == 1 && book->book[1] == 100 && book->book[15] == 1500,
            "the ADPCM wave, its book native in the file");
}

static int testAdpcmLoop(void)
{
    ALADPCMloop *loop = convertedBank()->instArray[0]->soundArray[0]->wavetable->waveInfo.adpcmWave.loop;
    return expect(loop == (ALADPCMloop *) (s_File + F_ADPCMLOOP) && loop->start == 10 && loop->end == 20
            && loop->count == 0xFFFFFFFF && loop->state[0] == 0x1234 && loop->state[15] == -5,
            "the ADPCM loop native in the file, its state too");
}

static int testRawWave(void)
{
    ALWaveTable *w = convertedBank()->instArray[1]->soundArray[0]->wavetable;
    ALRawLoop *loop = w->waveInfo.rawWave.loop;
    return expect(w->base == s_Table + 0x80 && w->len == 4 && w->flags == 1 && loop == (ALRawLoop *) (s_File + F_RAWLOOP)
            && loop->start == 0 && loop->end == 2 && loop->count == 1 && ((s16 *) (s_Table + 0x80))[1] == 0x0304,
            "the raw wave, its loop native in the file, both samples swapped");
}

static int testRawSamplesOfOneBaseSwappedOnce(void)
{
    // Arrange: the second instrument's sound also uses a second raw wave
    // table at the same base, which the in-place conversion swaps once.
    buildBank();
    put32(F_SND1 + 8, F_SPARE);
    memcpy(s_File + F_SPARE, s_File + F_WAVE1, 20);
    put32(F_SPARE + 12, 0);

    // Act
    ALBankFile *f = portBankConvert(s_File, FILE_SIZE, s_Table, s_Out, sizeof(s_Out));

    // Assert
    return expect(f != NULL && ((s16 *) (s_Table + 0x80))[0] == 0x0102, "two raw waves of one base swap it once");
}

static int testPercussionSharesTheInstrument(void)
{
    buildBank();
    put32(F_BANK + 8, F_INST1);
    ALBank *b = convert()->bankArray[0];
    return expect(b->percussion == b->instArray[1], "the percussion instrument is the listed one, written once");
}

static int testInstrumentOffsetZeroIsTheFile(void)
{
    buildBank();
    put32(F_BANK + 16, 0);
    ALBank *b = convert()->bankArray[0];
    return expect(b->instArray[1] == (ALInstrument *) s_File && portBankConvertedSize(s_File, FILE_SIZE) < RECORD_BYTES,
            "an instrument offset of 0 is the file's start, not followed");
}

static int testRefusesAShortBuffer(void)
{
    // Arrange
    u8 file[FILE_SIZE], table[TABLE_SIZE];
    buildBank();
    memcpy(file, s_File, FILE_SIZE);
    memcpy(table, s_Table, TABLE_SIZE);
    s_Log[0] = '\0';

    // Act
    ALBankFile *f = portBankConvert(s_File, FILE_SIZE, s_Table, s_Out, RECORD_BYTES - 8);

    // Assert
    return expect(f == NULL && allBytes(s_Out, sizeof(s_Out), 0xAA) && sameBytes(file, s_File, FILE_SIZE)
            && sameBytes(table, s_Table, TABLE_SIZE) && contains(s_Log, "records need"),
            "a buffer one record short is refused, nothing written");
}

static int testRefusesAnUnalignedBuffer(void)
{
    buildBank();
    s_Log[0] = '\0';
    ALBankFile *f = portBankConvert(s_File, FILE_SIZE, s_Table, s_Out + 4, sizeof(s_Out) - 4);
    return expect(f == NULL && allBytes(s_Out, sizeof(s_Out), 0xAA) && contains(s_Log, "aligned"),
            "a buffer not 8-byte aligned is refused, nothing written");
}

/* One changed word of the built file each, which must be refused, naming WHY. */
static const struct
{
    u32 at, bytes, value;
    const char *why, *what;
} kRefusals[] = {
    {F_INST1 + 14, 2, 0x100, "instrument at 0x34", "an instrument count past the file's end gives size 0"},
    {F_BANK, 2, 0xFFFF, "count -1", "a negative instrument count is refused"},
    {0, 2, 0x4232, "revision 0x4232", "a revision other than B1 is refused"},
    {F_SND2 + 4, 4, FILE_SIZE, "key map at 0x144", "a key map past the file's end is refused"},
    {F_SND1, 4, F_ENV1 + 2, "not a multiple of 4", "an envelope offset not a multiple of 4 is refused"},
    {F_SND2, 4, F_SND0, "two kinds of record: sound and envelope", "a sound reached again as an envelope is refused"},
    {F_BOOK, 4, 0xFFFFFFFF, "order -1 and 1 predictors", "a book of negative order is refused"},
    {F_BOOK, 4, 0x1000, "ADPCM book at 0xD0", "a book whose entries run past the file's end is refused"},
};

static int testRefusals(void)
{
    int failures = 0;
    size_t r;

    for (r = 0; r < sizeof(kRefusals) / sizeof(kRefusals[0]); r++)
    {
        buildBank();
        (kRefusals[r].bytes == 2 ? put16 : put32)(kRefusals[r].at, kRefusals[r].value);
        failures += expectRefused(kRefusals[r].why, kRefusals[r].what);
    }
    return failures;
}

#ifdef GE_LP64_ORACLE
static u8 s_FileB[FILE_SIZE], s_TableB[TABLE_SIZE];
static u8 s_OutB[RECORD_BYTES] __attribute__((aligned(8)));
static char s_Why[256];

/* Converts the bank twice (A in s_File/s_Out, B in s_FileB/s_OutB) and
 * returns B's root; *A gets A's. */
static ALBankFile *convertTwice(ALBankFile **a)
{
    buildBank();
    memcpy(s_FileB, s_File, FILE_SIZE);
    memcpy(s_TableB, s_Table, TABLE_SIZE);
    *a = convert();
    return portBankConvert(s_FileB, FILE_SIZE, s_TableB, s_OutB, RECORD_BYTES);
}

static int graphsEqual(const ALBankFile *a, const ALBankFile *b)
{
    struct PortBankGraph ga = {a, s_File, FILE_SIZE, s_Table};
    struct PortBankGraph gb = {b, s_FileB, FILE_SIZE, s_TableB};
    s_Why[0] = '\0';
    return portBankGraphsEqual(&ga, &gb, s_Why, sizeof(s_Why));
}

static int testOracleEqualGraphs(void)
{
    ALBankFile *a, *b = convertTwice(&a);
    return expect(graphsEqual(a, b), "the oracle finds two conversions of one file equal");
}

/* The oracle finds one changed field of B, and names its record KIND. */
#define ORACLE_SEES(name, change, kind)                                                                     \
    static int name(void)                                                                               \
    {                                                                                                   \
        ALBankFile *a, *b = convertTwice(&a);                                                           \
        change;                                                                                         \
        return expect(!graphsEqual(a, b) && contains(s_Why, kind), "the oracle sees a changed " kind); \
    }
ORACLE_SEES(testOracleSeesABank, b->bankArray[0]->sampleRate++, "bank")
ORACLE_SEES(testOracleSeesAnInstrument, b->bankArray[0]->instArray[1]->bendRange++, "instrument")
ORACLE_SEES(testOracleSeesASound, b->bankArray[0]->instArray[0]->soundArray[1]->samplePan++, "sound")
ORACLE_SEES(testOracleSeesAWaveTable, b->bankArray[0]->instArray[1]->soundArray[0]->wavetable->len++, "wave table")
ORACLE_SEES(testOracleSeesAnEnvelope, s_FileB[F_ENV1 + 13]++, "envelope")
ORACLE_SEES(testOracleSeesABook, s_FileB[F_BOOK + 20]++, "ADPCM book")
ORACLE_SEES(testOracleSeesAnAdpcmLoop, s_FileB[F_ADPCMLOOP + 40]++, "ADPCM loop")
ORACLE_SEES(testOracleSeesARawLoop, s_FileB[F_RAWLOOP + 4]++, "raw loop")
ORACLE_SEES(testOracleSeesAKeyMap, s_FileB[F_KEY0 + 4]++, "key map")
ORACLE_SEES(testOracleSeesAWaveBase, b->bankArray[0]->instArray[0]->soundArray[0]->wavetable->base++, "wave table")
ORACLE_SEES(testOracleSeesARecordLeftInTheFile, b->bankArray[0]->instArray[0] = (ALInstrument *) (s_FileB + F_INST0),
        "instrument at file offset 0x1C was left")
#endif

int main(void)
{
    int failures = 0;

    failures += testMeasuresEachRecordOnce();
    failures += testWritesExactlyTheMeasuredBytes();
    failures += testBrief();
    failures += testBankFields();
    failures += testInstrumentFields();
    failures += testSoundFields();
    failures += testEnvelopeSwappedOnceInTheFile();
    failures += testAdpcmWave();
    failures += testAdpcmLoop();
    failures += testRawWave();
    failures += testRawSamplesOfOneBaseSwappedOnce();
    failures += testPercussionSharesTheInstrument();
    failures += testInstrumentOffsetZeroIsTheFile();
    failures += testRefusesAShortBuffer();
    failures += testRefusesAnUnalignedBuffer();
    failures += testRefusals();
#ifdef GE_LP64_ORACLE
    failures += testOracleEqualGraphs();
    failures += testOracleSeesABank();
    failures += testOracleSeesAnInstrument();
    failures += testOracleSeesASound();
    failures += testOracleSeesAWaveTable();
    failures += testOracleSeesAnEnvelope();
    failures += testOracleSeesABook();
    failures += testOracleSeesAnAdpcmLoop();
    failures += testOracleSeesARawLoop();
    failures += testOracleSeesAKeyMap();
    failures += testOracleSeesAWaveBase();
    failures += testOracleSeesARecordLeftInTheFile();
#endif
    return failures == 0 ? 0 : 1;
}
