/**
 * The setup file converter's shared state (LP64-004 Task 14), used by
 * romconv_setup.c (the layout, the writing pass and the interface),
 * romconv_setup_walk.c (the claims of the header, the tables and the s32
 * lists), romconv_setup_records.c (the references and the table writers) and
 * romconv_setup_intro.c (the intro, the credits table and the prop list's
 * extent). Not part of port_romconv_stage.h's interface; compiled only where
 * the converters are (64 bits and the 32-bit oracle build).
 */
#ifndef ROMCONV_SETUP_WALK_H
#define ROMCONV_SETUP_WALK_H

#include <PR/ultratypes.h>
#include "port_romconv.h"

/*
 * What owns each 4 bytes of the file. An item's first slot holds its kind,
 * its other slots the kind with SETUP_INSIDE. The words of the s32 lists are
 * SETUP_LIST each, with no first slot: lists may share words, and a run of
 * them is copied as one block. Unowned slots (SETUP_RAW) hold the names, the
 * AI byte code and padding, and are copied as they are.
 */
enum SetupKind
{
    SETUP_RAW = 0,
    SETUP_HEADER,
    SETUP_WAYPOINTS,
    SETUP_WAYGROUPS,
    SETUP_PATHS,
    SETUP_AILISTS,
    SETUP_PADS,
    SETUP_BOUNDPADS,
    SETUP_PADNAMES,
    SETUP_BOUNDPADNAMES,
    SETUP_LIST,
    SETUP_INTRO,
    SETUP_CREDITS,
    SETUP_PROPS,
    SETUP_KIND_COUNT
};
#define SETUP_INSIDE 0x80u
#define SETUP_KIND(k) ((k) & 0x7Fu)

/** The ten header words, in RomSetupHeader's order. */
enum SetupField
{
    SF_WAYPOINTS,
    SF_WAYGROUPS,
    SF_INTRO,
    SF_PROPS,
    SF_PATHS,
    SF_AILISTS,
    SF_PADS,
    SF_BOUNDPADS,
    SF_PADNAMES,
    SF_BOUNDPADNAMES,
    SF_COUNT
};

/** What a byte reference names: a name ends at its NUL, an AI list at AI_EndList. */
enum SetupBytes
{
    SB_NAME,
    SB_AILIST
};

struct SetupWalk
{
    const u8 *file;
    u32 size;
    const char *name; /* the file, for refusals */
    u32 header[SF_COUNT];
    u8 *kinds; /* one per 4 bytes of the file */
    u32 slots;
    struct PortConvMap map; /* each item's new offset; every slot of a list run and of unowned bytes */
    u32 total;              /* the image's bytes, once laid out */
    u8 *out;                /* NULL while checking */
};

/** One item of the file as the layout sees it. */
struct SetupItem
{
    u32 at;       /* file offset */
    u32 bytes;    /* its bytes in the file */
    u32 outBytes; /* its bytes in the image */
    u8 kind;
};

#define setupRefuse(w, ...) portConvRefuse(PORT_LP64_SETUPS, (w)->name, __VA_ARGS__)
#define setupBe32(w, at) portConvBe32((w)->file + (at))
#define setupBe16(w, at) portConvBe16((w)->file + (at))

/**
 * The file word at SRC swapped as romfile_setup.c's kindApply swaps a word of
 * KIND (setup_kinds.h's SETUP_KIND_), written to DST (romconv_setup_intro.c).
 */
void portSetupKindWord(const u8 *src, int kind, u8 *dst);

/** The name of an item of KIND, for refusals. */
const char *portSetupKindName(u8 kind);

/** Gives W its owner slots, all unowned; refuses when they cannot be had. */
int portSetupKindsInit(struct SetupWalk *w);

/**
 * Claims BYTES at AT for an item of KIND (romconv_setup_walk.c). Returns 1
 * for a first claim, 2 when the same item (kind, place and length) was
 * claimed before, 0 after refusing an item off the 4-byte grid, past the file
 * or over another item.
 */
int portSetupClaim(struct SetupWalk *w, u32 at, u64 bytes, u8 kind, const char *what);

/** Claims the header, every table and every s32 list they name (romconv_setup_walk.c). */
int portSetupClaimTables(struct SetupWalk *w);

/** Claims the intro, the credits tables it names and the prop list (romconv_setup_intro.c). */
int portSetupClaimIntroAndProps(struct SetupWalk *w);

/** A table's file record size, or 0 for a kind that is no table. */
u32 portSetupRecordFileSize(u8 kind);

/** A table's record size and alignment in the image (the C record's). */
u32 portSetupRecordOutSize(u8 kind);
u32 portSetupKindAlign(u8 kind);

/** The intro at AT's bytes in the image, walked as the writer walks it. */
u32 portSetupIntroOutBytes(const struct SetupWalk *w, u32 at);

/** The prop list at AT (BYTES in the file)'s bytes in the image, or 0 after refusing. */
u32 portSetupPropsOutBytes(const struct SetupWalk *w, u32 at, u32 bytes);

/**
 * The new offset of the table of KIND that starts at file offset VALUE, in
 * *TO; refuses, naming WHAT at file offset WHERE, a value that is not the
 * start of such a table.
 */
int portSetupRefTable(const struct SetupWalk *w, u32 value, u8 kind, const char *what, u32 where, u32 *to);

/** The new offset of the s32 list word at VALUE; refuses one that is not a list word. */
int portSetupRefList(const struct SetupWalk *w, u32 value, const char *what, u32 where, u32 *to);

/**
 * The new offset of the byte at VALUE in the unowned bytes: its slot's new
 * offset plus its distance into the slot. Refuses a VALUE outside them, and
 * one whose terminator (a NUL for SB_NAME, an AI_EndList byte for SB_AILIST)
 * is not in the run of unowned bytes it starts in.
 */
int portSetupRefBytes(const struct SetupWalk *w, u32 value, enum SetupBytes what, const char *field, u32 where,
        u32 *to);

/**
 * Checks (W->out NULL) or writes the item ITEM at TO in W->out. Each reads the
 * file only; while checking each refuses what writing would find wrong, so
 * writing refuses nothing.
 */
int portSetupWriteHeader(const struct SetupWalk *w, const struct SetupItem *item, u32 to);
int portSetupWriteTable(const struct SetupWalk *w, const struct SetupItem *item, u32 to);
int portSetupWriteList(const struct SetupWalk *w, const struct SetupItem *item, u32 to);
int portSetupWriteIntro(const struct SetupWalk *w, const struct SetupItem *item, u32 to);
int portSetupWriteCredits(const struct SetupWalk *w, const struct SetupItem *item, u32 to);
int portSetupWriteProps(const struct SetupWalk *w, const struct SetupItem *item, u32 to);

#endif
