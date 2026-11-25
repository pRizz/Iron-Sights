/* The music sequence table converter (LP64-004 Task 6): portSeqTableSize
 * measures the run-time table of a big-endian sequence bank header, and
 * portSeqTableConvert writes it, each offset zero-extended into its address
 * slot, refusing by name, with nothing written, a file too short for its
 * entries or a room too small for the table. Built and run at each pointer
 * width by test_romconv.sh. Every input is written here: kFile is built by
 * buildFile, which main calls first. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "port_romconv_audio.h"

#define SEQ_ENTRIES 63u /* NUM_MUSIC_TRACKS, the entries musicInit reads */
#define FILE_SIZE (4u + SEQ_ENTRIES * 8u) /* the header and entries */
#define BANK_SIZE 0x1200u                 /* the whole file, with room for the sequences */
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

/* The header and 63 entries, big-endian: count 63, unk 0, then offsets
 * 0x1FC, 0x300 and 0x1100 with lengths (uncompressed/compressed) 6109/3948,
 * 10/8 and 1/1, and 60 more at 0x1180 of 1/1, in a file of BANK_SIZE bytes
 * that holds each sequence's len bytes. */
static u8 kFile[BANK_SIZE];

static void putEntry(u8 *file, u32 index, u32 offset, u16 uncompressed, u16 len)
{
    u8 *e = file + 4 + index * 8;

    e[0] = (u8) (offset >> 24);
    e[1] = (u8) (offset >> 16);
    e[2] = (u8) (offset >> 8);
    e[3] = (u8) offset;
    e[4] = (u8) (uncompressed >> 8);
    e[5] = (u8) uncompressed;
    e[6] = (u8) (len >> 8);
    e[7] = (u8) len;
}

static void buildFile(void)
{
    u32 i;

    memset(kFile, 0, sizeof(kFile));
    kFile[1] = (u8) SEQ_ENTRIES;
    putEntry(kFile, 0, 0x1FC, 6109, 3948);
    putEntry(kFile, 1, 0x300, 10, 8);
    putEntry(kFile, 2, 0x1100, 1, 1);
    for (i = 3; i < SEQ_ENTRIES; i++)
    {
        putEntry(kFile, i, 0x1180, 1, 1);
    }
}

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

    portSeqTableConvert(kFile, BANK_SIZE, out, portSeqTableSize(kFile));
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
    return expect(size == offsetof(RareALSeqBankFile, seqArray) + SEQ_ENTRIES * sizeof(RareALSeqData),
            "63 entries measure the header and 63 RareALSeqData");
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
    int result = portSeqTableConvert(kFile, BANK_SIZE, out, portSeqTableSize(kFile));

    // Assert
    return expect(result == 1, "a table of exactly the 63 entries the game reads converts");
}

static int testWritesTheCount(void)
{
    // Act
    RareALSeqBankFile *out = convertStock();

    // Assert
    return expect(out->seqCount == SEQ_ENTRIES, "seqCount is 63");
}

static int testZeroExtendsTheOffsets(void)
{
    // Act
    RareALSeqBankFile *out = convertStock();

    // Assert
    return expect((uintptr_t) out->seqArray[0].address == 0x1FC && (uintptr_t) out->seqArray[1].address == 0x300,
            "the offsets 0x1FC and 0x300 are in the address slots");
}

/* LP64-007 Task 7: musicTrack1Play and its twins read each sequence's len
 * bytes from its offset. Sequence 2 keeps its uncompressed length, 1. */
static const u8 *fileWithSequence2(u32 offset, u16 len)
{
    static u8 file[BANK_SIZE];

    memcpy(file, kFile, BANK_SIZE);
    putEntry(file, 2, offset, 1, len);
    return file;
}

/* musicInit reads NUM_MUSIC_TRACKS entries whatever the count. */
static int testRefusesFewerSequencesThanTheGameReads(void)
{
    // Arrange: count 62, the file otherwise as kFile
    static u8 file[BANK_SIZE];
    memcpy(file, kFile, BANK_SIZE);
    file[1] = 62;

    // Act, Assert
    return expectRefused(file, BANK_SIZE, (u32) TABLE_SIZE, "the file has 62 sequences, fewer than the 63 the game reads",
            "a file of 62 sequences is refused, nothing written");
}

/* musicTrack1Play and its twins copy len, rounded up to 16, to end where the
 * even uncompressed length, rounded up to 16, plus 64 ends from the track
 * buffer's start; a longer copy starts before the buffer. */
static const u8 *fileWithSequence2Lengths(u16 uncompressed, u16 len)
{
    static u8 file[BANK_SIZE];

    memcpy(file, kFile, BANK_SIZE);
    putEntry(file, 2, 0x1100, uncompressed, len);
    return file;
}

static int testRefusesACopyThatStartsBeforeTheTrackBuffer(void)
{
    // Arrange: 81 compressed bytes round to 96, past 16 + 64 = 80
    const u8 *file = fileWithSequence2Lengths(15, 81);

    // Act, Assert
    return expectRefused(file, BANK_SIZE, (u32) TABLE_SIZE,
            "sequence 2's 81 compressed bytes, rounded to 96, pass the 80 bytes its 15 uncompressed bytes leave",
            "a compressed copy longer than its uncompressed length leaves room for is refused, nothing written");
}

static int testRefusesAnOddLengthThatWrapsTheGamesRounding(void)
{
    // Arrange: the game rounds 0xFFFF up to an even u16, 0, so leaves 64 bytes
    const u8 *file = fileWithSequence2Lengths(0xFFFF, 65);

    // Act, Assert
    return expectRefused(file, BANK_SIZE, (u32) TABLE_SIZE,
            "sequence 2's 65 compressed bytes, rounded to 80, pass the 64 bytes its 65535 uncompressed bytes leave",
            "an uncompressed length of 0xFFFF, which the game rounds to 0, is refused, nothing written");
}

static int testAcceptsACopyThatStartsAtTheTrackBuffer(void)
{
    // Arrange: 80 compressed bytes fill the 80 that 15 uncompressed bytes leave
    const u8 *file = fileWithSequence2Lengths(15, 80);
    RareALSeqBankFile *out = freshOut();

    // Act
    int result = portSeqTableConvert(file, BANK_SIZE, out, portSeqTableSize(file));

    // Assert
    return expect(result == 1, "a compressed copy that starts at the track buffer's start is converted");
}

static int testRefusesASequencePastTheFile(void)
{
    // Arrange: sequence 2's two bytes start at the file's last byte
    const u8 *file = fileWithSequence2(BANK_SIZE - 1, 2);

    // Act, Assert
    return expectRefused(file, BANK_SIZE, (u32) TABLE_SIZE, "sequence 2's 2 bytes at 0x11FF pass the 4608-byte file",
            "a sequence that runs past the file is refused, nothing written");
}

static int testRefusesAnOffsetWithBit31Set(void)
{
    // Arrange: an offset a sign extension would turn negative
    const u8 *file = fileWithSequence2(0x80000010u, 1);

    // Act, Assert
    return expectRefused(file, BANK_SIZE, (u32) TABLE_SIZE, "sequence 2's 1 bytes at 0x80000010 pass",
            "an offset of 0x80000010 is refused, nothing written");
}

static int testAcceptsASequenceEndingAtTheFileEnd(void)
{
    // Arrange
    const u8 *file = fileWithSequence2(BANK_SIZE - 1, 1);
    RareALSeqBankFile *out = freshOut();

    // Act
    int result = portSeqTableConvert(file, BANK_SIZE, out, portSeqTableSize(file));

    // Assert
    return expect(result == 1 && (uintptr_t) out->seqArray[2].address == BANK_SIZE - 1,
            "a sequence whose last byte is the file's last is converted");
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
    return expectRefused(kFile, BANK_SIZE, (u32) TABLE_SIZE - 1, "the table needs ",
            "a room one byte short is refused, nothing written");
}

static int testRefusesAFileShortOfItsEntries(void)
{
    // Act, Assert
    return expectRefused(kFile, FILE_SIZE - 1, (u32) TABLE_SIZE, "63 entries need 508 bytes, the file has 507",
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
    // Arrange: 0xFFFF entries in a 508-byte file, into a room that would
    // fit them; a count that wrapped would pass the file check.
    u8 file[FILE_SIZE];
    memcpy(file, kFile, FILE_SIZE);
    file[0] = 0xFF;
    file[1] = 0xFF;

    // Act, Assert
    return expectRefused(file, FILE_SIZE, 0xFFFFFFFFu, "65535 entries need 524284 bytes, the file has 508",
            "a count of 0xFFFF in a 508-byte file is refused, nothing written");
}

#ifdef GE_LP64_ORACLE
/* The in-place result for kFile at 32 bits, written field by field: the
 * native count, unk as stored (zero), each offset in its address slot. */
static RareALSeqBankFile *inPlaceTable(void)
{
    RareALSeqBankFile *t = freshOut();
    u32 i;

    memset(t, 0, TABLE_SIZE);
    t->seqCount = SEQ_ENTRIES;
    t->seqArray[0].address = (u8 *) (uintptr_t) 0x1FC;
    t->seqArray[0].uncompressed_len = 6109;
    t->seqArray[0].len = 3948;
    t->seqArray[1].address = (u8 *) (uintptr_t) 0x300;
    t->seqArray[1].uncompressed_len = 10;
    t->seqArray[1].len = 8;
    t->seqArray[2].address = (u8 *) (uintptr_t) 0x1100u;
    t->seqArray[2].uncompressed_len = 1;
    t->seqArray[2].len = 1;
    for (i = 3; i < SEQ_ENTRIES; i++)
    {
        t->seqArray[i].address = (u8 *) (uintptr_t) 0x1180u;
        t->seqArray[i].uncompressed_len = 1;
        t->seqArray[i].len = 1;
    }
    return t;
}

static int testOracleFindsTheInPlaceTableIdentical(void)
{
    // Arrange
    RareALSeqBankFile *inplace = inPlaceTable();
    s_Log[0] = '\0';

    // Act
    portSeqTableOracle(inplace, kFile, BANK_SIZE);

    // Assert
    return expect(startsWith(s_Log, "ge: lp64: oracle: music sequences musicsampletbl identical (508 bytes)"),
            "the oracle finds the in-place table identical");
}

static int testOracleFindsALengthThatDiffers(void)
{
    // Arrange
    RareALSeqBankFile *inplace = inPlaceTable();
    inplace->seqArray[2].len = 2;
    s_Log[0] = '\0';

    // Act
    portSeqTableOracle(inplace, kFile, BANK_SIZE);

    // Assert
    return expect(startsWith(s_Log, "ge: lp64: oracle: music sequences musicsampletbl DIFFERS at +0x1A"),
            "the oracle finds the third entry's len differing");
}
#endif

int main(void)
{
    int failures = 0;

    buildFile();
    failures += testMeasuresTheRunTimeTable();
    failures += testMeasuresTheLargestCountWithoutWrapping();
    failures += testConverts();
    failures += testWritesTheCount();
    failures += testZeroExtendsTheOffsets();
    failures += testRefusesASequencePastTheFile();
    failures += testRefusesAnOffsetWithBit31Set();
    failures += testAcceptsASequenceEndingAtTheFileEnd();
    failures += testRefusesFewerSequencesThanTheGameReads();
    failures += testRefusesACopyThatStartsBeforeTheTrackBuffer();
    failures += testRefusesAnOddLengthThatWrapsTheGamesRounding();
    failures += testAcceptsACopyThatStartsAtTheTrackBuffer();
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
