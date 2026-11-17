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
#include "port_romconv_audio.h"
#include "test_romconv_bank_fixture.h"

static int testMeasuresEachRecordOnce(void)
{
    buildBank();
    return expect(measure(s_File) == RECORD_BYTES, "the size counts each distinct record once");
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
    n = measure(s_File);
    f = convertInto(s_File, s_Table, s_Out, n);
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
    ALBankFile *f = convertInto(s_File, s_Table, s_Out, sizeof(s_Out));

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
    return expect(b->instArray[1] == (ALInstrument *) s_File && measure(s_File) < RECORD_BYTES,
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
    ALBankFile *f = convertInto(s_File, s_Table, s_Out, RECORD_BYTES - 8);

    // Assert
    return expect(f == NULL && allBytes(s_Out, sizeof(s_Out), 0xAA) && sameBytes(file, s_File, FILE_SIZE)
            && sameBytes(table, s_Table, TABLE_SIZE) && contains(s_Log, "records need"),
            "a buffer one record short is refused, nothing written");
}

static int testRefusesAnUnalignedBuffer(void)
{
    buildBank();
    s_Log[0] = '\0';
    ALBankFile *f = convertInto(s_File, s_Table, s_Out + 4, sizeof(s_Out) - 4);
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
    return convertInto(s_FileB, s_TableB, s_OutB, RECORD_BYTES);
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
