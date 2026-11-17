/**
 * The setup file converter's intro, credits table and prop list
 * (port_romconv_stage.h; LP64-004 Task 14). The intro is a run of records the
 * game walks by sizeof (bondview_r.c, bondview2.c): each record is written at
 * its struct's size, and where a camera or credits record, which hold
 * pointer-sized fields at 64 bits, would start off their alignment, one word
 * of type INTROTYPE_MAX goes first; every intro walk steps over a type it does
 * not know by 4 bytes. Scalars are swapped by port/generated/setup_kinds.h's
 * kinds, exactly as romfile_setup.c swaps them. The prop list's extent is
 * walked here by the same table; its records are portSetupPropsConvert's.
 * Compiled only at 64 bits and in the 32-bit oracle build.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "bondtypes.h"
#include "bondconstants.h"
#include "port_romconv.h"
#include "port_romconv_stage.h"
#include "port_romlayout_stage.h"
#include "romconv_setup_walk.h"
#include "../generated/setup_kinds.h"

_Static_assert(SETUP_INTRO_TYPES == INTROTYPE_END + 1, "the kinds table has a row for each intro type");
_Static_assert(SETUP_PROPDEF_TYPES == PROPDEF_END + 1, "the kinds table has a row for each prop type");
/* Every intro record but the camera and the credits record is its file
 * record at both widths, so its words are copied in order. */
_Static_assert(sizeof(SetupIntroSpawn) == sizeof(RomIntroSpawn) && sizeof(struct SetupIntroItem) == sizeof(RomIntroItem)
                   && sizeof(struct SetupIntroAmmo) == sizeof(RomIntroAmmo)
                   && sizeof(struct SetupIntroSwirl) == sizeof(RomIntroSwirl)
                   && sizeof(struct SetupIntroAnim) == sizeof(RomIntroAnim)
                   && sizeof(struct SetupIntroCuff) == sizeof(RomIntroCuff)
                   && sizeof(struct SetupIntroWatch) == sizeof(RomIntroWatch)
                   && sizeof(SetupIntroEmpty) == sizeof(RomIntroEnd),
               "the intro records other than the camera and credits keep their file layout");
/* The camera's first seven words keep their places. */
_Static_assert(offsetof(SetupIntroCamera, unk18) == 24, "the camera's scalars are its first seven words");

/* A word the intro walks read as a type they do not know (INTRO_TYPE is the
 * native word's top byte on PC), so they step over it by 4 bytes. */
#define INTRO_PAD_WORD ((u32) INTROTYPE_MAX << 24)

/* ==== byte order, as romfile_setup.c's kindApply ======================= */

void portSetupKindWord(const u8 *src, int kind, u8 *dst)
{
    u8 b[4];

    memcpy(b, src, 4);
    switch (kind)
    {
    case SETUP_KIND_K32: dst[0] = b[3]; dst[1] = b[2]; dst[2] = b[1]; dst[3] = b[0]; return;
    case SETUP_KIND_K16: dst[0] = b[1]; dst[1] = b[0]; dst[2] = b[3]; dst[3] = b[2]; return;
    case SETUP_KIND_KSK: dst[0] = b[1]; dst[1] = b[0]; dst[2] = b[2]; dst[3] = b[3]; return;
    case SETUP_KIND_KKS: dst[0] = b[0]; dst[1] = b[1]; dst[2] = b[3]; dst[3] = b[2]; return;
    default: memcpy(dst, b, 4); return; /* K8 */
    }
}

/* ==== claims =========================================================== */

static int claimCredits(struct SetupWalk *w, u32 table, u32 where)
{
    u32 at = table;

    for (;;)
    {
        if ((u64) at + sizeof(RomCreditsEntry) > w->size)
        {
            setupRefuse(w, "the credits record at 0x%X names a table at 0x%X with no terminator inside the %u-byte file",
                    where, table, w->size);
            return 0;
        }
        at += sizeof(RomCreditsEntry);
        if (setupBe16(w, at - 12 + offsetof(RomCreditsEntry, TextId1)) == 0
                && setupBe16(w, at - 12 + offsetof(RomCreditsEntry, TextId2)) == 0)
        {
            break;
        }
    }
    return portSetupClaim(w, table, (u64) at - table, SETUP_CREDITS, "credits table") != 0;
}

static int claimIntro(struct SetupWalk *w)
{
    const u32 start = w->header[SF_INTRO];
    u32 at = start;

    for (;;)
    {
        u32 type, bytes;

        if ((u64) at + 4 > w->size)
        {
            setupRefuse(w, "the intro at 0x%X has no end record inside the %u-byte file", start, w->size);
            return 0;
        }
        type = w->file[at + 3];
        if (type >= SETUP_INTRO_TYPES)
        {
            setupRefuse(w, "the intro record at 0x%X has type %u (the last is %d)", at, type, SETUP_INTRO_TYPES - 1);
            return 0;
        }
        bytes = kSetupIntroKinds[type].words * 4u;
        if ((u64) at + bytes > w->size)
        {
            setupRefuse(w, "the intro record at 0x%X (type %u) runs past the %u-byte file", at, type, w->size);
            return 0;
        }
        if (type == INTROTYPE_CREDITS && !claimCredits(w, setupBe32(w, at + offsetof(RomIntroCredits, table)), at))
        {
            return 0;
        }
        at += bytes;
        if (type == INTROTYPE_END)
        {
            break;
        }
    }
    return portSetupClaim(w, start, (u64) at - start, SETUP_INTRO, "intro") == 1;
}

static int claimProps(struct SetupWalk *w)
{
    const u32 start = w->header[SF_PROPS];
    u32 at = start;

    for (;;)
    {
        u32 type, bytes;

        if ((u64) at + 4 > w->size)
        {
            setupRefuse(w, "the prop list at 0x%X has no end record inside the %u-byte file", start, w->size);
            return 0;
        }
        type = w->file[at + 3];
        if (type >= SETUP_PROPDEF_TYPES)
        {
            setupRefuse(w, "the prop record at 0x%X has type %u (the last is %d)", at, type, SETUP_PROPDEF_TYPES - 1);
            return 0;
        }
        bytes = kSetupPropdefKinds[type].words * 4u;
        if ((u64) at + bytes > w->size)
        {
            setupRefuse(w, "the prop record at 0x%X (type %u) runs past the %u-byte file", at, type, w->size);
            return 0;
        }
        at += bytes;
        if (type == PROPDEF_END)
        {
            break;
        }
    }
    return portSetupClaim(w, start, (u64) at - start, SETUP_PROPS, "prop list") == 1;
}

int portSetupClaimIntroAndProps(struct SetupWalk *w)
{
    return claimIntro(w) && claimProps(w);
}

/* ==== sizes ============================================================ */

static u32 introSize(u32 type)
{
    switch (type)
    {
    case INTROTYPE_CAMERA: return sizeof(SetupIntroCamera);
    case INTROTYPE_CREDITS: return sizeof(struct SetupIntroCredits);
    default: return kSetupIntroKinds[type].words * 4u; /* the struct's size, asserted above */
    }
}

static u32 introAlign(u32 type)
{
    switch (type)
    {
    case INTROTYPE_CAMERA: return _Alignof(SetupIntroCamera) > 4 ? _Alignof(SetupIntroCamera) : 4;
    case INTROTYPE_CREDITS: return _Alignof(struct SetupIntroCredits) > 4 ? _Alignof(struct SetupIntroCredits) : 4;
    default: return 4;
    }
}

/* Walks the intro at AT, already claimed (so every record is inside the file
 * and of a known type), calling VISIT for each record with its place in the
 * image after any pad words. Returns the image bytes, or 0 when VISIT fails. */
typedef int (*IntroVisit)(const struct SetupWalk *w, u32 from, u32 type, u32 place, const struct SetupItem *item,
        u32 to);

static u32 introWalk(const struct SetupWalk *w, u32 at, IntroVisit visit, const struct SetupItem *item, u32 to)
{
    const u32 pad = INTRO_PAD_WORD;
    u32 place = 0, aligned, p, type;

    do
    {
        type = w->file[at + 3];
        for (aligned = place; aligned % introAlign(type) != 0; aligned += 4)
        {
        }
        if (visit != NULL)
        {
            /* A backstop: the layout measured these bytes by this same walk. */
            if ((u64) aligned + introSize(type) > item->outBytes)
            {
                setupRefuse(w, "the intro record at 0x%X outgrows the intro's %u bytes measured", at, item->outBytes);
                return 0;
            }
            for (p = place; w->out != NULL && p < aligned; p += 4)
            {
                memcpy(w->out + to + p, &pad, 4);
            }
            if (!visit(w, at, type, aligned, item, to))
            {
                return 0;
            }
        }
        place = aligned + introSize(type);
        at += kSetupIntroKinds[type].words * 4u;
    } while (type != INTROTYPE_END);
    return place;
}

u32 portSetupIntroOutBytes(const struct SetupWalk *w, u32 at)
{
    return introWalk(w, at, NULL, NULL, 0);
}

u32 portSetupPropsOutBytes(const struct SetupWalk *w, u32 at, u32 bytes)
{
    const u32 out = portSetupPropsSize(w->file + at, bytes);

    if (out == 0)
    {
        setupRefuse(w, "the prop list at 0x%X (%u bytes) holds records the prop converter does not convert", at, bytes);
    }
    return out;
}

/* ==== writers ========================================================== */

static int writeIntroRecord(const struct SetupWalk *w, u32 from, u32 type, u32 place, const struct SetupItem *item,
        u32 to)
{
    const u8 *kinds = kSetupIntroKinds[type].kind;
    u8 record[sizeof(SetupIntroCamera) > sizeof(struct SetupIntroCredits) ? sizeof(SetupIntroCamera)
                                                                            : sizeof(struct SetupIntroCredits)];
    u32 k, value;

    (void) item;
    memset(record, 0, sizeof(record));
    if (type == INTROTYPE_CAMERA)
    {
        SetupIntroCamera *camera = (SetupIntroCamera *) (void *) record;

        for (k = 0; k < 7; k++)
        {
            portSetupKindWord(w->file + from + k * 4, kinds[k], record + k * 4);
        }
        /* The text IDs in the low 32 bits of their unions, as the game reads them. */
        portSetupKindWord(w->file + from + offsetof(RomIntroCamera, text1), kinds[7], (u8 *) &camera->lang1c);
        portSetupKindWord(w->file + from + offsetof(RomIntroCamera, text2), kinds[8], (u8 *) &camera->lang20);
        portSetupKindWord(w->file + from + offsetof(RomIntroCamera, prev), kinds[9], (u8 *) &value);
        camera->prev = (SetupIntroCamera *) (uintptr_t) value;
    }
    else if (type == INTROTYPE_CREDITS)
    {
        struct SetupIntroCredits *credits = (struct SetupIntroCredits *) (void *) record;
        u32 table;

        if (!portSetupRefTable(w, setupBe32(w, from + offsetof(RomIntroCredits, table)), SETUP_CREDITS,
                    "credits record's table", from + offsetof(RomIntroCredits, table), &table))
        {
            return 0;
        }
        portSetupKindWord(w->file + from, kinds[0], (u8 *) &credits->type);
#if IS_64_BIT
        credits->unk04 = table; /* bondview_r.c adds the setup's base */
#else
        /* At 32 bits the game reads the table's address, as the in-place pass writes it. */
        if (w->out != NULL)
        {
            const u8 *address = w->out + table;
            _Static_assert(sizeof(credits->unk04) == sizeof(address), "the credits word holds an address at 32 bits");
            memcpy(&credits->unk04, &address, sizeof(address));
        }
#endif
    }
    else
    {
        for (k = 0; k < kSetupIntroKinds[type].words; k++)
        {
            portSetupKindWord(w->file + from + k * 4, kinds[k], record + k * 4);
        }
    }
    if (w->out != NULL)
    {
        memcpy(w->out + to + place, record, introSize(type));
    }
    return 1;
}

int portSetupWriteIntro(const struct SetupWalk *w, const struct SetupItem *item, u32 to)
{
    return introWalk(w, item->at, writeIntroRecord, item, to) != 0;
}

int portSetupWriteCredits(const struct SetupWalk *w, const struct SetupItem *item, u32 to)
{
    const u32 count = item->bytes / sizeof(RomCreditsEntry);
    CreditsEntry entry;
    u32 i, from;

    for (i = 0; w->out != NULL && i < count; i++)
    {
        from = item->at + i * (u32) sizeof(RomCreditsEntry);
        entry.TextId1 = setupBe16(w, from + offsetof(RomCreditsEntry, TextId1));
        entry.TextId2 = setupBe16(w, from + offsetof(RomCreditsEntry, TextId2));
        entry.Position1 = (s16) setupBe16(w, from + offsetof(RomCreditsEntry, Position1));
        entry.Alignment1 = setupBe16(w, from + offsetof(RomCreditsEntry, Alignment1));
        entry.Position2 = (s16) setupBe16(w, from + offsetof(RomCreditsEntry, Position2));
        entry.Alignment2 = setupBe16(w, from + offsetof(RomCreditsEntry, Alignment2));
        memcpy(w->out + to + i * sizeof(CreditsEntry), &entry, sizeof(entry));
    }
    return 1;
}

int portSetupWriteProps(const struct SetupWalk *w, const struct SetupItem *item, u32 to)
{
    /* The checking pass converts into scratch memory, so that the write pass
     * cannot refuse a prop list the checks accepted. */
    u8 *scratch = w->out == NULL ? (u8 *) malloc(item->outBytes) : NULL;
    s32 ok;

    if (w->out == NULL && scratch == NULL)
    {
        setupRefuse(w, "no memory to check the prop list at 0x%X (%u bytes)", item->at, item->outBytes);
        return 0;
    }
    ok = portSetupPropsConvert(w->file + item->at, item->bytes, w->out != NULL ? w->out + to : scratch, item->outBytes);
    free(scratch);
    if (!ok)
    {
        setupRefuse(w, "the prop converter refused the prop list at 0x%X after measuring it", item->at);
        return 0;
    }
    return 1;
}
