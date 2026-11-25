/**
 * The audio bank converter's record checks (romconv_bank_walk.h; LP64-004
 * Task 5): each record's extent in the file and size in OUT, a wave's samples
 * against the table, the rule that no two records share a byte, and the byte
 * order of the kinds that stay in the file. Compiled only at 64 bits and in
 * the 32-bit oracle build.
 */
#include <stddef.h>
#include <string.h>
#include <ultra64.h>
#include <PR/libaudio.h>
#include "platform_info.h"
#include "port_romconv_audio.h"
#include "port_romlayout_audio.h"
#include "romconv_bank_walk.h"


const char *const kPortBankKindNames[] = {
    "raw loop", "bank", "instrument", "sound", "wave table", "envelope", "ADPCM book", "ADPCM loop", "key map",
    "bank list",
};

/* The bytes of each in-file kind, by kind (0 for the built ones). */
static const u32 kInFileBytes[] = {ROM_BANK_RAWLOOP_BYTES, 0, 0, 0, 0, ROM_BANK_ENVELOPE_BYTES, ROM_BANK_BOOK_HEAD,
    ROM_BANK_ADPCMLOOP_BYTES, ROM_BANK_KEYMAP_BYTES};

int portBankFits(const struct BankWalk *w, u32 offset, u64 bytes, int kind)
{
    if (offset % 4 != 0)
    {
        bankRefuse(w, "the %s offset 0x%X is not a multiple of 4", kPortBankKindNames[kind], offset);
        return 0;
    }
    if (offset > w->size || bytes > w->size - offset)
    {
        bankRefuse(w, "the %s at 0x%X needs 0x%llX bytes, past the 0x%X-byte file", kPortBankKindNames[kind], offset,
                (unsigned long long) bytes, w->size);
        return 0;
    }
    return 1;
}

int portBankCountValid(const struct BankWalk *w, s32 count, u32 offset, int kind)
{
    if (count >= 0)
    {
        return 1;
    }
    bankRefuse(w, "the %s at 0x%X has count %d", kPortBankKindNames[kind], offset, (int) count);
    return 0;
}

/* A bank or instrument: its head, then COUNT 4-byte offsets. */
static int bankMeasureArray(const struct BankWalk *w, u32 offset, int kind, u32 *bytes, u32 *extent)
{
    u32 head = kind == KIND_BANK ? sizeof(RomBank) : sizeof(RomInstrument);
    u32 field = kind == KIND_BANK ? offsetof(RomBank, instCount) : offsetof(RomInstrument, soundCount);
    s32 count;

    if (!portBankFits(w, offset, head, kind))
    {
        return 0;
    }
    count = bankBe16(w, offset + field);
    if (!portBankCountValid(w, count, offset, kind) || !portBankFits(w, offset, head + (u64) count * 4, kind))
    {
        return 0;
    }
    *extent = head + (u32) count * 4;
    *bytes = kind == KIND_BANK ? BANK_ARRAY_BYTES(ALBank, instArray, count)
                               : BANK_ARRAY_BYTES(ALInstrument, soundArray, count);
    return 1;
}

/* A wave table, whose samples must lie in the table: a wave of any type gets
 * the table's address plus base (bankWriteWave, as alBnkfNew), which the mixer
 * reads for len bytes (LP64-007 Task 7 bounded the types other than ADPCM and
 * raw too). */
static int bankMeasureWave(const struct BankWalk *w, u32 offset)
{
    u8 type;
    u32 base, len;

    if (!portBankFits(w, offset, sizeof(RomWaveTable), KIND_WAVE))
    {
        return 0;
    }
    type = w->file[offset + offsetof(RomWaveTable, type)];
    base = bankBe32(w, offset + offsetof(RomWaveTable, base));
    len = bankBe32(w, offset + offsetof(RomWaveTable, len));
    if ((u64) base + len <= w->tablesize)
    {
        return 1;
    }
    if (type == AL_ADPCM_WAVE || type == AL_RAW16_WAVE)
    {
        bankRefuse(w, "the %s wave table at 0x%X has samples at 0x%X to 0x%llX, past the 0x%X-byte wave table",
                type == AL_RAW16_WAVE ? "raw" : "ADPCM", offset, base, (unsigned long long) base + len, w->tablesize);
    }
    else
    {
        bankRefuse(w, "the type %u wave table at 0x%X has samples at 0x%X to 0x%llX, past the 0x%X-byte wave table",
                (u32) type, offset, base, (unsigned long long) base + len, w->tablesize);
    }
    return 0;
}

/* An in-file kind; a book's entries follow its header. */
static int bankMeasureInFile(const struct BankWalk *w, u32 offset, int kind, u32 *extent)
{
    s32 order, npredictors;
    u64 bytes = kInFileBytes[kind];

    if (!portBankFits(w, offset, bytes, kind))
    {
        return 0;
    }
    if (kind == KIND_BOOK)
    {
        order = (s32) bankBe32(w, offset);
        npredictors = (s32) bankBe32(w, offset + 4);
        if (order < 0 || npredictors < 0)
        {
            bankRefuse(w, "the ADPCM book at 0x%X has order %d and %d predictors", offset, (int) order,
                    (int) npredictors);
            return 0;
        }
        bytes = ROM_BANK_BOOK_HEAD + (u64) order * (u64) npredictors * 16;
        if (!portBankFits(w, offset, bytes, kind))
        {
            return 0;
        }
    }
    *extent = (u32) bytes;
    return 1;
}

int portBankMeasure(const struct BankWalk *w, u32 offset, int kind, u32 *bytes, u32 *extent)
{
    *bytes = 0;
    *extent = 0;
    switch (kind)
    {
    case KIND_BANK:
    case KIND_INST:
        return bankMeasureArray(w, offset, kind, bytes, extent);
    case KIND_SOUND:
        *bytes = sizeof(ALSound);
        *extent = sizeof(RomSound);
        return portBankFits(w, offset, sizeof(RomSound), kind);
    case KIND_WAVE:
        *bytes = sizeof(ALWaveTable);
        *extent = sizeof(RomWaveTable);
        return bankMeasureWave(w, offset);
    default:
        return bankMeasureInFile(w, offset, kind, extent);
    }
}

int portBankChecksInit(struct BankWalk *w)
{
    u64 slots = (u64) w->size / 4 + 1;
    u64 bits = (u64) w->tablesize / 8 + 1;
    u64 total = slots * 4 + slots + bits;
    u8 *p;

    if (total > 0xFFFFFFF0u)
    {
        bankRefuse(w, "the 0x%X-byte file and 0x%X-byte wave table are too large to check", w->size, w->tablesize);
        return 0;
    }
    p = portConvScratch((u32) total);
    memset(p, 0, (size_t) total);
    w->owner = (u32 *) p; /* the scratch block is malloc'd, so aligned for u32 */
    w->ownerKind = p + slots * 4;
    w->rawBases = w->ownerKind + slots;
    return 1;
}

int portBankClaim(struct BankWalk *w, u32 offset, u32 extent, int kind)
{
    u32 first = offset / 4, end = (u32) (((u64) offset + extent + 3) / 4), slot;

    for (slot = first; slot < end; slot++)
    {
        u32 owner = w->owner[slot];

        if (owner == 0)
        {
            w->owner[slot] = first + 1;
            w->ownerKind[slot] = (u8) kind;
        }
        else if (owner == first + 1 && w->ownerKind[slot] == kind)
        {
            continue;
        }
        else if (owner == first + 1)
        {
            bankRefuse(w, "offset 0x%X is reached as two kinds of record: %s and %s", offset,
                    kPortBankKindNames[w->ownerKind[slot]], kPortBankKindNames[kind]);
            return 0;
        }
        else
        {
            bankRefuse(w, "the %s at 0x%X overlaps the %s at 0x%X", kPortBankKindNames[kind], offset,
                    kPortBankKindNames[w->ownerKind[slot]], (owner - 1) * 4);
            return 0;
        }
    }
    return 1;
}

int portBankRawBaseFirst(struct BankWalk *w, u32 base)
{
    u8 bit = (u8) (1u << (base % 8));

    if (w->rawBases[base / 8] & bit)
    {
        return 0;
    }
    w->rawBases[base / 8] |= bit;
    return 1;
}

void portBankNative(u8 *p, u32 count, u32 width)
{
    u32 i;

    for (i = 0; i < count; i++, p += width)
    {
        u32 v32 = width == 4 ? portConvBe32(p) : portConvBe16(p);
        u16 v16 = (u16) v32;

        memcpy(p, width == 4 ? (const void *) &v32 : (const void *) &v16, width);
    }
}

void portBankNativeInFile(struct BankWalk *w, u32 offset, int kind)
{
    u8 *p = w->file + offset;

    if (kind == KIND_BOOK)
    {
        /* order * npredictors * 8 entries, measured to lie in the file */
        portBankNative(p + ROM_BANK_BOOK_HEAD, bankBe32(w, offset) * bankBe32(w, offset + 4) * 8, 2);
        portBankNative(p, 2, 4);
        return;
    }
    portBankNative(p, 3, 4); /* an envelope's three times, a loop's start, end and count */
    if (kind == KIND_LOOP)
    {
        portBankNative(p + 12, 16, 2);
    }
}
