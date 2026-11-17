/**
 * The animation table converter (port_romconv.h; LP64-004 Task 3).
 *
 * The animation_data segment holds 0x14-byte file headers at the offsets the
 * PTR_ANIM_* constants name, and 83 sites form a header's address from such a
 * constant, so a header cannot move. On the N64 and at 32 bits the game
 * relocates each header in place (expand_ani_table_entries). At 64 bits its
 * three address fields are 8 bytes, so the run-time ModelAnimation, packed
 * and 4-byte aligned (bondtypes.h), takes 0x20 bytes: it is written over the
 * file header in the permanent copy, and its descriptor and stream pointers
 * lead into the staging image, which keeps the file layout where the
 * permanent copy's 0xC bytes after each header are overwritten. In the stock
 * ROM the 173 headers are at least 0x30 apart and the last ends 0x20 before
 * the segment's end.
 *
 * Compiled only at 64 bits and in the 32-bit oracle build. At 32 bits the
 * converter writes the 0x14 bytes the in-place relocation writes, with its
 * pointers into STAGED rather than into the permanent copy; the oracle
 * compares them as offsets.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include <bondtypes.h>
#include "port_romconv.h"
#include "port_romlayout.h"
#include "port_lp64_oracle.h"

/* The room a run-time header takes at 64 bits, and the least distance between
 * two headers, at both widths so that the oracle checks the 64-bit rule. */
#define ANIM_HEADER_ROOM 0x20u
#define ANIM_FILE "animation_data"

#if IS_64_BIT
_Static_assert(offsetof(ModelAnimation, unk14) == ANIM_HEADER_ROOM, "the read fields end at 0x20");
#else
_Static_assert(offsetof(ModelAnimation, unk14) == sizeof(RomAnimHeader), "the read fields are the file header");
#endif

/* The table's entries in order, as (table, index) of each that names a
 * header; a 0 ends a table and a 1 is empty. */
typedef struct AnimEntryCursor
{
    intptr_t *const *tables;
    int ntables;
    int table;
    int index;
} AnimEntryCursor;

static void animCursorStart(AnimEntryCursor *c, intptr_t *const *tables, int ntables)
{
    c->tables = tables;
    c->ntables = ntables;
    c->table = 0;
    c->index = -1;
}

/* Advances to the next entry that names a header; returns 0 past the last. */
static int animCursorNext(AnimEntryCursor *c)
{
    while (c->table < c->ntables)
    {
        intptr_t entry;

        c->index++;
        entry = c->tables[c->table][c->index];
        if (entry == 0)
        {
            c->table++;
            c->index = -1;
            continue;
        }
        if (entry != 1)
        {
            return 1;
        }
    }
    return 0;
}

static uintptr_t animCursorOffset(const AnimEntryCursor *c)
{
    return (uintptr_t) c->tables[c->table][c->index];
}

/* Whether an entry before C's names the header at OFFSET. */
static int animListedBefore(const AnimEntryCursor *c, uintptr_t offset)
{
    AnimEntryCursor earlier;

    animCursorStart(&earlier, c->tables, c->ntables);
    while (animCursorNext(&earlier) && (earlier.table != c->table || earlier.index != c->index))
    {
        if (animCursorOffset(&earlier) == offset)
        {
            return 1;
        }
    }
    return 0;
}

/* Refuses, naming it, a header that does not fit or that overlaps one listed
 * earlier. Reads offsets only. */
static int animHeadersFit(intptr_t *const *tables, int ntables, u32 tablesize, u32 stagedsize)
{
    AnimEntryCursor c;

    animCursorStart(&c, tables, ntables);
    while (animCursorNext(&c))
    {
        uintptr_t offset = animCursorOffset(&c);
        AnimEntryCursor earlier;

        if (offset > tablesize || tablesize - offset < ANIM_HEADER_ROOM)
        {
            portConvRefuse(PORT_LP64_ANIMATIONS, ANIM_FILE, "header at 0x%lX needs 0x%X bytes of a 0x%X-byte table",
                    (unsigned long) offset, ANIM_HEADER_ROOM, tablesize);
            return 0;
        }
        if (offset > stagedsize || stagedsize - offset < sizeof(RomAnimHeader))
        {
            portConvRefuse(PORT_LP64_ANIMATIONS, ANIM_FILE, "header at 0x%lX is outside the 0x%X-byte staged image",
                    (unsigned long) offset, stagedsize);
            return 0;
        }
        animCursorStart(&earlier, tables, ntables);
        while (animCursorNext(&earlier) && (earlier.table != c.table || earlier.index != c.index))
        {
            uintptr_t other = animCursorOffset(&earlier);
            uintptr_t apart = other > offset ? other - offset : offset - other;

            if (other != offset && apart < ANIM_HEADER_ROOM)
            {
                portConvRefuse(PORT_LP64_ANIMATIONS, ANIM_FILE, "headers at 0x%lX and 0x%lX are 0x%lX apart, under 0x%X",
                        (unsigned long) other, (unsigned long) offset, (unsigned long) apart, ANIM_HEADER_ROOM);
                return 0;
            }
        }
    }
    return 1;
}

static void animWriteHeader(u8 *table, const u8 *staged, const u8 *entries, uintptr_t offset)
{
    RomAnimHeader file;
    ModelAnimation *anim = (ModelAnimation *) (table + offset);

    memcpy(&file, staged + offset, sizeof(file));
    anim->address = (intptr_t) (entries + (uintptr_t) file.entries_offset);
    anim->unk04 = file.frames;
    anim->unk06 = file.unk06;
    anim->unk07 = file.unk07;
    anim->bitDescriptors = (ModelAnimBitField *) (staged + (uintptr_t) file.descriptors_offset);
    anim->unk0C = file.unk0c;
    anim->unk0E = file.frame_bits;
    anim->bitStream = (u8 *) (staged + (uintptr_t) file.stream_offset);
}

int portAnimHeadersConvert(u8 *table, u32 tablesize, const u8 *staged, u32 stagedsize, const u8 *entries,
        intptr_t *const *tables, int ntables)
{
    AnimEntryCursor c;

    if (!animHeadersFit(tables, ntables, tablesize, stagedsize))
    {
        return 0;
    }
    animCursorStart(&c, tables, ntables);
    while (animCursorNext(&c))
    {
        uintptr_t offset = animCursorOffset(&c);

        if (!animListedBefore(&c, offset))
        {
            animWriteHeader(table, staged, entries, offset);
        }
    }
    /* Last, so that the passes above read every entry as an offset. */
    animCursorStart(&c, tables, ntables);
    while (animCursorNext(&c))
    {
        c.tables[c.table][c.index] = (intptr_t) (table + animCursorOffset(&c));
    }
    return 1;
}

#ifdef GE_LP64_ORACLE

#define ANIM_ORACLE_TABLES 4
#define ANIM_ORACLE_ENTRIES 256

/* The 0x14 bytes of the header at HEADER with its two pointers into BASE
 * written as offsets from BASE; the address field is kept. */
static void animOracleRecord(u8 *record, const u8 *header, const u8 *base)
{
    const ModelAnimation *anim = (const ModelAnimation *) header;
    u32 descriptors = (u32) ((const u8 *) anim->bitDescriptors - base);
    u32 stream = (u32) ((const u8 *) anim->bitStream - base);

    memcpy(record, header, sizeof(RomAnimHeader));
    memcpy(record + offsetof(ModelAnimation, bitDescriptors), &descriptors, sizeof(descriptors));
    memcpy(record + offsetof(ModelAnimation, bitStream), &stream, sizeof(stream));
}

/* Copies the relocated TABLES back to offsets from INPLACE into LISTS. */
static int animOracleCopyTables(intptr_t lists[][ANIM_ORACLE_ENTRIES], intptr_t *const *tables, int ntables,
        const u8 *inplace)
{
    int t, i;

    if (ntables > ANIM_ORACLE_TABLES)
    {
        return 0;
    }
    for (t = 0; t < ntables; t++)
    {
        for (i = 0; tables[t][i] != 0; i++)
        {
            if (i + 1 >= ANIM_ORACLE_ENTRIES)
            {
                return 0;
            }
            lists[t][i] = tables[t][i] == 1 ? 1 : (intptr_t) ((const u8 *) tables[t][i] - inplace);
        }
        lists[t][i] = 0;
    }
    return 1;
}

static void animOracleCompare(const u8 *inplace, const u8 *staged, const u8 *copy, intptr_t *const *tables,
        intptr_t lists[][ANIM_ORACLE_ENTRIES], int ntables)
{
    int t, i;

    for (t = 0; t < ntables; t++)
    {
        for (i = 0; tables[t][i] != 0; i++)
        {
            char name[48];
            uintptr_t offset;
            u8 expected[sizeof(RomAnimHeader)], converted[sizeof(RomAnimHeader)];
            int listedBefore = 0, u, j;

            if (tables[t][i] == 1)
            {
                continue;
            }
            offset = (uintptr_t) ((const u8 *) tables[t][i] - inplace);
            snprintf(name, sizeof(name), "header 0x%04lX", (unsigned long) offset);
            if ((uintptr_t) ((const u8 *) lists[t][i] - copy) != offset)
            {
                portLp64OracleResult(PORT_LP64_ANIMATIONS, name, 0, "table entry");
            }
            for (u = 0; u <= t && !listedBefore; u++)
            {
                for (j = 0; tables[u][j] != 0 && (u < t || j < i); j++)
                {
                    listedBefore |= tables[u][j] == tables[t][i];
                }
            }
            if (listedBefore)
            {
                continue;
            }
            animOracleRecord(expected, inplace + offset, inplace);
            animOracleRecord(converted, copy + offset, staged);
            portLp64OracleBytes(PORT_LP64_ANIMATIONS, name, expected, converted, sizeof(expected));
        }
    }
}

void portAnimHeadersOracle(const u8 *inplace, u32 tablesize, const u8 *staged, const u8 *entries,
        intptr_t *const *tables, int ntables)
{
    static intptr_t lists[ANIM_ORACLE_TABLES][ANIM_ORACLE_ENTRIES];
    intptr_t *listp[ANIM_ORACLE_TABLES];
    u8 *copy = (u8 *) malloc(tablesize);
    int t;

    if (copy == NULL || !animOracleCopyTables(lists, tables, ntables, inplace))
    {
        portLp64OracleResult(PORT_LP64_ANIMATIONS, ANIM_FILE, 0, "the oracle could not copy the table");
        free(copy);
        return;
    }
    for (t = 0; t < ntables; t++)
    {
        listp[t] = lists[t];
    }
    memcpy(copy, staged, tablesize);
    if (!portAnimHeadersConvert(copy, tablesize, staged, tablesize, entries, listp, ntables))
    {
        portLp64OracleResult(PORT_LP64_ANIMATIONS, ANIM_FILE, 0, "the converter refused the table");
    }
    else
    {
        animOracleCompare(inplace, staged, copy, tables, lists, ntables);
    }
    free(copy);
}

#endif
