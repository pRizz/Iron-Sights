/**
 * The music sequence table converter (port_romconv.h; LP64-004 Task 6).
 *
 * The sequence bank (the musicsampletbl segment, music.sbk) starts with a
 * 4-byte header and 8-byte entries, big-endian, each entry a 4-byte offset
 * from the file's start and two lengths. At 32 bits the game copies the
 * header and entries into the music heap, swaps them in place, and
 * musicSeqFileNew adds the segment's address to each offset in its pointer
 * slot. At 64 bits RareALSeqData's pointer slot makes an entry 16 bytes and
 * the header 8, so the table is written at its run-time size here, each offset
 * zero-extended into its slot, and musicSeqFileNew rebases it unchanged.
 *
 * Compiled only at 64 bits and in the 32-bit oracle build, where the
 * converter's table must equal the in-place one byte for byte.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "port_romconv_audio.h"
#include "port_romlayout_audio.h"
#ifdef GE_LP64_ORACLE
#include <stdlib.h>
#include "port_lp64_oracle.h"
#endif

#define SEQ_FILE "musicsampletbl"
#define SEQ_ALIGN16(x) (((u32) (x) + 15u) & ~15u)

u32 portSeqTableSize(const u8 *file)
{
    u32 count = portConvBe16(file);

    /* count <= 0xFFFF, so at most 8 + 0xFFFF * 16 bytes: no wrap. */
    return (u32) offsetof(RareALSeqBankFile, seqArray) + count * (u32) sizeof(RareALSeqData);
}

/* The bytes musicTrack1Play and its two twins (music.c) leave from their
 * track buffer's start to the end of the compressed copy: the uncompressed
 * length made even in a u16, as musicInit does, rounded up to 16, plus
 * NUM_MUSIC_TRACKS rounded up to 16. */
static u32 seqCopyEnd(u16 uncompressed)
{
    u16 even = (u16) (uncompressed + (uncompressed & 1u));

    return SEQ_ALIGN16(even) + SEQ_ALIGN16(NUM_MUSIC_TRACKS);
}

/* Refuses, naming the file, an entry whose len bytes from its offset are not
 * all in the file, as musicTrack1Play and its two twins copy them from the
 * segment's address plus the offset; or whose len, rounded up to 16, is more
 * than seqCopyEnd, as the copy is placed to end there and would start before
 * the track buffer. Reads only. */
static int seqSequencesInFile(const u8 *file, u32 filesize, u32 count)
{
    u32 i;

    for (i = 0; i < count; i++)
    {
        const u8 *entry = file + sizeof(RomSeqTable) + i * sizeof(RomSeqEntry);
        u32 offset = portConvBe32(entry + offsetof(RomSeqEntry, offset));
        u32 len = portConvBe16(entry + offsetof(RomSeqEntry, len));
        u16 uncompressed = portConvBe16(entry + offsetof(RomSeqEntry, uncompressed_len));

        if ((u64) offset + len > filesize)
        {
            portConvRefuse(PORT_LP64_MUSIC, SEQ_FILE, "sequence %u's %u bytes at 0x%X pass the %u-byte file", i, len,
                    offset, filesize);
            return 0;
        }
        if (SEQ_ALIGN16(len) > seqCopyEnd(uncompressed))
        {
            portConvRefuse(PORT_LP64_MUSIC, SEQ_FILE,
                    "sequence %u's %u compressed bytes, rounded to %u, pass the %u bytes its %u uncompressed bytes "
                    "leave", i, len, SEQ_ALIGN16(len), seqCopyEnd(uncompressed), (u32) uncompressed);
            return 0;
        }
    }
    return 1;
}

/* Refuses, naming the file, a FILESIZE too small for the header, for the
 * header and its entries, or for an entry's sequence; fewer entries than the
 * NUM_MUSIC_TRACKS that musicInit reads whatever the count; or an entry whose
 * copy seqSequencesInFile refuses. Reads only. */
static int seqFileHoldsEntries(const u8 *file, u32 filesize)
{
    u32 count, needed;

    if (filesize < sizeof(RomSeqTable))
    {
        portConvRefuse(PORT_LP64_MUSIC, SEQ_FILE, "the %u-byte file is shorter than its %u-byte header", filesize,
                (u32) sizeof(RomSeqTable));
        return 0;
    }
    count = portConvBe16(file);
    /* count <= 0xFFFF, so at most 4 + 0xFFFF * 8 bytes: no wrap. */
    needed = (u32) sizeof(RomSeqTable) + count * (u32) sizeof(RomSeqEntry);
    if (needed > filesize)
    {
        portConvRefuse(PORT_LP64_MUSIC, SEQ_FILE, "%u entries need %u bytes, the file has %u", count, needed,
                filesize);
        return 0;
    }
    if (count < NUM_MUSIC_TRACKS)
    {
        portConvRefuse(PORT_LP64_MUSIC, SEQ_FILE, "the file has %u sequences, fewer than the %u the game reads", count,
                (u32) NUM_MUSIC_TRACKS);
        return 0;
    }
    return seqSequencesInFile(file, filesize, count);
}

int portSeqTableConvert(const u8 *file, u32 filesize, RareALSeqBankFile *out, u32 outsize)
{
    u32 size, count, i;

    if (!seqFileHoldsEntries(file, filesize))
    {
        return 0;
    }
    size = portSeqTableSize(file);
    if (outsize < size)
    {
        portConvRefuse(PORT_LP64_MUSIC, SEQ_FILE, "the table needs %u bytes, the room is %u", size, outsize);
        return 0;
    }
    count = portConvBe16(file);
    memset(out, 0, size);
    out->seqCount = (u16) count;
    memcpy(&out->unk, file + offsetof(RomSeqTable, unk), sizeof(out->unk));
    for (i = 0; i < count; i++)
    {
        const u8 *entry = file + sizeof(RomSeqTable) + i * sizeof(RomSeqEntry);

        out->seqArray[i].address = (u8 *) (uintptr_t) portConvBe32(entry + offsetof(RomSeqEntry, offset));
        out->seqArray[i].uncompressed_len = portConvBe16(entry + offsetof(RomSeqEntry, uncompressed_len));
        out->seqArray[i].len = portConvBe16(entry + offsetof(RomSeqEntry, len));
    }
    return 1;
}

#ifdef GE_LP64_ORACLE
void portSeqTableOracle(const RareALSeqBankFile *inplace, const u8 *file, u32 filesize)
{
    u32 size;
    RareALSeqBankFile *converted;

    if (filesize < sizeof(RomSeqTable))
    {
        portLp64OracleResult(PORT_LP64_MUSIC, SEQ_FILE, 0, "the file is shorter than its header");
        return;
    }
    size = portSeqTableSize(file);
    converted = (RareALSeqBankFile *) malloc(size);
    if (converted == NULL)
    {
        portLp64OracleResult(PORT_LP64_MUSIC, SEQ_FILE, 0, "the oracle could not allocate the table");
        return;
    }
    if (!portSeqTableConvert(file, filesize, converted, size))
    {
        portLp64OracleResult(PORT_LP64_MUSIC, SEQ_FILE, 0, "the converter refused the table");
    }
    else
    {
        portLp64OracleBytes(PORT_LP64_MUSIC, SEQ_FILE, inplace, converted, size);
    }
    free(converted);
}
#endif
