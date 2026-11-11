/* Opt-in Projectile Arc (FORK-069) audit; no ordinary gameplay changes.
 *
 * GE_ARC_AUDIT=1 (or "rocket", "grenade", "knife" or "mine") turns the arc
 * on for the run (never written to midas.ini), equips the grenade launcher
 * (or the rocket launcher, hand grenades, throwing knives or remote mines),
 * (GE_ARC_AUDIT_DUAL: the same in the left hand too; "shells": tank shells
 * carried on foot), holds
 * aim only with GE_ARC_AUDIT_ADS set, steps the look pitch through
 * GE_ARC_AUDIT_PITCH (degrees, default "-12,-4,4,12,24") every
 * GE_ARC_AUDIT_HOLD ticks (default 360), and requests a screenshot a third
 * and two thirds of the way through each pitch. Pair it with GE_AUTOFIRE,
 * GE_CHEATS=guns (ammo) and GE_ARCDBG to log each round's predicted and
 * actual explosion. GE_ARC_AUDIT=tank instead boards the level's tank
 * (walk Bond onto it with GE_SPAWNXYZ and GE_GOTO; it taps Use), selects the
 * shells and steps the turret pitch. GE_ARC_AUDIT_FACE=1 turns the player
 * each tick towards the nearest living character, the pitches then counting
 * from the line to its feet (a knife's strike on a guard). Exits when the
 * list ends. */
#if defined(GE_VIDEO) && !defined(__EMSCRIPTEN__) && !defined(GE_XBOX)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "game/bondview.h"
#include "game/player.h"
#include "game/gun.h"
#include "game/chrai.h"
#include "port_projectile_arc.h"

extern s32 portBondControlsLive(void);
extern int ge_shot_force;
extern void portConLogf(const char *, ...);
extern s32 get_cur_playernum(void);

static s32 arcAuditMode(void)
{
    static s32 mode = -1;
    const char *e;

    if (mode >= 0) return mode;
    e = getenv("GE_ARC_AUDIT");
    mode = (e == NULL || e[0] == '\0') ? 0 : (e[0] == 't' ? 2 : e[0] == 'r' ? 3 : e[0] == 'g' ? 4 : e[0] == 'k' ? 5 : e[0] == 'm' ? 6 : e[0] == 's' ? 7 : 1);
    return mode;
}

static s32 arcAuditPitch(s32 index, f32 *degrees)
{
    const char *list = getenv("GE_ARC_AUDIT_PITCH");
    const char *p = (list != NULL && list[0] != '\0') ? list : "-12,-4,4,12,24";
    s32 i;

    for (i = 0; i < index; i++)
    {
        while (*p != ',' && *p != '\0') p++;
        if (*p == '\0') return FALSE;
        p++;
    }
    *degrees = (f32) atof(p);
    return TRUE;
}

/* The tank audit first boards the tank: spawn on it (GE_SPAWNXYZ, from the
 * position this logs) and it taps Use until the player is inside. */
static PropRecord *arcAuditFindTank(void)
{
    s32 i;

    for (i = 0; i < MAX_PROPS; i++)
    {
        PropRecord *prop = &g_Props[i];

        if (prop->type == PROP_TYPE_OBJ && prop->obj != NULL && prop->obj->type == PROPDEF_TANK) return prop;
    }
    return NULL;
}

/* Whether the two props share a room: a cheap stand-in for a clear view. */
static s32 arcAuditSameRoom(const PropRecord *a, const PropRecord *b)
{
    s32 i, j;

    for (i = 0; i < PROPRECORD_STAN_ROOM_LEN && a->rooms[i] != 0xff; i++)
    {
        for (j = 0; j < PROPRECORD_STAN_ROOM_LEN && b->rooms[j] != 0xff; j++)
        {
            if (a->rooms[i] == b->rooms[j]) return TRUE;
        }
    }
    return FALSE;
}

/* GE_ARC_AUDIT_FACE: the yaw and pitch, in degrees, from the player to the
 * nearest living character's feet that shares a room with the player and
 * stands at least 250 units off (the arc fades in over its first stretch);
 * FALSE when there is none. */
static s32 arcAuditFaceChr(f32 *theta, f32 *verta, coord3d *at)
{
    const coord3d *me = &g_CurrentPlayer->prop->pos;
    f32 best = 1e30f;
    s32 found = FALSE;
    s32 i;

    for (i = 0; i < MAX_PROPS; i++)
    {
        const PropRecord *prop = &g_Props[i];
        f32 dx, dy, dz, d2;

        if (prop->type != PROP_TYPE_CHR || prop->chr == NULL) continue;
        if (prop->chr->actiontype == ACT_DIE || prop->chr->actiontype == ACT_DEAD) continue;
        dx = prop->pos.x - me->x;
        dy = prop->pos.y - me->y;
        dz = prop->pos.z - me->z;
        d2 = dx * dx + dz * dz;
        if (d2 >= best || d2 < 250.0f * 250.0f || !arcAuditSameRoom(prop, g_CurrentPlayer->prop)) continue;
        best = d2;
        *theta = atan2f(-dx, dz) * (180.0f / M_PI_F);
        *verta = atan2f(dy, sqrtf(d2)) * (180.0f / M_PI_F);
        *at = prop->pos;
        found = TRUE;
    }
    return found;
}

static s32 arcAuditBoardTank(s32 waited)
{
    PropRecord *tank = arcAuditFindTank();

    if (g_PlayerIsInTank == 1) return 0;
    if (waited == 1 && tank != NULL)
    {
        portConLogf("ge: arc-audit: tank at %.1f,%.1f,%.1f player at %.1f,%.1f,%.1f\n",
                    tank->pos.x, tank->pos.y, tank->pos.z, g_CurrentPlayer->prop->pos.x,
                    g_CurrentPlayer->prop->pos.y, g_CurrentPlayer->prop->pos.z);
    }
    if (waited > 600)
    {
        portConLogf("ge: arc-audit: could not board the tank (on it: %d)\n", g_BondCanEnterTank);
        exit(3);
    }
    return (waited % 20 == 10) ? PORT_ARC_AUDIT_USE : 0;
}

/* portAdsTick, each tick: PORT_ARC_AUDIT_AIM while the audit holds aim,
 * PORT_ARC_AUDIT_USE to tap Use. */
s32 portProjectileArcAuditAim(void)
{
    static s32 ticks;
    static s32 boarding;
    static s32 hold = -1;
    s32 mode = arcAuditMode();
    s32 step;
    f32 pitch;

    if (mode == 0 || g_CurrentPlayer == NULL || get_cur_playernum() != 0 || !portBondControlsLive()) return FALSE;
    if (hold < 0)
    {
        const char *e = getenv("GE_ARC_AUDIT_HOLD");
        hold = (e != NULL && atoi(e) >= 60) ? atoi(e) : 360;
    }
    if (mode == 2 && g_PlayerIsInTank != 1) return arcAuditBoardTank(++boarding);
    ticks++;
    if (ticks == 1)
    {
        /* GE_ARC_AUDIT_NOARC: leave the arc off, for the Arc Impact Crosshair alone */
        portProjectileArcOptionSet("projectile_arc", getenv("GE_ARC_AUDIT_NOARC") == NULL);
        currentPlayerEquipWeaponWrapper(GUNRIGHT, (mode == 1) ? ITEM_GRENADELAUNCH
                                                  : (mode == 3) ? ITEM_ROCKETLAUNCH
                                                  : (mode == 4) ? ITEM_GRENADE
                                                  : (mode == 5) ? ITEM_THROWKNIFE
                                                  : (mode == 6) ? ITEM_REMOTEMINE : ITEM_TANKSHELLS); /* 2 and 7 */
        if (getenv("GE_ARC_AUDIT_DUAL") != NULL && mode != 2)
        {   /* the same weapon in the left hand: an arc for each */
            currentPlayerEquipWeaponWrapper(GUNLEFT, (mode == 1) ? ITEM_GRENADELAUNCH
                                                    : (mode == 3) ? ITEM_ROCKETLAUNCH
                                                    : (mode == 5) ? ITEM_THROWKNIFE : ITEM_REMOTEMINE);
        }
    }
    step = ticks / hold;
    if (!arcAuditPitch(step, &pitch))
    {
        portConLogf("ge: arc-audit: complete (%d pitches)\n", step);
        exit(0);
    }
    if (mode == 2)
    {
        g_TankTurretVerticalAngle = pitch * (M_PI_F / 180.0f);
    }
    else
    {
        f32 theta, verta;
        coord3d at;

        if (getenv("GE_ARC_AUDIT_FACE") != NULL && arcAuditFaceChr(&theta, &verta, &at))
        {
            if (ticks % hold == hold / 3)
            {
                portConLogf("ge: arc-audit: facing chr at %.1f,%.1f,%.1f from %.1f,%.1f,%.1f\n", at.x, at.y, at.z,
                            g_CurrentPlayer->prop->pos.x, g_CurrentPlayer->prop->pos.y, g_CurrentPlayer->prop->pos.z);
            }
            g_CurrentPlayer->vv_theta = (theta < 0.0f) ? theta + 360.0f : theta;
            pitch += verta;
        }
        g_CurrentPlayer->vv_verta = pitch;
    }
    if (ticks % hold == hold / 3 || ticks % hold == (2 * hold) / 3)
    {
        portConLogf("ge: arc-audit: capture pitch=%.1f yaw=%.1f tick=%d\n", pitch, g_CurrentPlayer->vv_theta, ticks);
        {   /* the right hand's arc: where it ends, and whether that is in sight */
            PortArcLaunch shown;
            const PortArcPath *path = portProjectileArcLast(0, &shown);

            if (path != NULL)
            {
                portConLogf("ge: arc-audit: impact=%d at %.1f,%.1f,%.1f out of sight=%d\n",
                            path->impact, path->end.x, path->end.y, path->end.z, path->endHidden);
            }
        }
        ge_shot_force = 1;
    }
    return (mode != 2 && ticks > 1 && getenv("GE_ARC_AUDIT_ADS") != NULL) ? PORT_ARC_AUDIT_AIM : 0;
}

/* GE_ARC_AUDIT_STRIKE: draw the marker for a character hit (a knife stuck
 * in them, a round's or rocket's burst) at the end of every arc, so its look
 * can be captured without a character in view. */
s32 portProjectileArcAuditStrike(void)
{
    static s32 strike = -1;

    if (strike < 0) strike = arcAuditMode() != 0 && getenv("GE_ARC_AUDIT_STRIKE") != NULL;
    return strike;
}

/* GE_ARC_AUDIT_HIDDEN: draw every Arc Impact Crosshair as out of sight, to
 * capture its dashed cross. */
s32 portProjectileArcAuditHidden(void)
{
    static s32 hidden = -1;

    if (hidden < 0) hidden = arcAuditMode() != 0 && getenv("GE_ARC_AUDIT_HIDDEN") != NULL;
    return hidden;
}
#else
int portProjectileArcAuditAim(void) { return 0; }
int portProjectileArcAuditHidden(void) { return 0; }
int portProjectileArcAuditStrike(void) { return 0; }
#endif
