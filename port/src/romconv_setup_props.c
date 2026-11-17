/**
 * The setup file converter's prop records (port_romconv_stage.h; LP64-004
 * Task 15).
 *
 * The prop list is the setup's live objects: the game fills pointer members
 * (prop, model, a door's linked door, a list's next) in the records
 * themselves, so at 64 bits each record is written at the size of the record
 * the game reads that type through. kPropWide holds, per type, the file
 * words that are pointer-sized in that record; the rule (a pointer word at
 * the next multiple of the pointer size, pointer-sized and zero-extended,
 * every other word in 4 bytes, the record padded to its WIDEWORDS) gives
 * every member's place, and test_romconv_setup_props.c checks each place and
 * size against the compiler. Where the record the game reads is not the
 * record the file holds, the table follows the reader:
 *  - WATCH_MENU_OBJECTIVE_TEXT is linked through setup_objective_text.next and
 *    read as watchMenuObjectiveText (objective.c, objective_status.c): its
 *    word 3 is a pointer, not bondtypes.h's two-word record.
 *  - OBJECTIVE_ENTER_ROOM, _DEPOSIT_OBJECT_IN_ROOM and _PHOTOGRAPH are linked
 *    as criteria_roomentered, criteria_deposit and criteria_picture: the
 *    last word of each is a next pointer.
 *  - RENAME is linked as a textoverride (bondinv.c), whose word 8 is a next
 *    pointer and word 9 the object.
 *  - OBJECTIVE_START is read as an objective_entry, which has no pointer.
 *  - GLASS, SAFE and GAS_RELEASING are read as ObjectRecord, the size
 *    sizepropdef gives them at 32 bits.
 *  - LINK is read as a LinkRecord's two index unions only; the file record
 *    has no room for its next, which only SWITCH records use.
 * Compiled only at 64 bits and in the 32-bit oracle build.
 */
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "bondconstants.h"
#include "port_romconv_stage.h"
#include "romconv_setup_walk.h"
#include "../generated/setup_kinds.h"

/* WIDEWORDS is FILEWORDS at 32 bits, where every word keeps its place. */
#if IS_64_BIT
#define WIDE(file, wide64, n, ...) { file, wide64, n, { __VA_ARGS__ } }
#else
#define WIDE(file, wide64, n, ...) { file, file, n, { __VA_ARGS__ } }
#endif
/* ObjectRecord's prop, model, collision block and projectile. */
#define OBJ 4, 5, 26, 27

static const struct PortSetupWide kPropWide[SETUP_PROPDEF_TYPES] = {
    WIDE(1, 2, 0, 0),                                        /* NOTHING */
    WIDE(64, 74, 8, OBJ, 50, 51, 61, 62),                    /* DOOR: linkedDoor, unkcc, the sound states */
    WIDE(2, 2, 0, 0),                                        /* DOOR_SCALE */
    WIDE(32, 36, 4, OBJ),                                    /* PROP */
    WIDE(33, 38, 4, OBJ),                                    /* KEY */
    WIDE(32, 36, 4, OBJ),                                    /* ALARM */
    WIDE(59, 64, 4, OBJ),                                    /* CCTV */
    WIDE(33, 38, 4, OBJ),                                    /* MAGAZINE */
    WIDE(34, 40, 5, OBJ, 33),                                /* COLLECTABLE: dualweapon */
    WIDE(7, 8, 1, 6),                                        /* GUARD: Data */
    WIDE(64, 72, 6, OBJ, 32, 34),                            /* MONITOR: cmdlist, tconfig */
    WIDE(149, 166, 12, OBJ, 32, 34, 61, 63, 90, 92, 119, 121), /* MULTI_MONITOR: four monitors */
    WIDE(32, 36, 4, OBJ),                                    /* RACK */
    WIDE(54, 62, 7, OBJ, 49, 50, 51),                        /* AUTOGUN: the sound states, beam */
    WIDE(3, 6, 2, 1, 2),                                     /* LINK: the two index unions */
    WIDE(1, 2, 0, 0),                                        /* DEBRIS */
    WIDE(1, 2, 0, 0),                                        /* UNK16 */
    WIDE(32, 36, 4, OBJ),                                    /* HAT */
    WIDE(3, 4, 0, 0),                                        /* GUARD_ATTRIBUTE */
    WIDE(4, 8, 3, 1, 2, 3),                                  /* SWITCH: first, second, next */
    WIDE(45, 50, 4, OBJ),                                    /* AMMO */
    WIDE(34, 38, 4, OBJ),                                    /* ARMOUR */
    WIDE(4, 6, 2, 2, 3),                                     /* TAG: NextTag, TaggedObject */
    WIDE(4, 4, 0, 0),                                        /* OBJECTIVE_START */
    WIDE(1, 2, 0, 0),                                        /* OBJECTIVE_END */
    WIDE(2, 2, 0, 0),                                        /* OBJECTIVE_DESTROY_OBJECT */
    WIDE(2, 2, 0, 0),                                        /* OBJECTIVE_COMPLETE_CONDITION */
    WIDE(2, 2, 0, 0),                                        /* OBJECTIVE_FAIL_CONDITION */
    WIDE(2, 2, 0, 0),                                        /* OBJECTIVE_COLLECT_OBJECT */
    WIDE(2, 2, 0, 0),                                        /* OBJECTIVE_DEPOSIT_OBJECT */
    WIDE(4, 6, 1, 3),                                        /* OBJECTIVE_PHOTOGRAPH: next */
    WIDE(1, 2, 0, 0),                                        /* OBJECTIVE_NULL */
    WIDE(4, 6, 1, 3),                                        /* OBJECTIVE_ENTER_ROOM: next */
    WIDE(5, 6, 1, 4),                                        /* OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM: next */
    WIDE(1, 2, 0, 0),                                        /* OBJECTIVE_COPY_ITEM */
    WIDE(4, 6, 1, 3),                                        /* WATCH_MENU_OBJECTIVE_TEXT: next */
    WIDE(32, 36, 4, OBJ),                                    /* GAS_RELEASING */
    WIDE(10, 12, 2, 8, 9),                                   /* RENAME: next, obj */
    WIDE(4, 8, 3, 1, 2, 3),                                  /* LOCK_DOOR: door, lock, next */
    WIDE(44, 52, 7, OBJ, 32, 41, 43),                        /* VEHICHLE: ailist, path, Sound */
    WIDE(45, 52, 7, OBJ, 32, 43, 44),                        /* AIRCRAFT: ailist, path, Sound */
    WIDE(1, 2, 0, 0),                                        /* UNK41 */
    WIDE(32, 36, 4, OBJ),                                    /* GLASS */
    WIDE(32, 36, 4, OBJ),                                    /* SAFE */
    WIDE(5, 10, 4, 1, 2, 3, 4),                              /* SAFE_ITEM: item, safe, door, next */
    WIDE(56, 60, 4, OBJ),                                    /* TANK */
    WIDE(7, 8, 0, 0),                                        /* CAMERAPOS */
    WIDE(37, 42, 4, OBJ),                                    /* TINTED_GLASS */
    WIDE(1, 2, 0, 0),                                        /* END */
};

#define PROPS_POINTER ((u32) sizeof(void *))

/* A pointer word's 4 bytes go first in its slot: its value, zero-extended. */
_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "the prop converter writes little-endian slots");

const struct PortSetupWide *portSetupWide(u32 type)
{
    return type < SETUP_PROPDEF_TYPES ? &kPropWide[type] : NULL;
}

/* The record of TYPE at FROM written at OUT (NULL: only measured).
 * Returns the bytes its words take by the rule, which a correct table keeps
 * within WIDEWORDS. */
static u32 propRecord(const u8 *from, u32 type, u8 *out)
{
    const struct PortSetupWide *wide = &kPropWide[type];
    u32 place = 0, word, p = 0;

    for (word = 0; word < wide->filewords; word++)
    {
        const int pointer = p < wide->npointers && wide->pointerword[p] == word;
        /* propdef word 0 is always {u16 scale; u8 state; u8 type}, as romfile_setup.c swaps it */
        const int kind = word == 0 ? SETUP_KIND_KSK : kSetupPropdefKinds[type].kind[word];

        if (pointer)
        {
            place = (place + PROPS_POINTER - 1) & ~(PROPS_POINTER - 1);
            p++;
        }
        if (out != NULL)
        {
            portSetupKindWord(from + word * 4, kind, out + place); /* low 4 bytes: zero-extended on little-endian */
        }
        place += pointer ? PROPS_POINTER : 4;
    }
    return place;
}

/* The list's converted bytes, or 0: each record's type known, inside SIZE,
 * laid out within its WIDEWORDS, and nothing after the end record. */
static u32 propsMeasure(const u8 *props, u32 size)
{
    u64 out = 0;
    u32 at = 0;

    if (props == NULL)
    {
        return 0;
    }
    for (;;)
    {
        u32 type;

        if ((u64) at + 4 > size)
        {
            return 0;
        }
        type = props[at + 3];
        if (type >= SETUP_PROPDEF_TYPES || (u64) at + kPropWide[type].filewords * 4u > size
            || propRecord(props + at, type, NULL) > kPropWide[type].widewords * 4u)
        {
            return 0;
        }
        at += kPropWide[type].filewords * 4u;
        out += kPropWide[type].widewords * 4u;
        if (type == PROPDEF_END)
        {
            return at == size && out <= 0xFFFFFFF0u ? (u32) out : 0;
        }
    }
}

/* romconv_setup.c sizes the counts by bondconstants.h's PROPDEF_MAX. */
_Static_assert(SETUP_PROPDEF_TYPES == PROPDEF_MAX, "setup_kinds.h and PROPDEF_TYPE count the same prop types");

void portSetupPropsCount(const u8 *props, u32 size, u32 *counts)
{
    u32 at = 0;

    memset(counts, 0, SETUP_PROPDEF_TYPES * sizeof(counts[0]));
    if (propsMeasure(props, size) == 0)
    {
        return;
    }
    while (at < size)
    {
        const u32 type = props[at + 3];

        counts[type]++;
        at += kPropWide[type].filewords * 4u;
    }
}

u32 portSetupPropsSize(const u8 *props, u32 size)
{
    return propsMeasure(props, size);
}

s32 portSetupPropsConvert(const u8 *props, u32 size, u8 *out, u32 outsize)
{
    const u32 total = propsMeasure(props, size);
    u32 at = 0, to = 0;

    if (total == 0 || out == NULL || outsize < total)
    {
        return 0;
    }
    memset(out, 0, total);
    while (to < total)
    {
        const u32 type = props[at + 3];

        propRecord(props + at, type, out + to);
        at += kPropWide[type].filewords * 4u;
        to += kPropWide[type].widewords * 4u;
    }
    return 1;
}
