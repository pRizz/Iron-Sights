/**
 * The audio bank converter (port_romconv.h; LP64-004 Task 5).
 *
 * A bank file (sfxctl, instrumentsctl) has five kinds of record with 4-byte
 * offsets: the header, banks, instruments, sounds and wave tables. At 32 bits
 * alBnkfNew (audio_stubs.c) rewrites the offsets in place as addresses; at 64
 * bits the records grow, so they are built in new memory, each distinct one
 * once, while envelopes, key maps, ADPCM books and loops stay in the file,
 * turned to native byte order there once each, as alBnkfNew does.
 *
 * Measuring and writing walk the graph in one order, so measuring's places
 * are writing's. The offset map holds each record reached, its kind in the
 * low 3 bits of its new offset (an in-file record's entry is its kind alone).
 * Compiled only at 64 bits and in the 32-bit oracle build
 * (romconv_bank_oracle.c compares the two graphs there).
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include <PR/libaudio.h>
#include "platform_info.h"
#include "port_romconv.h"
#include "port_romlayout.h"

#define BANK_FILE "ctl image"
#define BANK_ROUND8(n) (((n) + 7u) & ~7u)
/* The run-time bytes of a record whose pointer ARRAY has COUNT entries. */
#define BANK_ARRAY_BYTES(type, array, count) ((u32) offsetof(type, array) + (u32) (count) * (u32) sizeof(void *))

enum BankKind
{
    KIND_RAWLOOP, /* 0: in-file kinds are stored as their kind alone */
    KIND_BANK,
    KIND_INST,
    KIND_SOUND,
    KIND_WAVE,
    KIND_ENVELOPE,
    KIND_BOOK,
    KIND_LOOP,
    KIND_KEYMAP, /* checked, never stored: nothing in it is converted */
    KIND_HEADER  /* the file's header and bank list, named in messages */
};

static const char *const kKindNames[] = {
    "raw loop", "bank", "instrument", "sound", "wave table", "envelope", "ADPCM book", "ADPCM loop", "key map",
    "bank list",
};

/* The bytes of each in-file kind, by kind (0 for the built ones). */
static const u32 kInFileBytes[] = {ROM_BANK_RAWLOOP_BYTES, 0, 0, 0, 0, ROM_BANK_ENVELOPE_BYTES, ROM_BANK_BOOK_HEAD,
    ROM_BANK_ADPCMLOOP_BYTES, ROM_BANK_KEYMAP_BYTES};

/* Whether KIND's records are built in OUT; the others stay in the file. */
#define bankKindIsBuilt(kind) ((kind) >= KIND_BANK && (kind) <= KIND_WAVE)
struct BankWalk
{
    u8 *file;
    u32 size;
    u8 *table;
    u8 *out; /* NULL while measuring */
    u32 cursor;
    struct PortConvMap map;
};

#define bankRefuse(...) portConvRefuse(PORT_LP64_AUDIO_BANKS, BANK_FILE, __VA_ARGS__)
#define bankBe32(w, at) portConvBe32((w)->file + (at))
#define bankBe16(w, at) ((s16) portConvBe16((w)->file + (at)))

/* Refuses a record of KIND at OFFSET that is not 4-byte aligned or whose
 * BYTES do not lie in the file. */
static int bankFits(const struct BankWalk *w, u32 offset, u64 bytes, int kind)
{
    if (offset % 4 != 0)
    {
        bankRefuse("the %s offset 0x%X is not a multiple of 4", kKindNames[kind], offset);
        return 0;
    }
    if (offset > w->size || bytes > w->size - offset)
    {
        bankRefuse("the %s at 0x%X needs 0x%llX bytes, past the 0x%X-byte file", kKindNames[kind], offset,
                (unsigned long long) bytes, w->size);
        return 0;
    }
    return 1;
}

/* Refuses a negative COUNT in the record of KIND at OFFSET. */
static int bankCountValid(s32 count, u32 offset, int kind)
{
    if (count >= 0)
    {
        return 1;
    }
    bankRefuse("the %s at 0x%X has count %d", kKindNames[kind], offset, (int) count);
    return 0;
}

/* Checks the record of KIND at OFFSET against the file and gives in *BYTES
 * the bytes it takes in OUT (0 for an in-file kind). Reads only. */
static int bankMeasure(const struct BankWalk *w, u32 offset, int kind, u32 *bytes)
{
    s32 count, order, npredictors;

    *bytes = 0;
    switch (kind)
    {
    case KIND_BANK:
    case KIND_INST:
    {
        u32 head = kind == KIND_BANK ? sizeof(RomBank) : sizeof(RomInstrument);
        u32 field = kind == KIND_BANK ? offsetof(RomBank, instCount) : offsetof(RomInstrument, soundCount);

        if (!bankFits(w, offset, head, kind))
        {
            return 0;
        }
        count = bankBe16(w, offset + field);
        if (!bankCountValid(count, offset, kind) || !bankFits(w, offset, head + (u64) count * 4, kind))
        {
            return 0;
        }
        *bytes = kind == KIND_BANK ? BANK_ARRAY_BYTES(ALBank, instArray, count)
                                   : BANK_ARRAY_BYTES(ALInstrument, soundArray, count);
        return 1;
    }
    case KIND_SOUND:
        *bytes = sizeof(ALSound);
        return bankFits(w, offset, sizeof(RomSound), kind);
    case KIND_WAVE:
        *bytes = sizeof(ALWaveTable);
        return bankFits(w, offset, sizeof(RomWaveTable), kind);
    default: /* the in-file kinds; a book's entries follow its header */
        if (!bankFits(w, offset, kInFileBytes[kind], kind))
        {
            return 0;
        }
        if (kind != KIND_BOOK)
        {
            return 1;
        }
        order = (s32) bankBe32(w, offset);
        npredictors = (s32) bankBe32(w, offset + 4);
        if (order < 0 || npredictors < 0)
        {
            bankRefuse("the ADPCM book at 0x%X has order %d and %d predictors", offset, (int) order, (int) npredictors);
            return 0;
        }
        return bankFits(w, offset, ROM_BANK_BOOK_HEAD + (u64) order * (u64) npredictors * 16, kind);
    }
}

/* Reaches the record of KIND at OFFSET. A record seen before must be of the
 * same kind; one not seen is checked and, if built, given the next place in
 * OUT. *TO is that place; *FIRST says whether this is the first reach. */
static int bankReach(struct BankWalk *w, u32 offset, int kind, u32 *to, int *first)
{
    u32 entry, bytes;

    *first = 0;
    *to = 0;
    if (portConvMapGet(&w->map, offset, &entry))
    {
        if ((int) (entry & 7) != kind)
        {
            bankRefuse("offset 0x%X is reached as two kinds of record: %s and %s", offset, kKindNames[entry & 7],
                    kKindNames[kind]);
            return 0;
        }
        *to = entry & ~7u;
        return 1;
    }
    if (!bankMeasure(w, offset, kind, &bytes))
    {
        return 0;
    }
    if (bytes > 0xFFFFFFF0u - w->cursor)
    {
        bankRefuse("the run-time records pass 4 GB at the %s at 0x%X", kKindNames[kind], offset);
        return 0;
    }
    if (bankKindIsBuilt(kind))
    {
        *to = w->cursor;
        w->cursor += BANK_ROUND8(bytes);
    }
    portConvMapPut(&w->map, offset, *to | (u32) kind);
    *first = 1;
    return 1;
}

/* Rewrites COUNT big-endian values of WIDTH bytes (2 or 4) at P in native
 * order. Only while writing. */
static void bankNative(u8 *p, u32 count, u32 width)
{
    u32 i;

    for (i = 0; i < count; i++, p += width)
    {
        u32 v32 = width == 4 ? portConvBe32(p) : portConvBe16(p);
        u16 v16 = (u16) v32;

        memcpy(p, width == 4 ? (const void *) &v32 : (const void *) &v16, width);
    }
}

/* Turns the in-file record of KIND at OFFSET to native order, as alBnkfNew
 * does. Called once a record, while writing. */
static void bankNativeInFile(struct BankWalk *w, u32 offset, int kind)
{
    u8 *p = w->file + offset;

    if (kind == KIND_BOOK)
    {
        /* order * npredictors * 8 entries, measured to lie in the file */
        bankNative(p + ROM_BANK_BOOK_HEAD, bankBe32(w, offset) * bankBe32(w, offset + 4) * 8, 2);
        bankNative(p, 2, 4);
        return;
    }
    bankNative(p, 3, 4); /* an envelope's three times, a loop's start, end and count */
    if (kind == KIND_LOOP)
    {
        bankNative(p + 12, 16, 2);
    }
}

static int bankLink(struct BankWalk *w, u32 offset, int kind, void **ptr);

/* Writes (while writing) the record of KIND from file offset OFFSET at OUT +
 * TO, linking its references. Each reads the file record, never OUT. */
static int bankWriteBank(struct BankWalk *w, u32 offset, u32 to)
{
    ALBank *bank = w->out != NULL ? (ALBank *) (w->out + to) : NULL;
    s16 count = bankBe16(w, offset + offsetof(RomBank, instCount));
    u32 percussion = bankBe32(w, offset + offsetof(RomBank, percussion));
    void *p = NULL;
    s16 i;

    if (bank != NULL)
    {
        bank->instCount = count;
        bank->flags = 1;
        bank->pad = w->file[offset + offsetof(RomBank, pad)];
        bank->sampleRate = (s32) bankBe32(w, offset + offsetof(RomBank, sampleRate));
    }
    if (percussion != 0 && !bankLink(w, percussion, KIND_INST, &p))
    {
        return 0;
    }
    if (bank != NULL)
    {
        bank->percussion = (ALInstrument *) p; /* NULL for none */
    }
    for (i = 0; i < count; i++)
    {
        u32 inst = bankBe32(w, offset + sizeof(RomBank) + (u32) i * 4);

        p = w->file; /* an offset of 0 names the file's start and is not followed */
        if (inst != 0 && !bankLink(w, inst, KIND_INST, &p))
        {
            return 0;
        }
        if (bank != NULL)
        {
            bank->instArray[i] = (ALInstrument *) p;
        }
    }
    return 1;
}

static int bankWriteInst(struct BankWalk *w, u32 offset, u32 to)
{
    ALInstrument *inst = w->out != NULL ? (ALInstrument *) (w->out + to) : NULL;
    s16 count = bankBe16(w, offset + offsetof(RomInstrument, soundCount));
    void *p;
    s16 i;

    if (inst != NULL)
    {
        /* volume to vibDelay: twelve bytes in both layouts */
        memcpy(inst, w->file + offset, offsetof(RomInstrument, bendRange));
        inst->flags = 1;
        inst->bendRange = bankBe16(w, offset + offsetof(RomInstrument, bendRange));
        inst->soundCount = count;
    }
    for (i = 0; i < count; i++)
    {
        if (!bankLink(w, bankBe32(w, offset + sizeof(RomInstrument) + (u32) i * 4), KIND_SOUND, &p))
        {
            return 0;
        }
        if (inst != NULL)
        {
            inst->soundArray[i] = (ALSound *) p;
        }
    }
    return 1;
}

static int bankWriteSound(struct BankWalk *w, u32 offset, u32 to)
{
    ALSound *sound = w->out != NULL ? (ALSound *) (w->out + to) : NULL;
    void *envelope, *keymap, *wave;

    if (!bankLink(w, bankBe32(w, offset + offsetof(RomSound, envelope)), KIND_ENVELOPE, &envelope)
            || !bankLink(w, bankBe32(w, offset + offsetof(RomSound, keyMap)), KIND_KEYMAP, &keymap)
            || !bankLink(w, bankBe32(w, offset + offsetof(RomSound, wavetable)), KIND_WAVE, &wave))
    {
        return 0;
    }
    if (sound != NULL)
    {
        sound->envelope = (ALEnvelope *) envelope;
        sound->keyMap = (ALKeyMap *) keymap;
        sound->wavetable = (ALWaveTable *) wave;
        sound->samplePan = w->file[offset + offsetof(RomSound, samplePan)];
        sound->sampleVolume = w->file[offset + offsetof(RomSound, sampleVolume)];
        sound->flags = 1;
    }
    return 1;
}

/* Whether a raw wave table other than the one at WAVE, reached before it, has
 * its samples at BASE: alBnkfNew swaps each base's samples once. Every wave
 * table in the map has been written, since each is written when first reached. */
static int bankRawBaseSwapped(const struct BankWalk *w, u32 wave, u32 base)
{
    u32 slot;

    for (slot = 0; slot < w->map.slots; slot++)
    {
        u32 at = slot * 4;

        if (w->map.present[slot] && (w->map.newOffset[slot] & 7) == KIND_WAVE && at != wave
                && w->file[at + offsetof(RomWaveTable, type)] == AL_RAW16_WAVE
                && bankBe32(w, at + offsetof(RomWaveTable, base)) == base)
        {
            return 1;
        }
    }
    return 0;
}

static int bankWriteWave(struct BankWalk *w, u32 offset, u32 to)
{
    ALWaveTable *wave = w->out != NULL ? (ALWaveTable *) (w->out + to) : NULL;
    u32 base = bankBe32(w, offset + offsetof(RomWaveTable, base));
    u32 len = bankBe32(w, offset + offsetof(RomWaveTable, len));
    u8 type = w->file[offset + offsetof(RomWaveTable, type)];
    u32 loop = bankBe32(w, offset + offsetof(RomWaveTable, loop));
    u32 book = bankBe32(w, offset + offsetof(RomWaveTable, book));
    void *lp = NULL, *bp = NULL;

    if (type == AL_ADPCM_WAVE
            && ((book != 0 && !bankLink(w, book, KIND_BOOK, &bp)) || (loop != 0 && !bankLink(w, loop, KIND_LOOP, &lp))))
    {
        return 0;
    }
    if (type == AL_RAW16_WAVE && loop != 0 && !bankLink(w, loop, KIND_RAWLOOP, &lp))
    {
        return 0;
    }
    if (wave == NULL)
    {
        return 1; /* measuring */
    }
    wave->base = w->table + base;
    wave->len = (s32) len;
    wave->type = type;
    wave->flags = 1;
    if (type == AL_ADPCM_WAVE)
    {
        wave->waveInfo.adpcmWave.loop = (ALADPCMloop *) lp;
        wave->waveInfo.adpcmWave.book = (ALADPCMBook *) bp;
    }
    else if (type == AL_RAW16_WAVE)
    {
        /* The mixer plays native s16; ADPCM is decoded from bytes. */
        wave->waveInfo.rawWave.loop = (ALRawLoop *) lp;
        s32 samples = (s32) len >> 1;

        if (samples > 0 && !bankRawBaseSwapped(w, offset, base))
        {
            bankNative(wave->base, (u32) samples, 2);
        }
    }
    return 1;
}

/* Reaches the record of KIND at OFFSET, writes it on its first reach, and
 * gives in *PTR its run-time address (NULL while measuring a built kind). */
static int bankLink(struct BankWalk *w, u32 offset, int kind, void **ptr)
{
    u32 to;
    int first, ok = 1;

    if (kind == KIND_KEYMAP)
    {
        *ptr = w->file + offset;
        return bankMeasure(w, offset, kind, &to);
    }
    if (!bankReach(w, offset, kind, &to, &first))
    {
        return 0;
    }
    if (!bankKindIsBuilt(kind))
    {
        *ptr = w->file + offset;
        if (first && w->out != NULL)
        {
            bankNativeInFile(w, offset, kind);
        }
        return 1;
    }
    *ptr = w->out != NULL ? (void *) (w->out + to) : NULL;
    if (first)
    {
        ok = kind == KIND_BANK    ? bankWriteBank(w, offset, to)
           : kind == KIND_INST    ? bankWriteInst(w, offset, to)
           : kind == KIND_SOUND   ? bankWriteSound(w, offset, to)
                                  : bankWriteWave(w, offset, to);
    }
    return ok;
}

/* Measures (OUT NULL) or writes the whole graph; W->cursor ends as its size. */
static int bankWalk(struct BankWalk *w)
{
    ALBankFile *f = (ALBankFile *) w->out;
    s16 count, i;
    int ok = 1;

    if (w->size < sizeof(RomBankFile) || portConvBe16(w->file) != AL_BANK_VERSION)
    {
        bankRefuse("revision 0x%04X, not 0x%04X (B1)", w->size < 2 ? 0u : portConvBe16(w->file), AL_BANK_VERSION);
        return 0;
    }
    count = bankBe16(w, offsetof(RomBankFile, bankCount));
    if (!bankCountValid(count, 0, KIND_HEADER) || !bankFits(w, 0, sizeof(RomBankFile) + (u64) count * 4, KIND_HEADER))
    {
        return 0;
    }
    if (!portConvMapInit(&w->map, w->size))
    {
        bankRefuse("no offset map for the %u-byte file", w->size);
        return 0;
    }
    w->cursor = BANK_ROUND8(BANK_ARRAY_BYTES(ALBankFile, bankArray, count));
    if (f != NULL)
    {
        f->revision = AL_BANK_VERSION;
        f->bankCount = count;
    }
    for (i = 0; ok && i < count; i++)
    {
        u32 offset = bankBe32(w, sizeof(RomBankFile) + (u32) i * 4);
        void *p = w->file; /* an offset of 0 names the file's start and is not followed */

        ok = offset == 0 || bankLink(w, offset, KIND_BANK, &p);
        if (ok && f != NULL)
        {
            f->bankArray[i] = (ALBank *) p;
        }
    }
    portConvMapFree(&w->map);
    return ok;
}

u32 portBankConvertedSize(const u8 *file, u32 size)
{
    struct BankWalk w = {(u8 *) file, size, NULL, NULL, 0, {NULL, NULL, 0}}; /* measuring writes nothing */

    return bankWalk(&w) ? w.cursor : 0;
}

ALBankFile *portBankConvert(u8 *file, u32 size, u8 *table, u8 *out, u32 outsize)
{
    struct BankWalk w = {file, size, table, out, 0, {NULL, NULL, 0}};
    u32 needed = portBankConvertedSize(file, size);

    if (needed == 0)
    {
        return NULL;
    }
    if (needed > outsize)
    {
        bankRefuse("the run-time records need %u bytes, the buffer has %u", needed, outsize);
        return NULL;
    }
    if ((uintptr_t) out % 8 != 0)
    {
        bankRefuse("the output buffer at %p is not 8-byte aligned", (void *) out);
        return NULL;
    }
    memset(out, 0, needed);
    /* Measured above, so the walk refuses nothing and ends at NEEDED. */
    return bankWalk(&w) ? (ALBankFile *) out : NULL;
}

