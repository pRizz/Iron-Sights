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
 * are writing's: no two records may share a byte (portBankClaim,
 * romconv_bank_records.c), so the byte swaps the write pass makes in the file
 * never change a byte it reads later. The offset map holds each record
 * reached, its kind in the low 3 bits of its new offset (an in-file record's
 * entry is its kind alone). Compiled only at 64 bits and in the 32-bit oracle
 * build (romconv_bank_oracle.c compares the two graphs there).
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include <PR/libaudio.h>
#include "platform_info.h"
#include "port_romconv.h"
#include "port_romlayout.h"
#include "romconv_bank_walk.h"

#define BANK_ROUND8(n) (((n) + 7u) & ~7u)

/* Reaches the record of KIND at OFFSET. A record seen before must be of the
 * same kind; one not seen is checked, claims its file bytes and, if built, is
 * given the next place in OUT. *TO is that place; *FIRST says whether this is
 * the first reach. */
static int bankReach(struct BankWalk *w, u32 offset, int kind, u32 *to, int *first)
{
    u32 entry, bytes, extent;

    *first = 0;
    *to = 0;
    if (portConvMapGet(&w->map, offset, &entry))
    {
        if ((int) (entry & 7) != kind)
        {
            bankRefuse(w, "offset 0x%X is reached as two kinds of record: %s and %s", offset,
                    kPortBankKindNames[entry & 7], kPortBankKindNames[kind]);
            return 0;
        }
        *to = entry & ~7u;
        return 1;
    }
    if (!portBankMeasure(w, offset, kind, &bytes, &extent) || !portBankClaim(w, offset, extent, kind))
    {
        return 0;
    }
    if (bytes > 0xFFFFFFF0u - w->cursor)
    {
        bankRefuse(w, "the run-time records pass 4 GB at the %s at 0x%X", kPortBankKindNames[kind], offset);
        return 0;
    }
    /* A backstop: the claims keep the write pass's bytes the measured ones. */
    if (w->out != NULL && BANK_ROUND8(bytes) > w->limit - w->cursor)
    {
        bankRefuse(w, "the run-time records outgrow the %u bytes measured at the %s at 0x%X", w->limit,
                kPortBankKindNames[kind], offset);
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
        /* The mixer plays native s16; ADPCM is decoded from bytes. The
         * samples were measured to lie in the table. */
        wave->waveInfo.rawWave.loop = (ALRawLoop *) lp;
        if (portBankRawBaseFirst(w, base))
        {
            portBankNative(wave->base, len >> 1, 2);
        }
    }
    return 1;
}

/* Reaches the record of KIND at OFFSET, writes it on its first reach, and
 * gives in *PTR its run-time address (NULL while measuring a built kind). */
static int bankLink(struct BankWalk *w, u32 offset, int kind, void **ptr)
{
    u32 to, extent;
    int first, ok = 1;

    if (kind == KIND_KEYMAP)
    {
        *ptr = w->file + offset;
        return portBankMeasure(w, offset, kind, &to, &extent) && portBankClaim(w, offset, extent, kind);
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
            portBankNativeInFile(w, offset, kind);
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

/* Checks the header and bank list, which claim their bytes, and sets W up. */
static int bankWalkStart(struct BankWalk *w, s16 *count)
{
    if (w->size < sizeof(RomBankFile) || portConvBe16(w->file) != AL_BANK_VERSION)
    {
        bankRefuse(w, "revision 0x%04X, not 0x%04X (B1)", w->size < 2 ? 0u : portConvBe16(w->file), AL_BANK_VERSION);
        return 0;
    }
    *count = bankBe16(w, offsetof(RomBankFile, bankCount));
    if (!portBankCountValid(w, *count, 0, KIND_HEADER)
            || !portBankFits(w, 0, sizeof(RomBankFile) + (u64) *count * 4, KIND_HEADER) || !portBankChecksInit(w)
            || !portBankClaim(w, 0, sizeof(RomBankFile) + (u32) *count * 4, KIND_HEADER))
    {
        return 0;
    }
    if (!portConvMapInit(&w->map, w->size))
    {
        bankRefuse(w, "no offset map for the %u-byte file", w->size);
        return 0;
    }
    w->cursor = BANK_ROUND8(BANK_ARRAY_BYTES(ALBankFile, bankArray, *count));
    return 1;
}

/* Measures (OUT NULL) or writes the whole graph; W->cursor ends as its size. */
static int bankWalk(struct BankWalk *w)
{
    ALBankFile *f = (ALBankFile *) w->out;
    s16 count, i;
    int ok = 1;

    if (!bankWalkStart(w, &count))
    {
        return 0;
    }
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

u32 portBankConvertedSize(const u8 *file, u32 size, u32 tablesize, const char *name)
{
    struct BankWalk w;

    memset(&w, 0, sizeof(w)); /* OUT and TABLE NULL: measuring writes nothing */
    w.file = (u8 *) file;
    w.size = size;
    w.tablesize = tablesize;
    w.name = name;
    return bankWalk(&w) ? w.cursor : 0;
}

ALBankFile *portBankConvert(u8 *file, u32 size, u8 *table, u32 tablesize, u8 *out, u32 outsize, const char *name)
{
    struct BankWalk w;
    u32 needed = portBankConvertedSize(file, size, tablesize, name);

    memset(&w, 0, sizeof(w));
    w.file = file;
    w.size = size;
    w.table = table;
    w.tablesize = tablesize;
    w.name = name;
    w.out = out;
    w.limit = needed;
    if (needed == 0)
    {
        return NULL;
    }
    if (needed > outsize)
    {
        bankRefuse(&w, "the run-time records need %u bytes, the buffer has %u", needed, outsize);
        return NULL;
    }
    if ((uintptr_t) out % 8 != 0)
    {
        bankRefuse(&w, "the output buffer at %p is not 8-byte aligned", (void *) out);
        return NULL;
    }
    memset(out, 0, needed);
    /* The claims keep every record's bytes as measured, so this walk refuses
     * nothing and ends at NEEDED; bankReach stops it at NEEDED regardless. */
    return bankWalk(&w) ? (ALBankFile *) out : NULL;
}
