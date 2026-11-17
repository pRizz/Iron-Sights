/**
 * The setup file converter's claims of the header, the tables and the s32
 * lists (port_romconv_stage.h; LP64-004 Task 14): marks which item owns each
 * 4 bytes of the file, walking every table to its terminator record and every
 * list to its negative word, each walk bounded by the file. Items that share
 * a byte are refused; s32 lists may share words with one another, as the
 * words are converted alike and keep their size. Reads only the file.
 * Compiled only at 64 bits and in the 32-bit oracle build.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "port_romconv.h"
#include "port_romlayout_stage.h"
#include "romconv_setup_walk.h"

/* ==== which item owns each 4 bytes ===================================== */

/* Grow-only, so that converting a stock file (at most 103,424 bytes; this
 * covers 262,144, portConvScratchReserve's size) allocates nothing. */
static u8 s_KindsStatic[65536];
static u8 *s_Kinds = s_KindsStatic;
static u32 s_KindsCapacity = sizeof(s_KindsStatic);

static const char *const kKindNames[SETUP_KIND_COUNT] = {"unowned bytes", "header", "waypoint table",
    "waygroup table", "path table", "AI list table", "pad table", "bound pad table", "pad name table",
    "bound pad name table", "s32 list", "intro", "credits table", "prop list"};

const char *portSetupKindName(u8 kind)
{
    return SETUP_KIND(kind) < SETUP_KIND_COUNT ? kKindNames[SETUP_KIND(kind)] : "item";
}

int portSetupKindsInit(struct SetupWalk *w)
{
    u32 slots = w->size / 4 + (w->size % 4 != 0);
    u8 *grown;

    if (slots > s_KindsCapacity)
    {
        grown = (u8 *) malloc(slots);
        if (grown == NULL)
        {
            setupRefuse(w, "no room to check the %u-byte file", w->size);
            return 0;
        }
        if (s_Kinds != s_KindsStatic)
        {
            free(s_Kinds);
        }
        s_Kinds = grown;
        s_KindsCapacity = slots;
    }
    memset(s_Kinds, 0, slots);
    w->kinds = s_Kinds;
    w->slots = slots;
    return 1;
}

int portSetupClaim(struct SetupWalk *w, u32 at, u64 bytes, u8 kind, const char *what)
{
    u32 first = at / 4, end, s;

    if (at % 4 != 0)
    {
        setupRefuse(w, "the %s at 0x%X is not at a multiple of 4", what, at);
        return 0;
    }
    if (bytes == 0 || bytes % 4 != 0 || (u64) at + bytes > w->size)
    {
        setupRefuse(w, "the %s at 0x%X (0x%llX bytes) runs past the %u-byte file", what, at,
                (unsigned long long) bytes, w->size);
        return 0;
    }
    end = (u32) (((u64) at + bytes) / 4);
    if (w->kinds[first] == kind)
    {
        for (s = first + 1; s < end && w->kinds[s] == (kind | SETUP_INSIDE); s++)
        {
        }
        if (s == end && (end == w->slots || w->kinds[end] != (kind | SETUP_INSIDE)))
        {
            return 2;
        }
        setupRefuse(w, "the %s at 0x%X is reached again with another length", what, at);
        return 0;
    }
    for (s = first; s < end; s++)
    {
        if (w->kinds[s] != SETUP_RAW)
        {
            setupRefuse(w, "the %s at 0x%X (0x%llX bytes) overlaps the %s at 0x%X", what, at,
                    (unsigned long long) bytes, portSetupKindName(w->kinds[s]), s * 4);
            return 0;
        }
    }
    w->kinds[first] = kind;
    for (s = first + 1; s < end; s++)
    {
        w->kinds[s] = (u8) (kind | SETUP_INSIDE);
    }
    return 1;
}

/* ==== record sizes ===================================================== */

#define MAX4(a) ((u32) (a) > 4u ? (u32) (a) : 4u)

u32 portSetupRecordFileSize(u8 kind)
{
    switch (SETUP_KIND(kind))
    {
    case SETUP_HEADER: return sizeof(RomSetupHeader);
    case SETUP_WAYPOINTS: return sizeof(RomWaypoint);
    case SETUP_WAYGROUPS: return sizeof(RomWaygroup);
    case SETUP_PATHS: return sizeof(RomPath);
    case SETUP_AILISTS: return sizeof(RomAiList);
    case SETUP_PADS: return sizeof(RomPad);
    case SETUP_BOUNDPADS: return sizeof(RomBoundPad);
    case SETUP_PADNAMES:
    case SETUP_BOUNDPADNAMES: return 4;
    case SETUP_LIST: return 4;
    case SETUP_CREDITS: return sizeof(RomCreditsEntry);
    default: return 0;
    }
}

u32 portSetupRecordOutSize(u8 kind)
{
    switch (SETUP_KIND(kind))
    {
    case SETUP_HEADER: return sizeof(struct stagesetup);
    case SETUP_WAYPOINTS: return sizeof(waypoint);
    case SETUP_WAYGROUPS: return sizeof(waygroup);
    case SETUP_PATHS: return sizeof(PathRecord);
    case SETUP_AILISTS: return sizeof(AIListRecord);
    case SETUP_PADS: return sizeof(PadRecord);
    case SETUP_BOUNDPADS: return sizeof(BoundPadRecord);
    case SETUP_PADNAMES:
    case SETUP_BOUNDPADNAMES: return sizeof(pname);
    case SETUP_LIST: return sizeof(s32);
    case SETUP_CREDITS: return sizeof(CreditsEntry);
    default: return 0;
    }
}

/* At least 4, so every place stays on the file's 4-byte grid; at 32 bits
 * exactly 4 for every kind, so no item moves from its file place. */
u32 portSetupKindAlign(u8 kind)
{
    switch (SETUP_KIND(kind))
    {
    case SETUP_HEADER: return MAX4(_Alignof(struct stagesetup));
    case SETUP_WAYPOINTS: return MAX4(_Alignof(waypoint));
    case SETUP_WAYGROUPS: return MAX4(_Alignof(waygroup));
    case SETUP_PATHS: return MAX4(_Alignof(PathRecord));
    case SETUP_AILISTS: return MAX4(_Alignof(AIListRecord));
    case SETUP_PADS: return MAX4(_Alignof(PadRecord));
    case SETUP_BOUNDPADS: return MAX4(_Alignof(BoundPadRecord));
    case SETUP_PADNAMES:
    case SETUP_BOUNDPADNAMES: return MAX4(_Alignof(pname));
    case SETUP_CREDITS: return MAX4(_Alignof(CreditsEntry));
    case SETUP_INTRO: return MAX4(_Alignof(SetupIntroCamera)) > MAX4(_Alignof(struct SetupIntroCredits))
            ? MAX4(_Alignof(SetupIntroCamera)) : MAX4(_Alignof(struct SetupIntroCredits));
    case SETUP_PROPS: return MAX4(_Alignof(ObjectRecord));
    default: return 4;
    }
}

/* ==== the s32 lists ==================================================== */

/* Claims the list at VALUE, named by WHAT at file offset WHERE: its words up
 * to and including the first negative one, each inside the file and unowned
 * or already a list word. */
static int claimList(struct SetupWalk *w, u32 value, const char *what, u32 where)
{
    u32 at = value;

    if (value % 4 != 0)
    {
        setupRefuse(w, "the %s at 0x%X names 0x%X, not a multiple of 4", what, where, value);
        return 0;
    }
    for (;;)
    {
        u8 kind;

        if ((u64) at + 4 > w->size)
        {
            setupRefuse(w, "the %s at 0x%X (list at 0x%X) has no negative word inside the %u-byte file", what, where,
                    value, w->size);
            return 0;
        }
        kind = w->kinds[at / 4];
        if (kind != SETUP_RAW && kind != SETUP_LIST)
        {
            setupRefuse(w, "the %s at 0x%X (list at 0x%X) runs into the %s at 0x%X", what, where, value,
                    portSetupKindName(kind), at);
            return 0;
        }
        w->kinds[at / 4] = SETUP_LIST;
        if ((s32) setupBe32(w, at) < 0)
        {
            return 1;
        }
        at += 4;
    }
}

/* ==== the tables ======================================================= */

static const struct
{
    u8 field;
    u8 kind;
    const char *what;
} kTables[] = {
    {SF_PADS, SETUP_PADS, "pad"},
    {SF_BOUNDPADS, SETUP_BOUNDPADS, "bound pad"},
    {SF_WAYPOINTS, SETUP_WAYPOINTS, "waypoint"},
    {SF_WAYGROUPS, SETUP_WAYGROUPS, "waygroup"},
    {SF_PATHS, SETUP_PATHS, "path"},
    {SF_AILISTS, SETUP_AILISTS, "AI list"},
    {SF_PADNAMES, SETUP_PADNAMES, "pad name"},
    {SF_BOUNDPADNAMES, SETUP_BOUNDPADNAMES, "bound pad name"},
};

/* Whether the record of KIND at AT ends its table, as the game's loops and
 * romfile_setup.c's walk decide. */
static int isTerminator(const struct SetupWalk *w, u8 kind, u32 at)
{
    switch (kind)
    {
    case SETUP_WAYPOINTS: return (s32) setupBe32(w, at + offsetof(RomWaypoint, padID)) < 0;
    case SETUP_PADS:
    case SETUP_BOUNDPADS: return setupBe32(w, at + offsetof(RomPad, plink)) == 0;
    default: return setupBe32(w, at) == 0; /* the first word: neighbours, waypoints, ailist, a name */
    }
}

/* Claims the lists a non-terminating record of KIND at AT names. */
static int claimRecordLists(struct SetupWalk *w, u8 kind, u32 at)
{
    switch (kind)
    {
    case SETUP_WAYPOINTS:
        return claimList(w, setupBe32(w, at + offsetof(RomWaypoint, neighbours)), "waypoint's neighbours",
                at + offsetof(RomWaypoint, neighbours));
    case SETUP_WAYGROUPS:
        return claimList(w, setupBe32(w, at + offsetof(RomWaygroup, neighbours)), "waygroup's neighbours",
                       at + offsetof(RomWaygroup, neighbours))
            && claimList(w, setupBe32(w, at + offsetof(RomWaygroup, waypoints)), "waygroup's waypoints",
                at + offsetof(RomWaygroup, waypoints));
    case SETUP_PATHS:
        return claimList(w, setupBe32(w, at + offsetof(RomPath, waypoints)), "path's waypoints",
                at + offsetof(RomPath, waypoints));
    default:
        return 1;
    }
}

static int claimTable(struct SetupWalk *w, u32 start, u8 kind, const char *what)
{
    const u32 record = portSetupRecordFileSize(kind);
    u32 at = start, count = 0, i;

    for (;;)
    {
        int last;

        if ((u64) at + record > w->size)
        {
            setupRefuse(w, "the %s table at 0x%X has no terminator inside the %u-byte file", what, start, w->size);
            return 0;
        }
        last = isTerminator(w, kind, at);
        count++;
        at += record;
        if (last)
        {
            break;
        }
    }
    if (portSetupClaim(w, start, (u64) count * record, kind, portSetupKindName(kind)) != 1)
    {
        return 0;
    }
    for (i = 0; i + 1 < count; i++)
    {
        if (!claimRecordLists(w, kind, start + i * record))
        {
            return 0;
        }
    }
    return 1;
}

int portSetupClaimTables(struct SetupWalk *w)
{
    u32 i;

    if (w->size < sizeof(RomSetupHeader))
    {
        setupRefuse(w, "%u bytes cannot hold the %u-byte header", w->size, (u32) sizeof(RomSetupHeader));
        return 0;
    }
    for (i = 0; i < SF_COUNT; i++)
    {
        const u32 value = setupBe32(w, i * 4);
        const int optional = i == SF_PADNAMES || i == SF_BOUNDPADNAMES;

        w->header[i] = value;
        if (value == 0 && optional)
        {
            continue;
        }
        if (value == 0 || value % 4 != 0 || value >= w->size)
        {
            setupRefuse(w, "header offset %u is 0x%X: %s", i, value,
                    value == 0 ? "0 names the header itself" : value % 4 != 0 ? "not a multiple of 4"
                                                                              : "outside the file");
            return 0;
        }
    }
    if (portSetupClaim(w, 0, sizeof(RomSetupHeader), SETUP_HEADER, "header") != 1)
    {
        return 0;
    }
    for (i = 0; i < sizeof(kTables) / sizeof(kTables[0]); i++)
    {
        const u32 start = w->header[kTables[i].field];

        if (start != 0 && !claimTable(w, start, kTables[i].kind, kTables[i].what))
        {
            return 0;
        }
    }
    return 1;
}
