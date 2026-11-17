/**
 * The audio bank converter's walk (LP64-004 Task 5), shared by
 * romconv_bank.c (the graph walk and the public functions) and
 * romconv_bank_records.c (each record's checks and the in-file kinds' byte
 * order). Not part of port_romconv.h's interface; compiled only where the
 * converters are (64 bits and the 32-bit oracle build).
 */
#ifndef ROMCONV_BANK_WALK_H
#define ROMCONV_BANK_WALK_H

#include <stddef.h>
#include <PR/ultratypes.h>
#include "port_romconv_audio.h"

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
    KIND_KEYMAP, /* never in the map: nothing in it is converted */
    KIND_HEADER  /* the file's header and bank list */
};

/* The run-time bytes of a record whose pointer ARRAY has COUNT entries. */
#define BANK_ARRAY_BYTES(type, array, count) ((u32) offsetof(type, array) + (u32) (count) * (u32) sizeof(void *))

/* Whether KIND's records are built in OUT; the others stay in the file. */
#define bankKindIsBuilt(kind) ((kind) >= KIND_BANK && (kind) <= KIND_WAVE)

/** Each kind's name, for refusals. */
extern const char *const kPortBankKindNames[];

struct BankWalk
{
    u8 *file;
    u32 size;
    u8 *table; /* NULL while measuring */
    u32 tablesize;
    const char *name; /* the bank, as the caller names it in refusals */
    u8 *out;          /* NULL while measuring */
    u32 cursor;       /* the bytes of OUT given out so far */
    u32 limit;        /* while writing, the bytes measured */
    struct PortConvMap map;
    u32 *owner;    /* per 4 bytes of the file: 1 + the first slot of the record covering them, or 0 */
    u8 *ownerKind; /* that record's kind */
    u8 *rawBases;  /* one bit per byte of the table: a raw wave with that base was reached */
};

#define bankRefuse(w, ...) portConvRefuse(PORT_LP64_AUDIO_BANKS, (w)->name, __VA_ARGS__)
#define bankBe32(w, at) portConvBe32((w)->file + (at))
#define bankBe16(w, at) ((s16) portConvBe16((w)->file + (at)))

/** Refuses a record of KIND at OFFSET that is not 4-byte aligned or whose
 * BYTES do not lie in the file. */
int portBankFits(const struct BankWalk *w, u32 offset, u64 bytes, int kind);

/** Refuses a negative COUNT in the record of KIND at OFFSET. */
int portBankCountValid(const struct BankWalk *w, s32 count, u32 offset, int kind);

/**
 * Checks the record of KIND at OFFSET against the file (and a wave's samples
 * against the table), giving in *BYTES what it takes in OUT (0 for an in-file
 * kind) and in *EXTENT the file bytes it covers, its offset arrays included.
 * Reads only.
 */
int portBankMeasure(const struct BankWalk *w, u32 offset, int kind, u32 *bytes, u32 *extent);

/** Gives W its owner slots and raw-base bits, all clear, from the converters'
 * scratch buffer; refuses a file and table too large for them. */
int portBankChecksInit(struct BankWalk *w);

/**
 * Marks the EXTENT file bytes at OFFSET as the record of KIND's. Refuses, naming
 * both, when a byte already belongs to another record: records that overlap
 * would see each other's in-file byte swaps. The same record claimed again (a
 * key map, which is not in the map) is accepted.
 */
int portBankClaim(struct BankWalk *w, u32 offset, u32 extent, int kind);

/** 1 when no raw wave with BASE (at most the table's size) was reached
 * before, marking it; alBnkfNew swaps each base's samples once. */
int portBankRawBaseFirst(struct BankWalk *w, u32 base);

/** Rewrites COUNT big-endian values of WIDTH bytes (2 or 4) at P in native order. */
void portBankNative(u8 *p, u32 count, u32 width);

/** Turns the in-file record of KIND at OFFSET to native order, as alBnkfNew does. */
void portBankNativeInFile(struct BankWalk *w, u32 offset, int kind);

#endif
