/* The music sequence table converter (LP64-004 Task 6): portSeqTableSize
 * measures the run-time table of a big-endian sequence bank header, and
 * portSeqTableConvert writes it, each offset zero-extended into its address
 * slot, refusing by name, with nothing written, a file too short for its
 * entries or a room too small for the table. Built and run at each pointer
 * width by test_romconv.sh. Every input is written here. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "port_romconv.h"

#define SEQ_ENTRIES 3u
#define FILE_SIZE (4u + SEQ_ENTRIES * 8u)
#define TABLE_SIZE (offsetof(RareALSeqBankFile, seqArray) + SEQ_ENTRIES * sizeof(RareALSeqData))
#define FILL 0xA5u

/* port_console.c's log, captured so the refusal line can be read back. */
static char s_Log[512];

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_Log, sizeof(s_Log), fmt, ap);
    va_end(ap);
}

static int startsWith(const char *text, const char *prefix)
{
    while (*prefix != '\0' && *text == *prefix)
    {
        text++;
        prefix++;
    }
    return *prefix == '\0';
}

static int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_seq: FAIL: %s\n", what);
        return 1;
    }
    return 0;
}

/* The header and three entries, big-endian: count 3, unk 0, then offsets
 * 0x1FC, 0x300 and 0x80000010 with lengths 6109/3948, 10/8 and 1/1. */
static const u8 kFile[FILE_SIZE] = {
    0x00, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xFC, 0x17, 0xDD, 0x0F, 0x6C,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x0A, 0x00, 0x08,
    0x80, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00, 0x01,
};

/* Room for the table, 8-byte aligned, every byte FILL before the call. */
static union
{
    RareALSeqBankFile table;
    u8 bytes[TABLE_SIZE + 16];
} s_Out;

static RareALSeqBankFile *freshOut(void)
{
    memset(s_Out.bytes, FILL, sizeof(s_Out.bytes));
    return &s_Out.table;
}

static int outUntouched(void)
{
    size_t i;

    for (i = 0; i < sizeof(s_Out.bytes); i++)
    {
        if (s_Out.bytes[i] != FILL)
        {
            return 0;
        }
    }
    return 1;
}

static RareALSeqBankFile *convertStock(void)
{
    RareALSeqBankFile *out = freshOut();

    portSeqTableConvert(kFile, FILE_SIZE, out, portSeqTableSize(kFile));
    return out;
}

#define REFUSAL "ge: lp64: cannot convert music sequences \"musicsampletbl\": "

/* Converts FILE (FILESIZE bytes) into OUTSIZE bytes, which must be refused,
 * and checks that nothing was written and the refusal names the class, the
 * file and the reason WHY. */
static int expectRefused(const u8 *file, u32 filesize, u32 outsize, const char *why, const char *what)
{
    RareALSeqBankFile *out = freshOut();
    int result;

    s_Log[0] = '\0';
    result = portSeqTableConvert(file, filesize, out, outsize);
    return expect(result == 0 && outUntouched() && startsWith(s_Log, REFUSAL)
            && startsWith(s_Log + sizeof(REFUSAL) - 1, why),
            what);
}

static int testMeasuresTheRunTimeTable(void)
{
    // Act
    u32 size = portSeqTableSize(kFile);

    // Assert
    return expect(size == offsetof(RareALSeqBankFile, seqArray) + 3 * sizeof(RareALSeqData),
            "three entries measure the header and three RareALSeqData");
}

static int testMeasuresTheLargestCountWithoutWrapping(void)
{
    // Arrange
    static const u8 file[4] = {0xFF, 0xFF, 0x00, 0x00};

    // Act
    u32 size = portSeqTableSize(file);

    // Assert
    return expect(size == offsetof(RareALSeqBankFile, seqArray) + 0xFFFFu * sizeof(RareALSeqData),
            "a count of 0xFFFF measures without wrapping");
}

static int testConverts(void)
{
    // Arrange
    RareALSeqBankFile *out = freshOut();

    // Act
    int result = portSeqTableConvert(kFile, FILE_SIZE, out, portSeqTableSize(kFile));

    // Assert
    return expect(result == 1, "a three-entry table converts");
}

static int testWritesTheCount(void)
{
    // Act
    RareALSeqBankFile *out = convertStock();

    // Assert
    return expect(out->seqCount == 3, "seqCount is 3");
}

static int testZeroExtendsTheOffsets(void)
{
    // Act
    RareALSeqBankFile *out = convertStock();

    // Assert
    return expect((uintptr_t) out->seqArray[0].address == 0x1FC && (uintptr_t) out->seqArray[1].address == 0x300,
            "the offsets 0x1FC and 0x300 are in the address slots");
}

static int testDoesNotSignExtend(void)
{
    // Act
    RareALSeqBankFile *out = convertStock();

    // Assert
    return expect((uintptr_t) out->seqArray[2].address == 0x80000010u, "0x80000010 is zero-extended");
}

static int testWritesTheLengths(void)
{
    // Act
    RareALSeqBankFile *out = convertStock();

    // Assert
    return expect(out->seqArray[0].uncompressed_len == 6109 && out->seqArray[0].len == 3948
            && out->seqArray[1].uncompressed_len == 10 && out->seqArray[1].len == 8
            && out->seqArray[2].uncompressed_len == 1 && out->seqArray[2].len == 1,
            "each entry's two lengths are in native byte order");
}

static int testWritesNothingPastTheTable(void)
{
    // Act
    convertStock();

    // Assert
    return expect(s_Out.bytes[TABLE_SIZE] == FILL, "the byte after the table is untouched");
}

static int testRefusesARoomOneByteShort(void)
{
    // Act, Assert
    return expectRefused(kFile, FILE_SIZE, (u32) TABLE_SIZE - 1, "the table needs ",
            "a room one byte short is refused, nothing written");
}

static int testRefusesAFileShortOfItsEntries(void)
{
    // Act, Assert
    return expectRefused(kFile, FILE_SIZE - 1, (u32) TABLE_SIZE, "3 entries need 28 bytes, the file has 27",
            "a file one byte short of its entries is refused, nothing written");
}

static int testRefusesAFileShortOfItsHeader(void)
{
    // Act, Assert
    return expectRefused(kFile, 3, (u32) TABLE_SIZE, "the 3-byte file is shorter than its 4-byte header",
            "a file shorter than the header is refused, nothing written");
}

static int testRefusesTheLargestCountInASmallFile(void)
{
    // Arrange: 0xFFFF entries in a 28-byte file, into a room that would
    // fit them; a count that wrapped would pass the file check.
    u8 file[FILE_SIZE];
    memcpy(file, kFile, FILE_SIZE);
    file[0] = 0xFF;
    file[1] = 0xFF;

    // Act, Assert
    return expectRefused(file, FILE_SIZE, 0xFFFFFFFFu, "65535 entries need 524284 bytes, the file has 28",
            "a count of 0xFFFF in a 28-byte file is refused, nothing written");
}

#ifdef GE_LP64_ORACLE
/* The in-place result for kFile at 32 bits, written field by field: the
 * native count, unk as stored (zero), each offset in its address slot. */
static RareALSeqBankFile *inPlaceTable(void)
{
    RareALSeqBankFile *t = freshOut();

    memset(t, 0, TABLE_SIZE);
    t->seqCount = 3;
    t->seqArray[0].address = (u8 *) (uintptr_t) 0x1FC;
    t->seqArray[0].uncompressed_len = 6109;
    t->seqArray[0].len = 3948;
    t->seqArray[1].address = (u8 *) (uintptr_t) 0x300;
    t->seqArray[1].uncompressed_len = 10;
    t->seqArray[1].len = 8;
    t->seqArray[2].address = (u8 *) (uintptr_t) 0x80000010u;
    t->seqArray[2].uncompressed_len = 1;
    t->seqArray[2].len = 1;
    return t;
}

static int testOracleFindsTheInPlaceTableIdentical(void)
{
    // Arrange
    RareALSeqBankFile *inplace = inPlaceTable();
    s_Log[0] = '\0';

    // Act
    portSeqTableOracle(inplace, kFile, FILE_SIZE);

    // Assert
    return expect(startsWith(s_Log, "ge: lp64: oracle: music sequences musicsampletbl identical (28 bytes)"),
            "the oracle finds the in-place table identical");
}

static int testOracleFindsALengthThatDiffers(void)
{
    // Arrange
    RareALSeqBankFile *inplace = inPlaceTable();
    inplace->seqArray[2].len = 2;
    s_Log[0] = '\0';

    // Act
    portSeqTableOracle(inplace, kFile, FILE_SIZE);

    // Assert
    return expect(startsWith(s_Log, "ge: lp64: oracle: music sequences musicsampletbl DIFFERS at +0x1A"),
            "the oracle finds the third entry's len differing");
}
#endif

int main(void)
{
    int failures = 0;

    failures += testMeasuresTheRunTimeTable();
    failures += testMeasuresTheLargestCountWithoutWrapping();
    failures += testConverts();
    failures += testWritesTheCount();
    failures += testZeroExtendsTheOffsets();
    failures += testDoesNotSignExtend();
    failures += testWritesTheLengths();
    failures += testWritesNothingPastTheTable();
    failures += testRefusesARoomOneByteShort();
    failures += testRefusesAFileShortOfItsEntries();
    failures += testRefusesAFileShortOfItsHeader();
    failures += testRefusesTheLargestCountInASmallFile();
#ifdef GE_LP64_ORACLE
    failures += testOracleFindsTheInPlaceTableIdentical();
    failures += testOracleFindsALengthThatDiffers();
#endif
    return failures == 0 ? 0 : 1;
}
