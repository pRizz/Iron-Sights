/* Projectile Arc (FORK-069): a predicted flight path for the tank's shells,
 * the grenade launcher, the rocket launcher and thrown hand grenades, drawn
 * while one is held, with a ring where it will explode. Off by default
 * ([display] projectile_arc).
 *
 * port_projectile_arc.c, port_projectile_arc_rocket.c and
 * port_projectile_arc_thrown.c are the pure part (the option, the flight
 * models and the dash layout), testable without the ROM; port_projectile_arc_world.c asks the level's collision code about
 * each step and port_projectile_arc_draw.c draws the result. */
#ifndef PORT_PROJECTILE_ARC_H
#define PORT_PROJECTILE_ARC_H

#include <ultra64.h>
#include <bondtypes.h>

#define PORT_PROJECTILE_ARC_DEFAULT 0
/* Arc Impact Crosshair ([display] arc_crosshair): the always-on crosshair
 * marks where a held arcing weapon's shot will land. On by default. */
#define PORT_ARC_CROSSHAIR_DEFAULT 1

/* The game's own flight constants (propobj.c, gun.c, gun_rodata.c). */
#define PORT_ARC_GRAVITY      0.27777779f /* PROP_PROJECTILE_GRAVITY_MODIFIER, units per field^2 */
#define PORT_ARC_GL_SPEED     33.333332f  /* gunSpawnGLGrenade */
#define PORT_ARC_TANK_SPEED   66.666664f  /* g_TankShellSpeed */
#define PORT_ARC_GL_BOUNCE    0.3f        /* g_GLGrenadeLaunchUnk8C: restitution off walls */
#define PORT_ARC_ROCKET_THRUST 1.111111f  /* D_80053DDC: a rocket's acceleration along its aim */
#define PORT_ARC_ROCKET_CADENCE 3.0f      /* PLAYER_LAUNCH_CADENCE: its launch speed is 3 fields of thrust */
#define PORT_ARC_ROCKET_SPEED_CAP2 27777.773f /* ROCKET_SPEED_BREAK_THRESHOLD: thrust stops past this speed^2 */
#define PORT_ARC_ROCKET_POWER_FIELDS 301.0f /* GRENADE_SMOKE_FRAMES (US/JP): then it falls */
#define PORT_ARC_HAND_SPEED   16.666666f  /* generate_player_thrown_grenade: along the aim */
#define PORT_ARC_HAND_LIFT    5.0f        /* ...plus this upwards */
#define PORT_ARC_HAND_BOUNCE  0.3f        /* its Projectile.unk8C */
#define PORT_ARC_HAND_FRICTION 0.13333333f /* its unk94: speed lost a field once it lies flat */
#define PORT_ARC_HAND_HOP     2.2222223f  /* a first bounce hops at least this fast */
#define PORT_ARC_KNIFE_SPEED  25.0f       /* generate_player_thrown_knife: along the aim, plus the lift */
#define PORT_ARC_LIGHT_BOUNCE 0.1f        /* a knife's or mine's Projectile.unk8C */
#define PORT_ARC_SLIDE_FACTOR 0.9f        /* PROJECTILE_FRICTION_FACTOR: speed a settled knife or mine keeps a field */
#ifdef REFRESH_PAL
#define PORT_ARC_HAND_FUSE    200         /* THROWN_ITEM_TIMER_DEFAULT, less the fields the trigger is held */
#else
#define PORT_ARC_HAND_FUSE    240
#endif
/* Fields a settled grenade slides without friction while objSettle turns it
 * flat. The game derives it from the spin it lands with, and the throw spins
 * it randomly (sub_GAME_7F057C14), so it cannot be known in advance. Of ten
 * traced settles (60 and 144 Hz), six turned at the fastest rate the game
 * allows (Projectile.unk64 0.15, 5 fields) and the rest took 6 to 11, so a
 * grenade can slide a few fields past the end of the arc, never short. */
#define PORT_ARC_HAND_TURN_FIELDS 5
/* Measured from traced grenades: one bounces once its origin dips this close
 * over the floor (its spinning model's bottom offset), and rests this high
 * once it lies flat. */
#define PORT_ARC_HAND_CLEARANCE 8.2f
#define PORT_ARC_HAND_REST      8.2f
/* objGetWidth for anything but a PROP_TYPE_WEAPON definition: how far a
 * settled grenade keeps from walls, objects and characters. */
#define PORT_ARC_HAND_WIDTH     10.0f
/* A knife's and a mine's bounce and rest heights and turn, measured as a
 * hand grenade's from traced throws: each rests this high over the floor
 * and bounces from there (so a falling mine is usually caught over the
 * floor before it can cross it and stick, and hops and slides instead);
 * the turn is the median of traced settles (knives 5 to 15 fields, mines 5
 * to 9; the throw's spin is random). */
#define PORT_ARC_KNIFE_CLEARANCE 5.2f
#define PORT_ARC_KNIFE_REST      5.2f
#define PORT_ARC_KNIFE_TURN      8
#define PORT_ARC_MINE_CLEARANCE  5.5f
#define PORT_ARC_MINE_REST       5.5f
#define PORT_ARC_MINE_TURN       6
/* How far over the floor a falling mine is caught, typically, while it
 * spins: the bottom of its turning bounding box. Thrown mines at 60 Hz that
 * fell 12.8 units a tick stuck to the floor about one time in five, so it
 * catches about 10 units of each tick's 12.8 (portArcSimulateThrown). */
#define PORT_ARC_MINE_CATCH      10.0f

#define PORT_ARC_MAX_LEGS     6           /* the launch plus up to five bounces */
#define PORT_ARC_MAX_POINTS   320         /* a rocket's path, one point per field, past its power */

typedef enum PortArcKind {
    PORT_ARC_SHELL,   /* explodes on the first thing it touches */
    PORT_ARC_GRENADE, /* bounces off walls and characters, explodes on landing */
    PORT_ARC_ROCKET,  /* a shell under thrust: dips, climbs back to its aim, speeds up */
    PORT_ARC_THROWN,  /* a hand grenade: bounces, slides, explodes when its fuse runs out */
    PORT_ARC_KNIFE,   /* a throwing knife: strikes the first character, else bounces and slides */
    PORT_ARC_MINE     /* a remote, timed or proximity mine: sticks to the first surface or object */
} PortArcKind;

typedef struct PortArcHit {
    coord3d pos;
    coord3d normal;
    s32 prop;         /* an object or character, rather than the level itself */
    s32 chr;          /* a character or another player */
} PortArcHit;

/* The level, as the flight model sees it. */
typedef struct PortArcWorld {
    void *ctx;
    /* The first surface, object or character on the straight segment from..to. */
    s32 (*segment)(void *ctx, const coord3d *from, const coord3d *to, PortArcHit *hit);
    /* The walkable floor height under pos; 0 when there is none. */
    s32 (*floorBelow)(void *ctx, const coord3d *pos, f32 *y);
    /* A settled object of this width moving from..to over the floor, as
     * objTryMovePropWithCollision moves it: TRUE when the way is clear;
     * otherwise *to becomes where it stops (x and z) and it returns 0.
     * NULL: nothing stops it. */
    s32 (*slide)(void *ctx, const coord3d *from, coord3d *to, f32 width);
} PortArcWorld;

typedef struct PortArcLaunch {
    PortArcKind kind;
    coord3d pos;      /* where the round enters the world */
    coord3d vel;      /* units per field */
    coord3d accel;    /* a rocket's thrust, units per field^2 (Projectile.unk10) */
    f32 maxFields;    /* stop predicting after this long (a hand grenade's fuse) */
    f32 step;         /* fields per collision segment */
    f32 floorClearance; /* the round's height above its origin when it lands */
    /* Thrown objects (portArcSimulateThrown): */
    f32 bounce;         /* restitution, Projectile.unk8C */
    f32 restHeight;     /* the origin's height over the floor once it lies flat */
    s32 turnFields;     /* fields it slides without friction while it turns flat */
    s32 sticks;         /* a mine: attaches to the first surface or object it hits */
    s32 strikes;        /* a knife: its flight ends on the first character it meets
                         * before any bounce (handles_shot_actors) */
    f32 tickStep;       /* fields a game tick moves it (g_GlobalTimerDelta): 1 at a
                         * steady 60 Hz, 60/tickhz above it; 0 reads as 1 */
    f32 slideFriction;  /* 0: it keeps PORT_ARC_SLIDE_FACTOR of its speed a field.
                         * Otherwise a hand grenade's speed lost a field once it lies flat:
                         * objectTick applies unk94 once a field, scaled at
                         * 60 Hz by the tick's step (g_GlobalTimerDelta: 2 on
                         * a frame that took two fields); above 60 Hz one
                         * field's worth (SLIDE_FRICTION_STEP, FORK-070) */
} PortArcLaunch;

/* One free-flight parabola: start + vel*t - (0, g*t*t/2, 0), 0 <= t <= duration. */
typedef struct PortArcLeg {
    coord3d start;
    coord3d vel;
    f32 duration;
} PortArcLeg;

/* A shell or grenade round's path is parabola legs; a rocket's or hand
 * grenade's is a polyline of its position each field (points, no legs). */
typedef struct PortArcPath {
    PortArcLeg legs[PORT_ARC_MAX_LEGS];
    s32 legCount;
    coord3d points[PORT_ARC_MAX_POINTS];
    s32 pointCount;
    s32 impact;       /* 1: the round explodes at end; 0: it flies on past the prediction */
    coord3d end;
    coord3d endNormal;
    s32 endChr;       /* it ends in a character: a knife strikes them, a round
                       * or rocket explodes on them */
    coord3d endDir;   /* a knife's flight there (unit length) */
    /* Where the blast ring lies (port_projectile_arc_world.c): the impact,
     * or for a blast on a character, the floor under them. */
    coord3d ringCentre;
    coord3d ringNormal;
    /* The level, an object or a door stands between the player's eye and
     * the impact, as predicted (port_projectile_arc_world.c). */
    s32 endHidden;
} PortArcPath;

typedef struct PortArcDash {
    coord3d a;
    coord3d b;
    f32 distA;        /* distance along the path to a */
} PortArcDash;

s32 portProjectileArcOptionGet(const char *key, s32 *value);
s32 portProjectileArcOptionSet(const char *key, s32 value);
s32 portProjectileArcEnabled(void);
s32 portArcCrosshairEnabled(void);
void portArcCursorEase(f32 *x, f32 *y, f32 toX, f32 toY, f32 seconds);
void portArcCursorClamp(f32 *x, f32 *y, f32 left, f32 top, f32 width, f32 height, f32 margin);
/* The Arc Impact Crosshair's size for an impact this far from the eye: full
 * up to PORT_ARC_CURSOR_NEAR units, shrinking as perspective would, never
 * below PORT_ARC_CURSOR_MIN_SCALE. */
#define PORT_ARC_CURSOR_NEAR      350.0f
#define PORT_ARC_CURSOR_MIN_SCALE 0.4f
f32 portArcCursorScale(f32 distance);

void portArcPointAt(const PortArcLeg *leg, f32 t, coord3d *out);
void portArcSimulate(const PortArcLaunch *launch, const PortArcWorld *world, PortArcPath *out);
void portArcSimulateRocket(const PortArcLaunch *launch, const PortArcWorld *world, PortArcPath *out);
void portArcSimulateThrown(const PortArcLaunch *launch, const PortArcWorld *world, PortArcPath *out);
s32 portArcDashes(const PortArcPath *path, f32 dashLen, f32 gapLen, f32 phase, PortArcDash *out, s32 max);
f32 portArcFadeIn(f32 dist);
f32 portArcScreenWidth(f32 worldWidth, f32 depth, f32 tanHalfFovY, f32 viewHeight, f32 minPixels);
void portArcKnifeAxis(const coord3d *dir, const coord3d *sight, coord3d *out);

/* The game side (port_projectile_arc_world.c, port_projectile_arc_draw.c). */
/* Each player has an arc per slot: the right hand (or the tank) and the
 * left hand, so dual-wielded weapons each show theirs. */
#define PORT_ARC_SLOTS 2
s32 portProjectileArcLaunchNow(s32 slot, PortArcLaunch *launch);
s32 portProjectileArcPlayerReady(void);
s32 portProjectileArcPredict(const PortArcLaunch *launch, PortArcPath *path);
void portProjectileArcNoteLaunch(void);
void portProjectileArcTick(void);
Gfx *portProjectileArcRender(Gfx *gdl);
const PortArcPath *portProjectileArcLast(s32 slot, PortArcLaunch *launch);
void portProjectileArcFlightFor(PortArcLaunch *launch);
void portProjectileArcNoteTickStep(void);
void portProjectileArcShotFired(ObjectRecord *obj, s32 item);
s32 portProjectileArcDebug(void);
void portProjectileArcLogTick(void);
void portProjectileArcExploded(PropRecord *prop);
void portProjectileArcGrenadeThrown(void);

/* GE_ARC_AUDIT (port_projectile_arc_audit.c), from portAdsTick. */
#define PORT_ARC_AUDIT_AIM 1
#define PORT_ARC_AUDIT_USE 2
s32 portProjectileArcAuditAim(void);
s32 portProjectileArcAuditStrike(void);
s32 portProjectileArcAuditHidden(void);

/* Arc Impact Crosshair (port_projectile_arc_cursor.c). */
s32 portProjectileArcWanted(void);
/* Each impact's x, y, the dash phase of its marching cross when something
 * stands in front of it (negative while in view), and its size for its
 * distance (portArcCursorScale). */
s32 portProjectileArcCrosshairs(f32 xy[PORT_ARC_SLOTS][4]);

#endif
