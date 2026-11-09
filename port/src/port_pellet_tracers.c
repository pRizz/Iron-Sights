/* Shotgun Tracers (FORK-063): one tracer per shotgun pellet. chrprop.c
 * records each pellet's hit and gunfire.c starts the beams: from the drawn
 * muzzle for the shooter's own view, and in multiplayer also from their
 * third-person gun for the other players' views, as gunCreateBeamForHand
 * does for a bullet. A guard's shotgun fires one shot (chraction.c), which
 * gets one beam. chrprop.c advances them all with the native tracers and
 * gunfire.c / propobj.c draw them beside the native ones. The beams use the
 * native tracer draw, so Tracer Strength styles them, but their own setup,
 * timer and random generator, so the game RNG is never drawn. */
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/player.h"
#include "port_tracers.h"

extern s32 get_cur_playernum(void);
extern s32 getPlayerCount(void);
extern f32 g_GlobalTimerDelta;
extern s32 g_GlobalTimer;
Gfx *sub_GAME_7F061E18(Gfx *gdl, BeamRecord *flash, s32 arg2);

typedef struct PelletSet {
    coord3d hits[NUMBER_SHOTGUN_BULLETS];
    s32 hitCount;
    BeamRecord beams[NUMBER_SHOTGUN_BULLETS];
    BeamRecord others[NUMBER_SHOTGUN_BULLETS]; /* from the third-person gun, for other viewers */
    u32 rng;
    s32 ready;
} PelletSet;

static PelletSet pelletSets[4][2];

static PelletSet *pelletSet(s32 player, s32 hand)
{
    PelletSet *set;
    s32 i;

    if (player < 0 || player > 3) player = 0;
    set = &pelletSets[player][hand & 1];
    if (!set->ready)
    {   /* A zeroed beam reads as live; start them all spent. */
        for (i = 0; i < NUMBER_SHOTGUN_BULLETS; i++) set->beams[i].unk00 = set->others[i].unk00 = -1;
        set->ready = TRUE;
    }
    return set;
}

/* xorshift32 in [0, 1). */
static f32 pelletRandom(PelletSet *set)
{
    u32 x = set->rng ? set->rng : 0x6C8E9CF5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    set->rng = x;
    return (f32) (x >> 8) * (1.0f / 16777216.0f);
}

void portPelletTracerRecord(s32 hand, s32 pellet)
{
    PelletSet *set = pelletSet(get_cur_playernum(), hand);

    if (!portShotgunTracersEnabled() || pellet < 0 || pellet >= NUMBER_SHOTGUN_BULLETS) return;
    /* gunSetTracerTarget left this pellet's hit (or its far point) here. */
    set->hits[pellet] = g_CurrentPlayer->hands[hand & 1].item_related;
    set->hitCount = pellet + 1;
}

void portPelletTracersCreate(s32 hand, s32 item, const coord3d *muzzle, f32 nearDepth)
{
    PelletSet *set = pelletSet(get_cur_playernum(), hand);
    Mtxf *view = camGetWorldToScreenMtxf();
    /* Multiplayer: the others see this player's third-person gun fire. */
    const s32 others = getPlayerCount() >= 2 && g_CurrentPlayer->prop != NULL && g_CurrentPlayer->prop->chr != NULL;
    const coord3d *thirdPerson = &g_CurrentPlayer->field_2A18[hand & 1];
    const coord3d *firstPerson = &g_CurrentPlayer->hands[hand & 1].field_B58;
    s32 i;

    for (i = 0; i < set->hitCount; i++)
    {
        const coord3d *hit = &set->hits[i];
        /* As gunCreateBeamForHand: no tracer for a hit nearer than the muzzle. */
        const f32 depth = -(hit->x * view->m[0][2] + hit->y * view->m[1][2] + hit->z * view->m[2][2] + view->m[3][2]);

        set->others[i].unk00 = -1;
        if (depth < nearDepth)
        {
            set->beams[i].unk00 = -1;
            continue;
        }
        portPelletBeamInit(&set->beams[i], item, muzzle, hit, pelletRandom(set));
        if (others && portPelletSameDirection(thirdPerson, firstPerson, hit))
        {
            portPelletBeamInit(&set->others[i], item, thirdPerson, hit, pelletRandom(set));
        }
    }
    set->hitCount = 0;
}

void portPelletTracersAdvance(s32 player)
{
    s32 hand, i;

    for (hand = 0; hand < 2; hand++)
    {
        PelletSet *set = pelletSet(player, hand);
        for (i = 0; i < NUMBER_SHOTGUN_BULLETS; i++)
        {
            portPelletBeamAdvance(&set->beams[i], g_GlobalTimerDelta);
            portPelletBeamAdvance(&set->others[i], g_GlobalTimerDelta);
        }
    }
}

Gfx *portPelletTracersDraw(Gfx *gdl, s32 hand)
{
    PelletSet *set = pelletSet(get_cur_playernum(), hand);
    s32 i;

    if (!portShotgunTracersEnabled()) return gdl;
    for (i = 0; i < NUMBER_SHOTGUN_BULLETS; i++)
    {
        if (set->beams[i].unk00 >= 0)
        {
            gdl = sub_GAME_7F061E18(gdl, &set->beams[i], 0);
        }
    }
    return gdl;
}

Gfx *portPelletTracersDrawOther(Gfx *gdl, s32 player)
{
    s32 hand, i;

    if (!portShotgunTracersEnabled()) return gdl;
    for (hand = 0; hand < 2; hand++)
    {
        PelletSet *set = pelletSet(player, hand);
        for (i = 0; i < NUMBER_SHOTGUN_BULLETS; i++)
        {
            if (set->others[i].unk00 >= 0)
            {
                gdl = sub_GAME_7F061E18(gdl, &set->others[i], 1);
            }
        }
    }
    return gdl;
}

/* Guards' shotgun beams, keyed by character. A slot older than
 * GUARD_SLOT_TICKS is free and never drawn, so a slot whose character went
 * away (or whose record was reused) cannot show a stale beam. Beam 0 of a
 * hand is the real shot; with Decorative Pellets the rest are a fan around it. */
#define GUARD_SLOTS 16
#define GUARD_SLOT_TICKS 60

typedef struct GuardShotgunSlot {
    const ChrRecord *chr;
    BeamRecord beams[2][NUMBER_SHOTGUN_BULLETS];
    s32 stamp;
} GuardShotgunSlot;

static void guardBeamsSpend(GuardShotgunSlot *slot)
{
    s32 hand, i;
    for (hand = 0; hand < 2; hand++)
    {
        for (i = 0; i < NUMBER_SHOTGUN_BULLETS; i++) slot->beams[hand][i].unk00 = -1;
    }
}

static GuardShotgunSlot guardSlots[GUARD_SLOTS];
static s32 guardNext;
static u32 guardRng;

static s32 guardSlotLive(const GuardShotgunSlot *slot)
{
    return slot->chr != NULL && (g_GlobalTimer - slot->stamp) >= 0 && (g_GlobalTimer - slot->stamp) < GUARD_SLOT_TICKS;
}

static GuardShotgunSlot *guardMaybeSlot(const ChrRecord *chr)
{
    s32 i;
    for (i = 0; i < GUARD_SLOTS; i++)
    {
        if (guardSlots[i].chr == chr && guardSlotLive(&guardSlots[i])) return &guardSlots[i];
    }
    return NULL;
}

static f32 guardRandom(void)
{
    u32 x = guardRng ? guardRng : 0x1B873593u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    guardRng = x;
    return (f32) (x >> 8) * (1.0f / 16777216.0f);
}

void portChrShotgunTracer(const ChrRecord *chr, s32 hand, s32 item, const coord3d *from, const coord3d *to)
{
    GuardShotgunSlot *slot;
    s32 i;

    if (!portShotgunTracersEnabled()) return;
    slot = guardMaybeSlot(chr);
    for (i = 0; slot == NULL && i < GUARD_SLOTS; i++)
    {
        if (!guardSlotLive(&guardSlots[i])) slot = &guardSlots[i];
    }
    if (slot == NULL)
    {   /* All busy: reuse the next in turn. */
        slot = &guardSlots[guardNext];
        guardNext = (guardNext + 1) % GUARD_SLOTS;
    }
    if (slot->chr != chr || !guardSlotLive(slot))
    {
        guardBeamsSpend(slot);
    }
    slot->chr = chr;
    slot->stamp = g_GlobalTimer;
    portPelletBeamInit(&slot->beams[hand & 1][0], item, from, to, guardRandom());
    for (i = 1; i < NUMBER_SHOTGUN_BULLETS; i++)
    {
        BeamRecord *beam = &slot->beams[hand & 1][i];
        coord3d end;

        beam->unk00 = -1;
        if (!portDecorativePelletsEnabled()) continue;
        /* The draws here are taken before the beam's own start offset. */
        {
            const f32 radius = guardRandom();
            const f32 turn = guardRandom();
            portPelletDecorativeEnd(from, to, radius, turn, &end);
        }
        portPelletBeamInit(beam, item, from, &end, guardRandom());
    }
}

void portChrShotgunTracersAdvance(const ChrRecord *chr)
{
    GuardShotgunSlot *slot = guardMaybeSlot(chr);
    s32 hand, i;

    if (slot == NULL) return;
    for (hand = 0; hand < 2; hand++)
    {
        for (i = 0; i < NUMBER_SHOTGUN_BULLETS; i++) portPelletBeamAdvance(&slot->beams[hand][i], g_GlobalTimerDelta);
    }
}

Gfx *portChrShotgunTracersDraw(Gfx *gdl, const ChrRecord *chr)
{
    GuardShotgunSlot *slot;
    s32 hand, i;

    if (!portShotgunTracersEnabled()) return gdl;
    slot = guardMaybeSlot(chr);
    if (slot == NULL) return gdl;
    for (hand = 0; hand < 2; hand++)
    {
        for (i = 0; i < NUMBER_SHOTGUN_BULLETS; i++)
        {
            if (slot->beams[hand][i].unk00 >= 0)
            {
                gdl = sub_GAME_7F061E18(gdl, &slot->beams[hand][i], 1);
            }
        }
    }
    return gdl;
}
