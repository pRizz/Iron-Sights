/* The audio bank converter's test fixture (LP64-004 Task 5), shared by
 * test_romconv_bank.c and test_romconv_bank_checks.c: one small bank file and
 * wave table built from numbers written here, and the checks the tests share. */
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

u8 s_File[FILE_SIZE];
u8 s_Table[TABLE_SIZE];
u8 s_Out[OUT_BYTES] __attribute__((aligned(8)));
char s_Log[512];

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_Log, sizeof(s_Log), fmt, ap);
    va_end(ap);
}

int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_bank: FAIL: %s (log: %s)\n", what, s_Log);
        fflush(stdout); /* before a sanitizer stops a later test */
        return 1;
    }
    return 0;
}

int allBytes(const u8 *p, size_t size, u8 value)
{
    size_t i;

    for (i = 0; i < size && p[i] == value; i++)
    {
    }
    return i == size;
}

void put16(u32 at, u32 v)
{
    s_File[at] = (u8) (v >> 8);
    s_File[at + 1] = (u8) v;
}

void put32(u32 at, u32 v)
{
    put16(at, v >> 16);
    put16(at + 2, v & 0xFFFF);
}

void putSound(u32 at, u32 env, u32 key, u32 wave, u8 pan, u8 vol)
{
    put32(at, env);
    put32(at + 4, key);
    put32(at + 8, wave);
    s_File[at + 12] = pan;
    s_File[at + 13] = vol;
}

void putEnvelope(u32 at, u32 attack, u32 decay, u32 release, u8 attackvol, u8 decayvol)
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
void buildBank(void)
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

u32 measure(const u8 *file)
{
    return portBankConvertedSize(file, FILE_SIZE, TABLE_SIZE, BANK_NAME);
}

ALBankFile *convertInto(u8 *file, u8 *table, u8 *out, u32 outsize)
{
    return portBankConvert(file, FILE_SIZE, table, TABLE_SIZE, out, outsize, BANK_NAME);
}

ALBankFile *convert(void)
{
    return convertInto(s_File, s_Table, s_Out, measure(s_File));
}

ALBank *convertedBank(void)
{
    buildBank();
    return convert()->bankArray[0];
}

/* Whether P is a whole record of BYTES inside the bytes written to s_Out. */
int inOut(const void *p, size_t bytes)
{
    uintptr_t at = (uintptr_t) p, out = (uintptr_t) s_Out;

    return at >= out && at - out + bytes <= RECORD_BYTES;
}

/* The file as built and then changed by the caller, which must be refused: the
 * size is 0, the conversion NULL, nothing written, and the line says WHY. */
int expectRefused(const char *why, const char *what)
{
    u8 file[FILE_SIZE], table[TABLE_SIZE];
    u32 size;
    ALBankFile *f;

    memcpy(file, s_File, FILE_SIZE);
    memcpy(table, s_Table, TABLE_SIZE);
    s_Log[0] = '\0';
    size = measure(s_File);
    f = convertInto(s_File, s_Table, s_Out, sizeof(s_Out));
    return expect(size == 0 && f == NULL && allBytes(s_Out, sizeof(s_Out), 0xAA) && sameBytes(file, s_File, FILE_SIZE)
            && sameBytes(table, s_Table, TABLE_SIZE) && contains(s_Log, REFUSAL) && contains(s_Log, why),
            what);
}
