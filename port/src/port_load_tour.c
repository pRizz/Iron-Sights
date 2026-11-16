/**
 * port_load_tour.c - GE_LOADTOUR: show a stage's every room, for audits.
 *
 *   GE_LOADTOUR=<ticks>[,<settle>]   e.g. GE_LOADTOUR=24
 *
 * Once Bond has control (the intro over) and <settle> more ticks have passed
 * (default 60), Bond is put on one pad of each room in turn, in the setup's
 * pad order, and turned a full circle over <ticks> ticks (default 24) at each,
 * so every room's walls, props and textures come into view at least once.
 * After the last room the run ends: the frame log (GE_BENCH) is written as at
 * its end tick, or the game quits. scripts/port/late_load_audit.py uses this
 * to find loading work that waits for the player's first look
 * (port_late_loads.c, scripts/port/late_loads.md).
 *
 * The tour is an audit aid only: it moves the player as noclip's landing does
 * and skips the room-entry objective check, though level scripts that watch
 * Bond's room may still react. Use invulnerability and invisibility cheats.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "game/bondview.h"
#include "game/chrai.h"
#include "game/lv.h"
#include "game/player.h"

#define TOUR_MAX_ROOMS 256

extern s32 portBondControlsLive(void);                   /* bondview2.c */
extern void bondviewUpdatePlayerRoom(struct player *player);
extern f32 bondviewYPositionRelated(struct StandTile *tile, f32 x, f32 z);
extern int portBenchRequestEnd(void);                     /* port_bench.c */
extern void portRequestQuit(void);                        /* video.c */
extern void portConLogf(const char *fmt, ...);

static s32 sTourState = -1; /* -1 unread, 0 off, 1 waiting, 2 touring, 3 done */
static s32 sTourDwell = 24, sTourSettle = 60;
static s32 sTourWait, sTourStep, sTourTick, sTourCount, sTourStage = -1;
static s16 sTourPads[TOUR_MAX_ROOMS];

static void tourRead(void)
{
    const char *spec = getenv("GE_LOADTOUR");
    int dwell = 0, settle = -1;

    sTourState = 0;
    if (spec == NULL || spec[0] == '\0' || sscanf(spec, "%d,%d", &dwell, &settle) < 1)
    {
        return;
    }
    if (dwell > 0) sTourDwell = dwell;
    if (settle >= 0) sTourSettle = settle;
    sTourState = 1;
}

/* One pad per room, the first the setup lists for it. */
static void tourPickPads(void)
{
    u8 seen[TOUR_MAX_ROOMS] = { 0 };
    PadRecord *pad;

    sTourCount = 0;
    if (g_CurrentSetup.pads == NULL)
    {
        return;
    }
    for (pad = g_CurrentSetup.pads; pad->plink != NULL && sTourCount < TOUR_MAX_ROOMS; pad++)
    {
        const u32 room = pad->stan != NULL ? pad->stan->room : 0xFFu;

        if (room >= TOUR_MAX_ROOMS || seen[room])
        {
            continue;
        }
        seen[room] = 1;
        sTourPads[sTourCount++] = (s16) (pad - g_CurrentSetup.pads);
    }
}

/* Stand Bond on the pad's tile, as noclip's landing does. */
static void tourPlace(struct player *pl, const PadRecord *pad)
{
    const f32 feet = bondviewYPositionRelated(pad->stan, pad->pos.f[0], pad->pos.f[2]);

    pl->field_488.collision_position.f[0] = pad->pos.f[0];
    pl->field_488.collision_position.f[2] = pad->pos.f[2];
    pl->bondprevpos = pl->field_488.collision_position;
    pl->field_70 = feet;
    pl->stanHeight = feet;
    pl->field_488.current_tile_ptr = pad->stan;
    bondviewUpdatePlayerRoom(pl);
}

static void tourFinish(const char *why)
{
    sTourState = 3;
    portConLogf("ge: loadtour: done, %d of %d rooms%s\n", (int) sTourStep, (int) sTourCount, why);
    if (!portBenchRequestEnd())
    {
        portRequestQuit();
    }
}

/** file.c, when a mission ends: a room the tour entered finished it (Dam's
 * bungee), and the debrief would wait for input, so the run ends here. */
void portLoadTourMissionEnded(void)
{
    if (sTourState == 2)
    {
        tourFinish(" (the mission ended there)");
    }
}

/** bondview2.c, each player tick after input: the tour's next step. */
void portLoadTourTick(void)
{
    struct player *pl = g_CurrentPlayer;
    extern s32 g_StageNum;

    if (sTourState < 0) tourRead();
    if (sTourState == 0 || sTourState == 3 || pl == NULL || get_cur_playernum() != 0)
    {
        return;
    }
    if (sTourStage != g_StageNum)
    {
        sTourStage = g_StageNum;
        sTourState = 1;
        sTourWait = 0;
    }
    if (sTourState == 1)
    {
        if (!portBondControlsLive() || ++sTourWait < sTourSettle)
        {
            return;
        }
        tourPickPads();
        portConLogf("ge: loadtour: stage %d, %d rooms, %d ticks each\n", (int) g_StageNum, (int) sTourCount,
                    (int) sTourDwell);
        sTourState = 2;
        sTourStep = 0;
        sTourTick = 0;
    }
    if (sTourStep >= sTourCount)
    {
        tourFinish("");
        return;
    }
    {
        const PadRecord *pad = &g_CurrentSetup.pads[sTourPads[sTourStep]];

        if (sTourTick == 0)
        {
            tourPlace(pl, pad);
            portConLogf("ge: loadtour: room %u (pad %d) at tick %d\n", (unsigned) pad->stan->room,
                        (int) sTourPads[sTourStep], (int) g_GlobalTickCount);
        }
        pl->vv_theta = 360.0f * (f32) sTourTick / (f32) sTourDwell;
        pl->vv_verta = 0.0f;
        if (++sTourTick >= sTourDwell)
        {
            sTourTick = 0;
            sTourStep++;
        }
    }
}
