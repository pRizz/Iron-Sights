/* The audio bank converter's test fixture (test_romconv_bank_fixture.c). */
#ifndef TEST_ROMCONV_BANK_FIXTURE_H
#define TEST_ROMCONV_BANK_FIXTURE_H

#include <stddef.h>
#include <ultra64.h>
#include <PR/libaudio.h>
#include "platform_info.h"

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

#define OUT_BYTES (RECORD_BYTES + 64)
#define BANK_NAME "testctl"
/* The start of every refusal of the fixture's file. */
#define REFUSAL "ge: lp64: cannot convert audio banks \"" BANK_NAME "\": "

/* The game's <string.h>, first on the include path, declares no memcmp or
 * strstr; the compiler's own are called by name. */
#define sameBytes(a, b, size) (__builtin_memcmp((a), (b), (size)) == 0)
#define contains(text, part) (__builtin_strstr((text), (part)) != NULL)

extern u8 s_File[FILE_SIZE];
extern u8 s_Table[TABLE_SIZE];
extern u8 s_Out[OUT_BYTES]; /* 8-byte aligned; 0xAA where nothing was written */
extern char s_Log[512];     /* the last line logged */

/** 0 when OK; otherwise prints WHAT and the last log line and returns 1. */
int expect(int ok, const char *what);
int allBytes(const u8 *p, size_t size, u8 value);
/** Big-endian values into s_File. */
void put16(u32 at, u32 v);
void put32(u32 at, u32 v);
void putSound(u32 at, u32 env, u32 key, u32 wave, u8 pan, u8 vol);
void putEnvelope(u32 at, u32 attack, u32 decay, u32 release, u8 attackvol, u8 decayvol);
/** The fixture's bank in s_File and table in s_Table; s_Out all 0xAA. */
void buildBank(void);
/** portBankConvertedSize and portBankConvert of a FILE_SIZE-byte file and a
 * TABLE_SIZE-byte table, named BANK_NAME. */
u32 measure(const u8 *file);
ALBankFile *convertInto(u8 *file, u8 *table, u8 *out, u32 outsize);
/** s_File converted into s_Out at its measured size. */
ALBankFile *convert(void);
/** buildBank, convert, and the bank. */
ALBank *convertedBank(void);
/** Whether P is a whole record of BYTES inside the bytes written to s_Out. */
int inOut(const void *p, size_t bytes);
/** s_File as changed by the caller must be refused: size 0, NULL, nothing
 * written to s_File, s_Table or s_Out, and the line says WHY. */
int expectRefused(const char *why, const char *what);

#endif
