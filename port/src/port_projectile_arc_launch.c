/* Projectile Arc (FORK-069), the launch: where a tank shell, grenade round,
 * rocket or hand grenade would leave the gun, derived as gunFireTankShell,
 * gunSpawnGLGrenade and generate_player_thrown_grenade derive a real one,
 * but without bullet_path_from_screen_center's random spread, so the game
 * RNG is never drawn. port_projectile_arc_world.c predicts its flight. */
#include <math.h>
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/chrai.h"
#include "game/player.h"
#include "game/gun.h"
#include "game/lv.h"
#include "game/matrixmath.h"
#include "game/frametiming.h"
#include "port_projectile_arc.h"
#include "port_settle_ticks.h"

extern s32 portTankTurretWeaponHeld(void);
extern s32 get_cur_playernum(void);

s32 portProjectileArcPlayerReady(void)
{
    return g_CurrentPlayer != NULL && !g_CurrentPlayer->bonddead && g_CurrentPlayer->mpmenuon == 0
        && lvlGetControlsLockedFlag() == 0;
}

/* How far ahead to predict (a shell's 160 fields cover about 10,700 units,
 * past the game's draw distance; a grenade lands well within 240; a rocket
 * reaches its speed cap of about 167 units a field within 150) and in what
 * steps: three fields for a shell (200 units), two for a grenade, each chord
 * within a third of a unit of the curve; portArcSimulate flies the landing
 * step again a field at a time, as the game moves a round. A rocket moves
 * a field at a time with its floor checked each field, its collisions on
 * three-field chords (portArcSimulateRocket). */
static f32 sArcTickStep = 1.0f;

void portProjectileArcFlightFor(PortArcLaunch *launch)
{
    const s32 kind = launch->kind;

    launch->maxFields = (kind == PORT_ARC_SHELL) ? 160.0f : (kind == PORT_ARC_ROCKET) ? 200.0f
                      : (kind == PORT_ARC_THROWN) ? (f32) PORT_ARC_HAND_FUSE + 1.0f : 240.0f;
    launch->step = (launch->kind == PORT_ARC_GRENADE) ? 2.0f : 3.0f;
    /* The game explodes a round once its origin is within its model's
     * bottom offset of the floor; measured from where the blasts land
     * (4 units above that height): about 8 for a shell or rocket (one
     * model, PROP_CHRROCKET), 5.5 for a grenade round. A hand grenade
     * bounces there instead, and rests 4 units above it. */
    launch->floorClearance = (kind == PORT_ARC_GRENADE) ? 5.5f
                           : (kind == PORT_ARC_THROWN) ? PORT_ARC_HAND_CLEARANCE
                           : (kind == PORT_ARC_KNIFE) ? PORT_ARC_KNIFE_CLEARANCE
                           : (kind == PORT_ARC_MINE) ? PORT_ARC_MINE_CLEARANCE : 8.2f;
    /* Thrown objects: a hand grenade's unk8C and unk94 (gun.c); a knife's or
     * mine's unk8C, with no unk94, so PROJECTILE_FRICTION_FACTOR. */
    launch->bounce = (kind == PORT_ARC_THROWN) ? PORT_ARC_HAND_BOUNCE : PORT_ARC_LIGHT_BOUNCE;
    launch->restHeight = (kind == PORT_ARC_THROWN) ? PORT_ARC_HAND_REST
                       : (kind == PORT_ARC_KNIFE) ? PORT_ARC_KNIFE_REST : PORT_ARC_MINE_REST;
    launch->turnFields = (kind == PORT_ARC_THROWN) ? PORT_ARC_HAND_TURN_FIELDS
                       : (kind == PORT_ARC_KNIFE) ? PORT_ARC_KNIFE_TURN : PORT_ARC_MINE_TURN;
    launch->sticks = kind == PORT_ARC_MINE;
    launch->strikes = kind == PORT_ARC_KNIFE;
    launch->tickStep = sArcTickStep;
    launch->slideFriction = (kind == PORT_ARC_THROWN)
        ? PORT_ARC_HAND_FRICTION * portSettleFrictionStepFor(portTickHz() != 60, sArcTickStep) : 0.0f;
}

/* portProjectileArcTick, each tick: the step of the latest whole-field tick
 * (g_GlobalTimerDelta where g_ClockTimer > 0), which at 60 Hz scales a
 * settled grenade's friction: 1 at a steady rate, 2 while frames take two
 * fields. Above 60 Hz the game applies a whole field's friction
 * (SLIDE_FRICTION_STEP in propobj.c, FORK-070). Smoothed and rounded so a
 * steady rate gives a steady launch (a changed launch is predicted again). */
void portProjectileArcNoteTickStep(void)
{
    static f32 smooth = 1.0f;

    if (g_ClockTimer <= 0 || !(g_GlobalTimerDelta > 0.0f)) return;
    smooth += (g_GlobalTimerDelta - smooth) * 0.1f;
    sArcTickStep = (f32) (s32) (smooth * 50.0f + 0.5f) / 50.0f;
    if (sArcTickStep < 0.02f) sArcTickStep = 0.02f;
}

/* gunFireTankShell's aim and spawn: from the turret's muzzle along the
 * turret while the tank reports running, else from the player along the
 * crosshair, as that function chooses (the second for shells carried on
 * foot, as the All Guns cheat gives). */
static s32 arcTankShell(PortArcLaunch *launch)
{
    PropRecord *tank = get_ptr_for_players_tank();
    ObjectRecord *tankobj = (tank != NULL) ? tank->obj : NULL;
    coord3d aimdir;

    if (tank != NULL && (tank->flags & TANK_RUN_STATE_RUNNING))
    {
        if (tankobj == NULL || tankobj->model == NULL || tankobj->model->render_pos == NULL) return FALSE;
        bondviewSet3dCoord7F07CEB0(&aimdir);
        launch->pos.x = tankobj->model->render_pos[4].pos.m[3][0];
        launch->pos.y = tankobj->model->render_pos[4].pos.m[3][1];
        launch->pos.z = tankobj->model->render_pos[4].pos.m[3][2];
        mtx4TransformVecInPlace(currentPlayerGetViewToWorldMtxf(), &launch->pos);
    }
    else
    {
        coord3d zero;

        sub_GAME_7F068190(&zero, &aimdir);
        mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), &aimdir);
        launch->pos = getCurrentPlayerProp()->pos;
    }
    launch->kind = PORT_ARC_SHELL;
    launch->accel.x = launch->accel.y = launch->accel.z = 0.0f;
    launch->vel.x = aimdir.x * PORT_ARC_TANK_SPEED;
    launch->vel.y = aimdir.y * PORT_ARC_TANK_SPEED;
    launch->vel.z = aimdir.z * PORT_ARC_TANK_SPEED;
    portProjectileArcFlightFor(launch);
    return TRUE;
}

/* Driving the tank with the shells selected. */
static s32 arcTankLaunch(PortArcLaunch *launch)
{
    return portTankTurretWeaponHeld() && arcTankShell(launch);
}

/* gunSpawnGLGrenade's and gunFireTankShell's aim and spawn for a launcher
 * in either hand: from its drawn muzzle along the crosshair (both hands aim
 * there), without bullet_path_from_screen_center's random spread. A grenade leaves at its
 * speed; a rocket at three fields of its thrust (PLAYER_LAUNCH_CADENCE, or
 * the recorded cadence in demo playback), keeping the thrust. */
static s32 arcLauncherLaunch(s32 hand, PortArcLaunch *launch)
{
    extern s32 is_ramrom_flag;
    extern s32 g_ramromPlayBackFlag;
    extern s32 portTickCadence;
    const s32 item = get_item_in_hand_or_watch_menu(hand);
    coord3d zero;
    coord3d aimdir;

    if (item == ITEM_TANKSHELLS && !portTankTurretWeaponHeld()) return arcTankShell(launch);
    if (item != ITEM_GRENADELAUNCH && item != ITEM_ROCKETLAUNCH) return FALSE;

    sub_GAME_7F068190(&zero, &aimdir);
    mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), &aimdir);
    launch->pos = g_CurrentPlayer->hands[hand].field_B58;
    gunWeaponFovLaunch(hand, &launch->pos, &aimdir);
    if (item == ITEM_GRENADELAUNCH)
    {
        launch->kind = PORT_ARC_GRENADE;
        launch->accel.x = launch->accel.y = launch->accel.z = 0.0f;
        launch->vel.x = aimdir.x * PORT_ARC_GL_SPEED;
        launch->vel.y = aimdir.y * PORT_ARC_GL_SPEED;
        launch->vel.z = aimdir.z * PORT_ARC_GL_SPEED;
    }
    else
    {
        const f32 cadence = (is_ramrom_flag != 0 || g_ramromPlayBackFlag != 0)
                              ? (f32) portTickCadence : PORT_ARC_ROCKET_CADENCE;

        launch->kind = PORT_ARC_ROCKET;
        launch->accel.x = aimdir.x * PORT_ARC_ROCKET_THRUST;
        launch->accel.y = aimdir.y * PORT_ARC_ROCKET_THRUST;
        launch->accel.z = aimdir.z * PORT_ARC_ROCKET_THRUST;
        launch->vel.x = launch->accel.x * cadence;
        launch->vel.y = launch->accel.y * cadence;
        launch->vel.z = launch->accel.z * cadence;
    }
    portProjectileArcFlightFor(launch);
    return TRUE;
}

/* generate_player_thrown_grenade's throw, for a hand grenade in either
 * hand: from the hand as drawn, along the crosshair at its speed plus a lift
 * (the release pose is near the hand's held one). The fuse runs from the
 * throw and explodes the grenade the tick after it runs out; holding the
 * trigger cooks it, taking the fields held off the fuse. Nothing once this
 * throw's grenade has left the hand. */
static s32 sArcThrown[4][2];

static s32 *arcThrownFlag(s32 hand)
{
    s32 p = get_cur_playernum();

    return &sArcThrown[(p >= 0 && p < 4) ? p : 0][hand != 0];
}

/* generate_player_thrown_grenade, through portProjectileArcShotFired: the
 * hand mid-throw is the one it left. */
void portProjectileArcGrenadeThrown(void)
{
    s32 hand;

    for (hand = 0; hand < 2; hand++)
    {
        if (g_CurrentPlayer->hands[hand].weapon_action_state == GUN_ANIM_STATE_GRENADE_THROW) *arcThrownFlag(hand) = TRUE;
    }
}

static s32 arcThrownLaunch(s32 handnum, PortArcLaunch *launch)
{
    struct hand *hand = &g_CurrentPlayer->hands[handnum];
    s32 *thrown = arcThrownFlag(handnum);
    s32 held = 0;
    s32 fuse;
    coord3d zero;
    coord3d aimdir;

    if (hand->weapon_action_state != GUN_ANIM_STATE_GRENADE_THROW) *thrown = FALSE;
    if (get_item_in_hand_or_watch_menu(handnum) != ITEM_GRENADE) return FALSE;
    if (*thrown || hand->weapon_ammo_in_magazine <= 0) return FALSE;

    if (hand->weapon_action_state == GUN_ANIM_STATE_TRIGGER_PRESS) held = hand->field_890;
    else if (hand->weapon_action_state == GUN_ANIM_STATE_GRENADE_THROW) held = g_CurrentPlayer->last_z_trigger_timer;
    fuse = PORT_ARC_HAND_FUSE - held;
    if (fuse < 0) fuse = 0;

    sub_GAME_7F068190(&zero, &aimdir);
    mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), &aimdir);
    launch->pos.x = hand->throw_item_pos_related.m[3][0];
    launch->pos.y = hand->throw_item_pos_related.m[3][1];
    launch->pos.z = hand->throw_item_pos_related.m[3][2];
    gunWeaponFovLaunch(handnum, &launch->pos, &aimdir);
    launch->kind = PORT_ARC_THROWN;
    launch->accel.x = launch->accel.y = launch->accel.z = 0.0f;
    launch->vel.x = aimdir.x * PORT_ARC_HAND_SPEED;
    launch->vel.y = aimdir.y * PORT_ARC_HAND_SPEED + PORT_ARC_HAND_LIFT;
    launch->vel.z = aimdir.z * PORT_ARC_HAND_SPEED;
    portProjectileArcFlightFor(launch);
    launch->maxFields = (f32) fuse + 1.0f;
    return TRUE;
}

/* generate_player_thrown_knife's and generate_player_thrown_object's throw,
 * for a throwing knife or a remote, timed or proximity mine in either hand:
 * from the hand as drawn, along the crosshair at its speed plus the same
 * lift as a grenade. */
static s32 arcObjectLaunch(s32 handnum, PortArcLaunch *launch)
{
    const s32 item = get_item_in_hand_or_watch_menu(handnum);
    struct hand *hand = &g_CurrentPlayer->hands[handnum];
    f32 speed;
    coord3d zero;
    coord3d aimdir;

    if (item == ITEM_THROWKNIFE)
    {
        launch->kind = PORT_ARC_KNIFE;
        speed = PORT_ARC_KNIFE_SPEED;
    }
    else if (item == ITEM_REMOTEMINE || item == ITEM_TIMEDMINE || item == ITEM_PROXIMITYMINE)
    {
        launch->kind = PORT_ARC_MINE;
        speed = PORT_ARC_HAND_SPEED;
    }
    else
    {
        return FALSE;
    }
    sub_GAME_7F068190(&zero, &aimdir);
    mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), &aimdir);
    launch->pos.x = hand->throw_item_pos_related.m[3][0];
    launch->pos.y = hand->throw_item_pos_related.m[3][1];
    launch->pos.z = hand->throw_item_pos_related.m[3][2];
    gunWeaponFovLaunch(handnum, &launch->pos, &aimdir);
    launch->accel.x = launch->accel.y = launch->accel.z = 0.0f;
    launch->vel.x = aimdir.x * speed;
    launch->vel.y = aimdir.y * speed + PORT_ARC_HAND_LIFT;
    launch->vel.z = aimdir.z * speed;
    portProjectileArcFlightFor(launch);
    return TRUE;
}

/* The player's motion, which a round keeps. The game measures it only on
 * frames where a tick ran (g_ClockTimer > 0); frames drawn faster than the
 * tick rate saw none in between and alternated between two arcs, so each
 * player's last measured motion carries over. Player launches happen on
 * whole-field ticks, since input and the weapon state run at 60 Hz. */
static coord3d sArcMotion[4];

static void arcAddMotion(PortArcLaunch *launch, const PropRecord *playerprop, const coord3d *prev)
{
    s32 p = get_cur_playernum();
    coord3d *motion = &sArcMotion[(p >= 0 && p < 4) ? p : 0];

    const coord3d was = *motion;

    if (g_ClockTimer > 0 && g_GlobalTimerDelta > 0.0f)
    {
        motion->x = (playerprop->pos.x - prev->x) / g_GlobalTimerDelta;
        motion->y = (playerprop->pos.y - prev->y) / g_GlobalTimerDelta;
        motion->z = (playerprop->pos.z - prev->z) / g_GlobalTimerDelta;
    }
    if (portProjectileArcDebug())
    {   /* GE_ARCDBG: frames that had no tick (motion kept), and jumps in it */
        static s32 frames, dropped, jumps;
        const f32 dx = motion->x - was.x, dz = motion->z - was.z;

        if (g_ClockTimer <= 0 && motion->x * motion->x + motion->z * motion->z > 0.25f) dropped++;
        if (dx * dx + dz * dz > 1.0f) jumps++;
        if (++frames == 240)
        {
            printf("arcdbg: motion: %d of %d frames had no tick (motion kept), carried motion jumped in %d\n",
                   dropped, frames, jumps);
            fflush(stdout);
            frames = dropped = jumps = 0;
        }
    }
    launch->vel.x += motion->x;
    launch->vel.y += motion->y;
    launch->vel.z += motion->z;
}

/* Slot 0 is the tank or the right hand, slot 1 the left hand. */
s32 portProjectileArcLaunchNow(s32 slot, PortArcLaunch *launch)
{
    const s32 hand = (slot == 1) ? GUNLEFT : GUNRIGHT;
    PropRecord *playerprop;
    coord3d *prev;

    if (!portProjectileArcPlayerReady()) return FALSE;
    if (!(slot == 0 && arcTankLaunch(launch)) && !arcLauncherLaunch(hand, launch) && !arcThrownLaunch(hand, launch)
        && !arcObjectLaunch(hand, launch))
    {
        return FALSE;
    }

    playerprop = getCurrentPlayerProp();
    prev = getCurrentPlayerPrevPos();
    if (playerprop == NULL || prev == NULL) return FALSE;
    arcAddMotion(launch, playerprop, prev);
    return TRUE;
}
