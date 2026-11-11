/* Noclip (FORK-072): the setting, per-player flight and the MoveBond hooks.
 * The rules are in port_noclip.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <fr.h>
#include "game/bondview.h"
#include "game/player.h"
#include "port_noclip.h"
#include "port_fog.h"

extern s32 get_is_ramrom_flag(void);
/* The tree's math.h hides its float declarations from GCC. */
extern float sinf(float);
extern float cosf(float);
#define NOCLIP_DEG_TO_RAD 0.017453292f
extern s32 portBondControlsLive(void);                    /* bondview2.c */
extern s32 portJoyPortNoclipPressed(s32 pad);             /* joy_port.c */
extern s32 portJoyPortNoclipControls(s32 *vertical);      /* joy_port.c: 1 = sprint held */
extern f32 g_GlobalTimerDelta;
extern void portConPrintf(s32 showmsg, const char *fmt, ...);
extern StandTile *stanFindTileBelowPos(coord3d *pos, u8 *rooms, f32 *yRtn);
extern void bondviewUpdatePlayerRoom(struct player *player);
extern void objectivestatusCheckRoomEntered(s32 roomid);
extern Gfx *portHudLabel(Gfx *g, const char *text, s32 x, s32 y, f32 scale, u32 color, s32 maxWidth);

#define NOCLIP_PLAYERS 4
/* Feet this close above a floor still count as on it. */
#define NOCLIP_FLOOR_SLACK 2.0f

/* Where a player last flew over a floor: put back there when flight ends
 * with no floor below. */
typedef struct NoclipFooting {
    s32 valid;
    f32 x, z, feet;
    StandTile *tile;
} NoclipFooting;

static s32 allowNoclip = PORT_NOCLIP_ALLOW_DEFAULT;
static s32 noclipView = PORT_NOCLIP_VIEW_DEFAULT;
static f32 viewApplied = 1.0f; /* the far multiplier the level's fog was last loaded with */
static s32 toggled[NOCLIP_PLAYERS];
static s32 flying[NOCLIP_PLAYERS];
static NoclipFooting footing[NOCLIP_PLAYERS];
static s32 landedTicks[NOCLIP_PLAYERS]; /* GE_NOCLIPDBG: ticks since landing, 0 = none */

/* GE_NOCLIPDBG=1 (stdout): flight starts and ends, and once a second while
 * flying, the position, feet and floor tile, for the noclip audit. */
static void noclipLog(const char *what, const struct player *pl)
{
    static s32 enabled = -1;
    const StandTile *t;

    if (enabled < 0)
    {
        const char *e = getenv("GE_NOCLIPDBG");
        enabled = e != NULL && e[0] != '\0' && e[0] != '0';
    }
    if (!enabled) return;
    t = pl->field_488.current_tile_ptr;
    printf("noclipdbg: %s p%d pos=(%.1f %.1f %.1f) feet=%.1f pitch=%.1f tile=%06x room=%d\n", what, get_cur_playernum(),
           pl->field_488.collision_position.f[0], pl->field_488.collision_position.f[1],
           pl->field_488.collision_position.f[2], pl->field_70, pl->vv_verta, t != NULL ? (u32) t->id : 0u,
           t != NULL ? (s32) t->room : -1);
    fflush(stdout);
}

static s32 noclipPlayer(void)
{
    const s32 p = get_cur_playernum();
    return (p >= 0 && p < NOCLIP_PLAYERS) ? p : 0;
}

s32 portNoclipOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "noclip_view") == 0)
    {
        *value = noclipView;
        return TRUE;
    }
    if (strcmp(key, "allow_noclip") != 0) return FALSE;
    *value = allowNoclip;
    return TRUE;
}

s32 portNoclipOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "noclip_view") == 0)
    {
        noclipView = portNoclipViewClamp(value); /* a flier's next tick reloads the fog */
        return TRUE;
    }
    if (strcmp(key, "allow_noclip") != 0) return FALSE;
    allowNoclip = value != 0;
    if (!allowNoclip) memset(toggled, 0, sizeof(toggled)); /* each flier lands on its next tick */
    return TRUE;
}

/* The level's far multiplier now: any player flying stretches the shared fog. */
static f32 noclipViewScale(void)
{
    s32 i, any = 0;

    for (i = 0; i < NOCLIP_PLAYERS; i++) any |= flying[i];
    return portNoclipViewScaleFor(any, noclipView);
}

void portNoclipResetPlayer(s32 player)
{
    if (player < 0 || player >= NOCLIP_PLAYERS) return;
    toggled[player] = 0;
    flying[player] = 0;
    footing[player].valid = 0;
}

/* The highest floor tile at or below the feet, NULL above none. */
static StandTile *noclipFloorBelow(const struct player *pl, f32 *floorY)
{
    coord3d probe;

    probe.f[0] = pl->field_488.collision_position.f[0];
    probe.f[1] = pl->field_70 + NOCLIP_FLOOR_SLACK;
    probe.f[2] = pl->field_488.collision_position.f[2];
    return stanFindTileBelowPos(&probe, NULL, floorY);
}

static void noclipSetTile(struct player *pl, StandTile *tile)
{
    pl->field_488.current_tile_ptr = tile;
    bondviewUpdatePlayerRoom(pl);
    objectivestatusCheckRoomEntered(tile->room);
}

static void noclipStart(s32 p)
{
    struct player *pl = g_CurrentPlayer;

    /* Leave the native drop-through state as its own landing would. */
    if (pl->field_2A6C != 0)
    {
        pl->field_2A6C = 0;
        pl->field_488.current_tile_ptr = pl->field_2A70;
        pl->field_2A70 = NULL;
    }
    pl->field_7C = 0.0f;
    footing[p].valid = pl->field_488.current_tile_ptr != NULL;
    footing[p].x = pl->field_488.collision_position.f[0];
    footing[p].z = pl->field_488.collision_position.f[2];
    footing[p].feet = pl->field_70;
    footing[p].tile = pl->field_488.current_tile_ptr;
}

/* Back on foot: onto the floor below (gravity drops him there), or with
 * none, where he last flew over one. */
static void noclipLand(s32 p)
{
    struct player *pl = g_CurrentPlayer;
    f32 floorY = 0.0f;
    StandTile *tile = noclipFloorBelow(pl, &floorY);

    pl->field_7C = 0.0f;
    if (tile != NULL)
    {
        noclipSetTile(pl, tile);
        return;
    }
    if (!footing[p].valid) return;
    pl->field_488.collision_position.f[0] = footing[p].x;
    pl->field_488.collision_position.f[2] = footing[p].z;
    pl->bondprevpos = pl->field_488.collision_position;
    pl->field_70 = footing[p].feet;
    pl->stanHeight = footing[p].feet;
    noclipSetTile(pl, footing[p].tile);
}

void portNoclipTick(void)
{
    const s32 p = noclipPlayer();
    struct player *pl = g_CurrentPlayer;
    s32 fly;

    if (pl == NULL) return;
    if (portJoyPortNoclipPressed(p) && portBondControlsLive())
    {
        if (portNoclipPressToggles(allowNoclip))
        {
            toggled[p] = !toggled[p];
            portConPrintf(1, toggled[p] ? "Noclip on" : "Noclip off");
        }
        else
        {
            portConPrintf(1, "Noclip is off: turn on Gameplay > Iron Sights > Aiming and Movement > Allow Noclip");
        }
    }
    fly = portNoclipFliesFor(allowNoclip, toggled[p], get_is_ramrom_flag() != 0, g_PlayerIsInTank != 0,
                             pl->bonddead != 0);
    if (fly && !flying[p])
    {
        noclipStart(p);
        noclipLog("start", pl);
    }
    if (!fly && flying[p])
    {
        noclipLand(p);
        noclipLog("land", pl);
        landedTicks[p] = 1;
    }
    else if (landedTicks[p] > 0 && ++landedTicks[p] > 90)
    {
        landedTicks[p] = 0;
        noclipLog("after-land", pl); /* where gravity left him */
    }
    flying[p] = fly;
    /* view distance follows flight, and the level's fog follows the Fog setting (FORK-074) */
    if (noclipViewScale() != viewApplied || portFogNeedsReload()) portFogReapply();
}

s32 portNoclipAllRooms(void)
{
    return flying[noclipPlayer()];
}

f32 portNoclipViewScaleApply(void)
{
    viewApplied = noclipViewScale();
    return viewApplied;
}

s32 portNoclipActive(void)
{
    return flying[noclipPlayer()];
}

void portNoclipMove(const coord3d *walk)
{
    const s32 p = noclipPlayer();
    struct player *pl = g_CurrentPlayer;
    coord3d step;
    f32 floorY = 0.0f;
    StandTile *tile;

    { /* GE_NOCLIPDBG_PITCH=deg[,ticks,deg2] (audit only): hold the view at that
       * pitch while flying (deg2 after that many flying ticks), since the
       * walk's look-ahead recentres any spawn pitch */
        static s32 parsed, held, after = -1;
        static f32 first, second;
        if (!parsed)
        {
            const char *e = getenv("GE_NOCLIPDBG_PITCH");
            parsed = 1;
            held = e != NULL && e[0] != '\0';
            if (held && sscanf(e, "%f,%d,%f", &first, &after, &second) < 3) after = -1;
        }
        if (held)
        {
            static s32 ticks;
            pl->vv_verta = (after >= 0 && ticks++ >= after) ? second : first;
            pl->vv_sinverta = sinf(pl->vv_verta * NOCLIP_DEG_TO_RAD);
            pl->vv_cosverta = cosf(pl->vv_verta * NOCLIP_DEG_TO_RAD);
        }
    }
    portNoclipFlyStep(walk, pl->field_488.theta_transform.f[0], pl->field_488.theta_transform.f[2],
                      pl->vv_sinverta, pl->vv_cosverta, &step);
    {
        s32 vertical;
        const s32 sprint = portJoyPortNoclipControls(&vertical);
        const f32 speed = portNoclipSpeedFor(sprint);

        step.f[0] *= speed;
        step.f[1] = step.f[1] * speed + portNoclipClimbFor(vertical, sprint, g_GlobalTimerDelta);
        step.f[2] *= speed;
    }
    pl->bondprevpos = pl->field_488.collision_position;
    pl->field_488.collision_position.f[0] += step.f[0];
    pl->field_488.collision_position.f[2] += step.f[2];
    pl->field_70 += step.f[1];
    pl->field_7C = 0.0f;
    pl->autocrouchpos = CROUCH_STAND;

    {
        static s32 ticks;
        if ((ticks++ % 60) == 0) noclipLog("fly", pl);
    }
    tile = noclipFloorBelow(pl, &floorY);
    if (tile == NULL) return; /* above no floor: keep the last room */
    noclipSetTile(pl, tile);
    footing[p].valid = 1;
    footing[p].x = pl->field_488.collision_position.f[0];
    footing[p].z = pl->field_488.collision_position.f[2];
    footing[p].feet = pl->field_70;
    footing[p].tile = tile;
}

Gfx *portNoclipDrawLabel(Gfx *g)
{
    if (!portNoclipActive()) return g;
    return portHudLabel(g, "NOCLIP", viGetViewLeft() + viGetViewWidth() - 64, viGetViewTop() + 12, 0.7f,
                        0xf0d994ff, 56);
}
