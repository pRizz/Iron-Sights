/**
 * The audio bank oracle (port_romconv.h; LP64-004 Task 5): the comparison of
 * the graph alBnkfNew builds in place with the one portBankConvert builds.
 * The two are different bytes even at 32 bits (one lies in the file, the
 * other in new memory), so they are walked together from the bank file down.
 *
 * Its own file because romconv_bank.c holds the converter at its 500-line
 * limit. Compiled with the other romconv_ sources, and empty outside the
 * 32-bit oracle build (-DGE_LP64_ORACLE=ON).
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <PR/libaudio.h>
#include "port_romconv_audio.h"
#include "port_romlayout_audio.h"
#include "port_lp64_oracle.h"

#ifdef GE_LP64_ORACLE

/* The kinds of record, named in the verdict; compared by address. */
static const char kHeader[] = "bank list", kBank[] = "bank", kInst[] = "instrument", kSound[] = "sound",
                  kWave[] = "wave table", kEnvelope[] = "envelope", kKeyMap[] = "key map", kBook[] = "ADPCM book",
                  kLoop[] = "ADPCM loop", kRawLoop[] = "raw loop";

struct BankCompare
{
    const struct PortBankGraph *a;
    const struct PortBankGraph *b;
    char *why;
    u32 whysize;
};

static int cmpDiffer(struct BankCompare *c, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static int cmpDiffer(struct BankCompare *c, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(c->why, c->whysize, fmt, ap);
    va_end(ap);
    return 0;
}

/* Whether SIZE bytes at A and B are equal (the game's <string.h> declares no
 * memcmp). */
static int cmpSameBytes(const void *a, const void *b, u32 size)
{
    const u8 *x = (const u8 *) a, *y = (const u8 *) b;
    u32 i;

    for (i = 0; i < size && x[i] == y[i]; i++)
    {
    }
    return i == size;
}

/* The offset of P in G's file when BYTES from P lie in it, else -1. */
static s64 cmpFileOffset(const struct PortBankGraph *g, const void *p, u32 bytes)
{
    uintptr_t at = (uintptr_t) p, file = (uintptr_t) g->file;

    if (at < file || at - file > g->size || bytes > g->size - (at - file))
    {
        return -1;
    }
    return (s64) (at - file);
}

/* Pointers to an in-file record of the kind WHAT: both NULL, or at one offset of their
 * files with the same BYTES there. A book's bytes follow from A's header. */
static int cmpInFile(struct BankCompare *c, const void *pa, const void *pb, u32 bytes, const char *what)
{
    s64 oa, ob;

    if (pa == NULL || pb == NULL)
    {
        return (pa == NULL && pb == NULL) || cmpDiffer(c, "a %s pointer is NULL in one graph only", what);
    }
    oa = cmpFileOffset(c->a, pa, bytes);
    if (what == kBook && oa >= 0)
    {
        const ALADPCMBook *book = (const ALADPCMBook *) pa;
        u64 total = ROM_BANK_BOOK_HEAD + (u64) book->order * (u64) book->npredictors * 16;

        bytes = book->order < 0 || book->npredictors < 0 || total > c->a->size ? c->a->size + 1 : (u32) total;
        oa = cmpFileOffset(c->a, pa, bytes);
    }
    ob = cmpFileOffset(c->b, pb, bytes);
    if (oa < 0 || ob < 0 || oa != ob)
    {
        return cmpDiffer(c, "the %s is at file offsets %lld and %lld", what, (long long) oa, (long long) ob);
    }
    if (!cmpSameBytes(pa, pb, bytes))
    {
        return cmpDiffer(c, "the %s at 0x%llX holds different bytes", what, (unsigned long long) oa);
    }
    return 1;
}

/* Returns from the calling comparison when FIELD of X and Y differ. */
#define CMP_FIELD(what, x, y, field)                                                                              \
    if ((x)->field != (y)->field)                                                                              \
    return cmpDiffer(c, "the %s's %s: %lld and %lld", what, #field, (long long) (x)->field, (long long) (y)->field)

static int cmpBuilt(struct BankCompare *c, const void *pa, const void *pb, const char *what);

static int cmpBank(struct BankCompare *c, const ALBank *x, const ALBank *y)
{
    s16 i;

    CMP_FIELD(kBank, x, y, instCount);
    CMP_FIELD(kBank, x, y, flags);
    CMP_FIELD(kBank, x, y, pad);
    CMP_FIELD(kBank, x, y, sampleRate);
    if (!cmpBuilt(c, x->percussion, y->percussion, kInst))
    {
        return 0;
    }
    for (i = 0; i < x->instCount; i++)
    {
        if (!cmpBuilt(c, x->instArray[i], y->instArray[i], kInst))
        {
            return 0;
        }
    }
    return 1;
}

static int cmpInst(struct BankCompare *c, const ALInstrument *x, const ALInstrument *y)
{
    s16 i;

    CMP_FIELD(kInst, x, y, volume);
    CMP_FIELD(kInst, x, y, pan);
    CMP_FIELD(kInst, x, y, priority);
    CMP_FIELD(kInst, x, y, flags);
    CMP_FIELD(kInst, x, y, tremType);
    CMP_FIELD(kInst, x, y, tremRate);
    CMP_FIELD(kInst, x, y, tremDepth);
    CMP_FIELD(kInst, x, y, tremDelay);
    CMP_FIELD(kInst, x, y, vibType);
    CMP_FIELD(kInst, x, y, vibRate);
    CMP_FIELD(kInst, x, y, vibDepth);
    CMP_FIELD(kInst, x, y, vibDelay);
    CMP_FIELD(kInst, x, y, bendRange);
    CMP_FIELD(kInst, x, y, soundCount);
    for (i = 0; i < x->soundCount; i++)
    {
        if (!cmpBuilt(c, x->soundArray[i], y->soundArray[i], kSound))
        {
            return 0;
        }
    }
    return 1;
}

static int cmpSound(struct BankCompare *c, const ALSound *x, const ALSound *y)
{
    CMP_FIELD(kSound, x, y, samplePan);
    CMP_FIELD(kSound, x, y, sampleVolume);
    CMP_FIELD(kSound, x, y, flags);
    return cmpInFile(c, x->envelope, y->envelope, ROM_BANK_ENVELOPE_BYTES, kEnvelope)
        && cmpInFile(c, x->keyMap, y->keyMap, ROM_BANK_KEYMAP_BYTES, kKeyMap)
        && cmpBuilt(c, x->wavetable, y->wavetable, kWave);
}

static int cmpWave(struct BankCompare *c, const ALWaveTable *x, const ALWaveTable *y)
{
    uintptr_t bx = (uintptr_t) x->base - (uintptr_t) c->a->table;
    uintptr_t by = (uintptr_t) y->base - (uintptr_t) c->b->table;

    CMP_FIELD(kWave, x, y, len);
    CMP_FIELD(kWave, x, y, type);
    CMP_FIELD(kWave, x, y, flags);
    if (bx != by)
    {
        return cmpDiffer(c, "the wave table's base: table offsets 0x%llX and 0x%llX", (unsigned long long) bx,
                (unsigned long long) by);
    }
    if (x->type == AL_ADPCM_WAVE)
    {
        return cmpInFile(c, x->waveInfo.adpcmWave.loop, y->waveInfo.adpcmWave.loop, ROM_BANK_ADPCMLOOP_BYTES, kLoop)
            && cmpInFile(c, x->waveInfo.adpcmWave.book, y->waveInfo.adpcmWave.book, ROM_BANK_BOOK_HEAD, kBook);
    }
    if (x->type == AL_RAW16_WAVE)
    {
        return cmpInFile(c, x->waveInfo.rawWave.loop, y->waveInfo.rawWave.loop, ROM_BANK_RAWLOOP_BYTES, kRawLoop);
    }
    return 1;
}

/* Pointers to a built record of the kind WHAT: both NULL, both their file's
 * start (an offset of 0), or records that compare equal. B's record must not
 * lie elsewhere in its file, where only the unconverted bytes are. */
static int cmpBuilt(struct BankCompare *c, const void *pa, const void *pb, const char *what)
{
    int astart = (const u8 *) pa == c->a->file, bstart = (const u8 *) pb == c->b->file;

    if (pa == NULL || pb == NULL)
    {
        return (pa == NULL && pb == NULL) || cmpDiffer(c, "a %s pointer is NULL in one graph only", what);
    }
    if (astart || bstart)
    {
        return (astart && bstart) || cmpDiffer(c, "one %s pointer is the file's start, the other is not", what);
    }
    if (cmpFileOffset(c->b, pb, 1) >= 0)
    {
        return cmpDiffer(c, "the second graph's %s at file offset 0x%llX was left in the file", what,
                (unsigned long long) cmpFileOffset(c->b, pb, 1));
    }
    if (what == kBank)
    {
        return cmpBank(c, (const ALBank *) pa, (const ALBank *) pb);
    }
    if (what == kInst)
    {
        return cmpInst(c, (const ALInstrument *) pa, (const ALInstrument *) pb);
    }
    if (what == kSound)
    {
        return cmpSound(c, (const ALSound *) pa, (const ALSound *) pb);
    }
    return cmpWave(c, (const ALWaveTable *) pa, (const ALWaveTable *) pb);
}

int portBankGraphsEqual(const struct PortBankGraph *a, const struct PortBankGraph *b, char *why, u32 whysize)
{
    struct BankCompare compare = {a, b, why, whysize};
    struct BankCompare *c = &compare;
    s16 i;

    CMP_FIELD(kHeader, a->root, b->root, revision);
    CMP_FIELD(kHeader, a->root, b->root, bankCount);
    for (i = 0; i < a->root->bankCount; i++)
    {
        if (!cmpBuilt(c, a->root->bankArray[i], b->root->bankArray[i], kBank))
        {
            return 0;
        }
    }
    return 1;
}

void portBankOracle(const char *name, const ALBankFile *inplace, const u8 *inplacetable, u8 *pristine, u32 size,
        u8 *tablecopy, u32 tablesize)
{
    char why[256];
    u32 needed = portBankConvertedSize(pristine, size, tablesize, name);
    u8 *out = needed != 0 ? (u8 *) malloc(needed) : NULL;
    ALBankFile *converted =
            out != NULL ? portBankConvert(pristine, size, tablecopy, tablesize, out, needed, name) : NULL;
    struct PortBankGraph a = {inplace, (const u8 *) inplace, size, inplacetable};
    struct PortBankGraph b = {converted, pristine, size, tablecopy};
    u32 i;

    if (converted == NULL)
    {
        portLp64OracleResult(PORT_LP64_AUDIO_BANKS, name, 0, "the converter refused the file, or no memory for it");
        free(out);
        return;
    }
    if (!portBankGraphsEqual(&a, &b, why, sizeof(why)))
    {
        portLp64OracleResult(PORT_LP64_AUDIO_BANKS, name, 0, why);
        free(out);
        return;
    }
    for (i = 0; i < tablesize && inplacetable[i] == tablecopy[i]; i++)
    {
    }
    if (i < tablesize)
    {
        snprintf(why, sizeof(why), "the graphs are equal, the wave tables differ at +0x%X", i);
    }
    else
    {
        snprintf(why, sizeof(why), "the graphs are equal from %u bytes of new records; the %u-byte wave tables too",
                needed, tablesize);
    }
    portLp64OracleResult(PORT_LP64_AUDIO_BANKS, name, i == tablesize, why);
    free(out);
}

#endif
