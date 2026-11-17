#include <string.h>
#include <ultra64.h>
#include <str.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <math.h>
extern s32 get_itemtype_in_hand(GUNHAND hand); /* gun.c */
extern float sqrtf(float);
extern float sinf(float);
extern float cosf(float);
#include "port_ads.h"
#include "game/dyn.h"
#include "game/player.h"
#include "game/bondview.h"
#include "game/matrixmath.h"
#include "game/file.h"
#include "game/othermodemicrocode.h"
#include "game/lv.h"
#include "gbi_extension.h"

#include "port_wardrobe.h"
#include "port_arm_mesh.h"
static coord3d adsHandLerp(coord3d a, coord3d b, f32 t)
{
    coord3d p = {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t};
    return p;
}

/* Grip centres in weapon-local view units (the original models use ten times these). */
static const struct {
    s32 item;
    coord3d trigger;
    coord3d support;
} sAdsGrips[] = {
    /* Set back so the trigger fist sits round the grip instead of through it. */
    { ITEM_SKORPION,      { -0.6f,-0.5f,-2.5f }, { 1.1f, 1.0f, 12.0f } },
    { ITEM_AK47,         { -0.6f,-1.8f,-2.0f }, { 1.1f, 0.5f, 30.0f } },
    { ITEM_UZI,          { -0.6f,-3.5f,-1.0f }, { 1.1f, 0.0f, 12.0f } },
    { ITEM_MP5K,         { -0.6f,-1.5f,-2.0f }, { 1.1f,-1.0f, 23.0f } },
    { ITEM_MP5KSIL,      { -0.6f,-1.5f,-2.0f }, { 1.1f,-1.0f, 23.0f } },
    { ITEM_SPECTRE,      { -0.6f,-2.0f,-6.0f }, { 1.1f,-1.0f, 24.0f } },
    { ITEM_M16,          { -0.6f, 2.0f,-3.0f }, { 1.1f, 5.0f, 36.0f } },
    { ITEM_FNP90,        { -4.5f, 5.0f,-4.0f }, { 4.5f, 5.0f, 11.0f } },
    /* Shouldered guns (adsShoulderedGrip): the trigger fist is centred on the
     * pistol grip, its fingers round the front and the thumb on the left;
     * set back so the palm and web cover the grip's rear instead of the grip
     * showing through the fist. */
    { ITEM_SHOTGUN,      { 1.25f,-3.0f,-5.0f }, { 1.1f, 0.0f, 33.0f } },
    { ITEM_AUTOSHOT,     { 1.25f, 1.0f,-5.0f }, { 1.1f, 1.0f, 30.0f } },
    /* The support hand holds the wooden fore-end, back and below the muzzle
     * (owner review 2026-11-11, FORK-075; previously {1.7,6,35}). The scope
     * hides both hands once raised, so only the hip and the raise show it. */
    { ITEM_SNIPERRIFLE,  { -1.2f, 5.0f,-2.0f }, { 0.1f, 1.0f, 19.0f } },
    { ITEM_LASER,        { -0.6f,-1.0f,-2.0f }, { 1.1f, 1.0f, 15.0f } },
    /* The support palm holds the underside of the body between the trigger
     * guard and the cylinder, instead of reaching through the chambers. */
    { ITEM_GRENADELAUNCH,{ 1.25f,-2.5f,-3.5f }, { 1.1f,-3.0f, 10.0f } },
    { ITEM_ROCKETLAUNCH, { -0.6f, 0.0f,-1.0f }, { 1.1f, 9.0f, 25.0f } },
};

s32 portAdsHandGrips(s32 item, coord3d *trigger, coord3d *support)
{
    s32 i;
    for (i = 0; i < sizeof(sAdsGrips) / sizeof(sAdsGrips[0]); i++) {
        if (sAdsGrips[i].item == item) {
            *trigger = sAdsGrips[i].trigger;
            *support = sAdsGrips[i].support;
            return TRUE;
        }
    }
    return FALSE;
}

static coord3d adsHandPoint(const Mtxf *gun, const coord3d *p)
{
    coord3d out;
    s32 i;
    for (i = 0; i < 3; i++) {
        out.f[i] = p->x * gun->m[0][i] + p->y * gun->m[1][i]
            + p->z * gun->m[2][i] + gun->m[3][i];
    }
    return out;
}

static void adsHandProjection(const Mtxf *scene, Mtxf *projection)
{
    s32 i;
    *projection = *scene;
    /* Reserve near depth without changing the selected gun pass's XY projection. */
    for (i = 0; i < 4; i++) {
        projection->m[i][2] = scene->m[i][2]*0.01f - scene->m[i][3]*0.99f;
    }
}

typedef struct {
    coord3d wrist, palm, elbow, axis, back, side;
    f32 scale;
    f32 girth;                    /* the forearm behind the cuff is this many times fuller */
    f32 wristDepth, wristWidth;   /* the wrist, cuff and watch scale: palm to back, and across */
    s32 untwist;                  /* a tilted hand: the forearm eases to the elbow frame */
    coord3d elbowBack, elbowSide; /* the forearm's cross-section frame at the elbow */
} AdsArmPose;

static void adsUnit(coord3d *v)
{
    f32 length = sqrtf(v->x*v->x+v->y*v->y+v->z*v->z);
    if (length > 0.0001f) { v->x /= length; v->y /= length; v->z /= length; }
}

/* Rifle/SMG/shotgun ADS shaping only; native pistol/chop arms and icon
 * generation do not use this profile. The authored closed PP7 hand remains
 * uniformly scaled with its wrist/cuff/watch: only the proximal forearm
 * stretches and widens independently behind the cuff. */
static const struct {
    f32 handScale,protectedWrist,bulkRamp,forearmBulk;
    f32 wristRamp; /* a fit's wrist scale eases in over this length of the hand's heel */
} sAdsArmProfile={.065f,40,35,2.5f,45}; /* 0.052, then 0.058, before the owner's 2026-11-11 requests (FORK-075) */

/* Owner-requested 2026-11-02: these guns' ADS arms read thin beside their
 * bodies, so their whole generated hand/wrist/forearm grows around the grip. */
static f32 adsArmScale(s32 item)
{
    switch (item) {
    case ITEM_SKORPION: case ITEM_UZI: case ITEM_SHOTGUN: case ITEM_GRENADELAUNCH: return 1.08f;
    default: return 1.0f;
    }
}

/* The canted rocket launcher (portAdsWeaponRoll) keeps its support hand under
 * the tube relative to the ground: rotate that hand's gun basis back about the
 * tube axis (weapon-local, from its muzzle node) instead of twisting with it. */
static const coord3d sAdsRocketTubeAxis = { -0.3f, 12.75f, 0.0f };

static void adsCounterRoll(Mtxf *out, const Mtxf *gun, coord3d axis, f32 angle)
{
    const coord3d pivot = adsHandPoint(gun, &axis);
    const f32 c = cosf(angle), s = sinf(angle);
    s32 k;
    *out = *gun;
    for (k = 0; k < 4; k++) {
        const f32 x = k < 3 ? gun->m[k][0] : gun->m[3][0] - pivot.x;
        const f32 y = k < 3 ? gun->m[k][1] : gun->m[3][1] - pivot.y;
        out->m[k][0] = x * c - y * s + (k < 3 ? 0 : pivot.x);
        out->m[k][1] = x * s + y * c + (k < 3 ? 0 : pivot.y);
    }
}

/* The RC-P90's trigger grip sits under its body, where a generated right hand
 * reads badly in ADS; the owner prefers showing only the support hand. */
static s32 adsGripHands(s32 item)
{
    return item == ITEM_FNP90 ? 1 : 2;
}

/* A tilted hand's forearm reaches its elbow frame this many times sooner than
 * the elbow itself: by mid-forearm, where the hip camera's view of it ends. */
#define ADS_UNTWIST_RATE 2.0f

/* A fit's wrist scale: the wrist, cuff and watch widen about the arm's centre
 * line, easing in along the heel of the hand, whose grip keeps its size. */
static coord3d adsWristPoint(const coord3d *source, const PortGripSpace *space, const AdsArmPose *pose)
{
    coord3d point = *source, center;
    f32 ease = (space->wrist.z + sAdsArmProfile.wristRamp - source->z)/sAdsArmProfile.wristRamp;
    if (ease <= 0 || (pose->wristDepth == 1.0f && pose->wristWidth == 1.0f)) return point;
    if (ease > 1) ease = 1;
    ease = ease*ease*(3-2*ease);
    center = adsHandLerp(space->wrist, space->elbow, (space->wrist.z-source->z)/(space->wrist.z-space->elbow.z));
    point.x = center.x + (point.x-center.x)*(1+(pose->wristDepth-1)*ease);
    point.y = center.y + (point.y-center.y)*(1+(pose->wristWidth-1)*ease);
    return point;
}

static void adsGripPoint(const coord3d *source,coord3d *target,s32 part,const PortGripSpace *space,void *context)
{
    const AdsArmPose *pose=context;
    const f32 scale=pose->scale;
    f32 nearZ=space->wrist.z-sAdsArmProfile.protectedWrist;
    s32 k;
    if (part==PORT_ARM_FOREARM && source->z<nearZ && nearZ>space->elbow.z) {
        f32 t=(nearZ-source->z)/(nearZ-space->elbow.z);
        f32 nearT=(space->wrist.z-nearZ)/(space->wrist.z-space->elbow.z);
        f32 nativeT=(space->wrist.z-source->z)/(space->wrist.z-space->elbow.z);
        coord3d near=adsHandLerp(space->wrist,space->elbow,nearT);
        coord3d center=adsHandLerp(space->wrist,space->elbow,nativeT);
        /* Restore forearm volume without scaling the grip, wrist seam or
         * watch. Smoothly widen both radial axes behind the protected cuff. */
        f32 grow=(nearZ-source->z)/sAdsArmProfile.bulkRamp;
        f32 bulk, deep, wide;
        coord3d back=pose->back, side=pose->side;
        if (grow>1) grow=1;
        grow=grow*grow*(3-2*grow);
        bulk=(1+(sAdsArmProfile.forearmBulk-1)*grow)*pose->girth;
        /* The sleeve leaves the cuff at the wrist's scale; both are exactly 1 without one. */
        deep=1+(pose->wristDepth/pose->girth-1)*(1-grow);
        wide=1+(pose->wristWidth/pose->girth-1)*(1-grow);
        if (pose->untwist) {
            /* The wrist's roll and bend fade along the forearm, as an arm's do,
             * so the sleeve is not sheared thin by the hand's frame. */
            f32 ease=t*ADS_UNTWIST_RATE;
            if (ease>1) ease=1;
            back=adsHandLerp(pose->back,pose->elbowBack,ease); adsUnit(&back);
            side=adsHandLerp(pose->side,pose->elbowSide,ease); adsUnit(&side);
        }
        for (k=0;k<3;k++) {
            f32 start=pose->palm.f[k]+scale*(nearZ*pose->axis.f[k]-near.x*pose->back.f[k]+near.y*pose->side.f[k]);
            target->f[k]=16*(start+(pose->elbow.f[k]-start)*t
                +scale*bulk*(-(source->x-center.x)*deep*back.f[k]+(source->y-center.y)*wide*side.f[k]));
        }
    } else {
        const coord3d point=adsWristPoint(source,space,pose);
        for (k=0;k<3;k++) target->f[k]=16*(pose->palm.f[k]+scale*
            (point.z*pose->axis.f[k]-point.x*pose->back.f[k]+point.y*pose->side.f[k]));
    }
}

/* Stocked guns whose trigger hand shows on a slanted pistol grip. The default
 * low elbow tilts that fist off the grip; a higher elbow beside the shoulder
 * keeps the forearm level so the closed hand wraps it. */
static s32 adsShoulderedGrip(s32 item)
{
    return item == ITEM_SHOTGUN || item == ITEM_AUTOSHOT || item == ITEM_GRENADELAUNCH;
}

/* Camera-space elbow targets: they drop 18 units as the ADS arms fade out. */
static coord3d adsElbow(s32 left, f32 blend, s32 shouldered)
{
    if (shouldered && !left) return (coord3d){14.0f, -12.0f-(1-blend)*18.0f, 6.0f};
    return (coord3d){left ? -20.0f : 17.0f, -28.0f-(1-blend)*18.0f, -12.0f};
}

/* The hand's back before it is squared to the forearm: the support hand palm
 * up with a slight outward cant plus the fit's, the trigger hand knuckles out. */
static f32 adsBackTerm(const Mtxf *gun, s32 left, f32 cant, s32 k)
{
    return gun->m[0][k]*(left ? 0.2f + cant : -1.0f) + gun->m[1][k]*(left ? -1.0f : 0.15f);
}

/* The elbow end of a tilted support hand's forearm keeps the plain palm-up
 * frame about the untilted forearm: no fit roll and no wrist bend. */
static void adsElbowFrame(AdsArmPose *pose, const Mtxf *gun, coord3d forearm)
{
    coord3d back;
    f32 along = 0;
    s32 k;
    for (k = 0; k < 3; k++) { back.f[k] = adsBackTerm(gun, TRUE, 0.0f, k); along += back.f[k]*forearm.f[k]; }
    for (k = 0; k < 3; k++) back.f[k] -= along*forearm.f[k];
    if (back.x*back.x+back.y*back.y+back.z*back.z < 0.001f) return;
    adsUnit(&back);
    pose->elbowBack = back;
    pose->elbowSide = (coord3d){forearm.y*back.z-forearm.z*back.y,
        forearm.z*back.x-forearm.x*back.z,forearm.x*back.y-forearm.y*back.x};
    pose->untwist = TRUE;
}

/* cant adds to the support hand's outward roll and tilt leans that hand
 * towards the gun's up (Hip-Fire Arms); both 0 in ADS. */
static AdsArmPose adsArmPose(const Mtxf *gun, coord3d grip, s32 left, f32 scale, coord3d elbow, f32 cant,
    f32 tilt)
{
    AdsArmPose pose = {0};
    coord3d forearm;
    pose.scale = sAdsArmProfile.handScale*scale;
    pose.girth = pose.wristDepth = pose.wristWidth = 1.0f;
    f32 along = 0;
    s32 k;
    f32 side = left ? 1 : -1;
    pose.palm = grip; pose.palm.x += side*1.25f;
    /* The palm-up support hand rises slightly to meet the fore-end's underside. */
    if (left) pose.palm.y += 0.4f;
    pose.wrist = pose.palm;
    pose.wrist.x += side*1.1f; pose.wrist.y -= left ? 2.8f : 3.4f; pose.wrist.z -= 2;
    pose.palm = adsHandPoint(gun, &pose.palm); pose.wrist = adsHandPoint(gun, &pose.wrist);
    pose.elbow = elbow;
    pose.axis = (coord3d){pose.wrist.x-pose.elbow.x,pose.wrist.y-pose.elbow.y,pose.wrist.z-pose.elbow.z};
    adsUnit(&pose.axis);
    forearm = pose.axis;
    /* A tilted hand bends at the wrist: its thumb, which lies along this axis,
     * runs up the fore-end while the forearm still ends at the same elbow. */
    if (tilt != 0.0f) {
        for (k = 0; k < 3; k++) pose.axis.f[k] += tilt*gun->m[1][k];
        adsUnit(&pose.axis);
    }
    /* Grip-driven forearm roll: the support hand cradles the fore-end palm up
     * (owner request), its back facing the ground with a slight outward cant,
     * which rolls the watch under the wrist and away from the camera. Never
     * turn the watch towards the camera. The trigger hand's knuckles face out. */
    for (k = 0; k < 3; k++) {
        pose.back.f[k] = adsBackTerm(gun, left, cant, k);
        along += pose.back.f[k]*pose.axis.f[k];
    }
    for (k = 0; k < 3; k++) pose.back.f[k] -= along*pose.axis.f[k];
    if (pose.back.x*pose.back.x+pose.back.y*pose.back.y+pose.back.z*pose.back.z < 0.001f) {
        pose.back = pose.axis.y > -0.9f && pose.axis.y < 0.9f ? (coord3d){0,1,0} : (coord3d){1,0,0};
        along = pose.back.x*pose.axis.x+pose.back.y*pose.axis.y;
        for (k = 0; k < 3; k++) pose.back.f[k] -= along*pose.axis.f[k];
    }
    adsUnit(&pose.back);
    pose.side = (coord3d){pose.axis.y*pose.back.z-pose.axis.z*pose.back.y,
        pose.axis.z*pose.back.x-pose.axis.x*pose.back.z,pose.axis.x*pose.back.y-pose.axis.y*pose.back.x};
    if (!left) { pose.side.x *= -1; pose.side.y *= -1; pose.side.z *= -1; }
    if (left && tilt != 0.0f) adsElbowFrame(&pose, gun, forearm);
    return pose;
}

static AdsArmPose adsNativePose(const Mtxf *gun, coord3d grip, s32 left, f32 blend, f32 scale,
    s32 shouldered)
{
    return adsArmPose(gun, grip, left, scale, adsElbow(left, blend, shouldered), 0.0f, 0.0f);
}

/* Hip-Fire Arms (FORK-075): the settled ADS gun matrix, built as the hand
 * gallery builds it in gunfire.c (view-facing basis, ADS pitch and roll, sight
 * pose). FALSE for a gun without an ADS pose. */
static s32 adsSettledGun(s32 item, const coord3d *muzzle, Mtxf *out)
{
    Mtxf turn, aimed;
    coord3d pose;
    if (!portAdsWeaponPose(item, muzzle, &pose)) return FALSE;
    matrix_4x4_set_identity(out);
    out->m[0][0] = out->m[2][2] = -1;
    matrix_4x4_set_rotation_around_x(portAdsWeaponPitch(item), &turn);
    matrix_4x4_multiply_homogeneous(out, &turn, &aimed);
    *out = aimed;
    if (portAdsWeaponRoll(item) != 0.0f) {
        matrix_4x4_set_rotation_around_z(portAdsWeaponRoll(item), &turn);
        matrix_4x4_multiply_homogeneous(&turn, out, &aimed);
        *out = aimed;
    }
    matrix_4x4_set_position(&pose, out);
    return TRUE;
}

/* Carry a camera-space point fixed to the settled gun onto the current gun. */
static coord3d adsGunCarried(const Mtxf *gun, const Mtxf *settled, coord3d point)
{
    coord3d local;
    s32 j;
    for (j = 0; j < 3; j++) {
        local.f[j] = (point.x-settled->m[3][0])*settled->m[j][0]
            + (point.y-settled->m[3][1])*settled->m[j][1] + (point.z-settled->m[3][2])*settled->m[j][2];
    }
    return adsHandPoint(gun, &local);
}

/* Per-gun hip adjustments, all in weapon-local units (x is the gun's left,
 * y up, z towards the muzzle), faded out as the gun rises to ADS. The hip
 * camera sees the support hand from above and beside the gun, where the ADS
 * palm-up cradle floats beside the fore-end or sinks into the body. */
typedef struct {
    s32 item;
    coord3d support; /* added to the support grip */
    f32 cant;        /* added outward roll of the support hand */
    coord3d elbow;   /* added to the carried support elbow */
    s32 behind;      /* the support hand draws before the gun, which covers it */
    f32 tilt;        /* the support hand leans this far towards the gun's up */
    f32 girth;       /* the support forearm is this many times fuller (0: as it is) */
    f32 wristDepth;  /* the wrist, cuff and watch scale from palm to back (0: as it is) */
    f32 wristWidth;  /* and across the wrist (0: as it is) */
} AdsHipGrip;

static const AdsHipGrip sAdsHipGrips[] = {
    /* Owner reviews, 2026-11-11. The Klobb and ZMG wrap the fore-end's side,
     * rolled out, without clipping or floating. */
    { ITEM_SKORPION,     {  0.0f,  0.5f, 0 }, 0.8f, { 0, 0, 0 } },
    { ITEM_UZI,          { -0.75f, 1.5f, 0 }, 1.2f, { 0, 0, 0 } },
    /* Against the KF7's wood and drawn behind the gun: the closed finger tips
     * pass through the fore-end, and the gun now covers them. */
    { ITEM_AK47,         {  0.2f,  0.0f, 0 }, 0.2f, { 0, 0, 0 }, TRUE },
    /* Held from underneath, palm up, instead of upright beside the gun. */
    { ITEM_MP5K,         { -1.5f, -0.5f, 0 }, 0.0f, { 0, 0, 0 } },
    { ITEM_MP5KSIL,      { -1.5f, -0.5f, 0 }, 0.0f, { 0, 0, 0 } },
    /* Owner's choices, 2026-11-12: tilted so the thumb runs up the hand guard
     * instead of along the barrel, then tucked under the guard and rolled out
     * a little so the guard hides the index finger while the wrist keeps its
     * broad side to the camera, with a fuller forearm (first { 0, -2.5, 0 },
     * cant -0.3, untilted; then { 0.75, -1.5, 0 }, cant 0.2, tilt 1.0; then
     * { 0, -0.75, 0 }, where a fingertip showed through the guard's top).
     * 2026-11-14: a thicker, wider wrist, the hand 0.5 lower, then 0.25 to
     * the left to show a little more thumb (0.75 and over bring the index
     * finger through the guard). */
    { ITEM_M16,          {  0.25f, -1.25f, 0 }, 0.7f, { 0, 0, 0 }, FALSE, 1.0f, 1.3f, 1.5f, 1.25f },
    { ITEM_FNP90,        { -3.5f, -2.0f, 0 }, 0.0f, { 0, 0, 0 } },
    { ITEM_SHOTGUN,      { -1.5f, -0.5f, 0 }, 0.0f, { 0, 0, 0 } },
    /* Back from the muzzle. */
    { ITEM_SPECTRE,      {  0.0f,  0.0f, -6.0f }, 0.0f, { 0, 0, 0 } },
    /* Elbow bent below the tube, not stretched to the lower left corner. */
    { ITEM_ROCKETLAUNCH, { 0, 0, 0 }, 0.0f, { -15.0f, -15.0f, -5.0f } },
};

static AdsHipGrip adsHipGrip(s32 item)
{
    AdsHipGrip hip = { item };
    s32 i;
    for (i = 0; i < sizeof(sAdsHipGrips)/sizeof(sAdsHipGrips[0]); i++) {
        if (sAdsHipGrips[i].item == item) hip = sAdsHipGrips[i];
    }
    return hip;
}

/* At the hip the settled ADS arm rides rigidly with the gun, so its shape
 * relative to the weapon is the approved ADS one through hip fire, raises,
 * lowers and reloads; aiming eases the elbow onto its camera-space target.
 * The per-gun hip adjustments fade out over the same raise. */
static AdsArmPose adsHipPose(const Mtxf *handGun, const Mtxf *gun, const Mtxf *settled,
    coord3d grip, s32 left, f32 blend, f32 scale, s32 shouldered, const AdsHipGrip *hip)
{
    const coord3d ads = adsElbow(left, 1.0f, shouldered);
    const f32 rest = 1.0f - blend;
    AdsArmPose pose;
    coord3d carried = adsGunCarried(gun, settled, ads);
    if (left) {
        const coord3d zero = {0, 0, 0};
        const coord3d shift = adsHandLerp(zero, hip->elbow, rest);
        coord3d moved = adsHandPoint(gun, &shift), origin = adsHandPoint(gun, &zero);
        carried.x += moved.x-origin.x; carried.y += moved.y-origin.y; carried.z += moved.z-origin.z;
        grip.x += hip->support.x*rest; grip.y += hip->support.y*rest; grip.z += hip->support.z*rest;
    }
    pose = adsArmPose(handGun, grip, left, scale, adsHandLerp(carried, ads, blend),
        left ? hip->cant*rest : 0.0f, left ? hip->tilt*rest : 0.0f);
    if (left && hip->girth > 0.0f) pose.girth = 1.0f + (hip->girth-1.0f)*rest;
    if (left && hip->wristDepth > 0.0f) pose.wristDepth = 1.0f + (hip->wristDepth-1.0f)*rest;
    if (left && hip->wristWidth > 0.0f) pose.wristWidth = 1.0f + (hip->wristWidth-1.0f)*rest;
    return pose;
}

/* Which hands a draw pass builds (0 support, 1 trigger). A support hand
 * marked behind draws alone before the gun; the pass after the gun skips it. */
static s32 adsHandInPass(s32 behind, s32 beforeGun, s32 hand)
{
    if (!behind) return !beforeGun;
    return beforeGun ? hand == 0 : hand != 0;
}

static s32 adsNativeGrip(PortArmMesh *mesh, const AdsArmPose *pose, s32 left, f32 alpha, f32 light)
{
    return portWardrobeBuildGrip(mesh,g_CurrentPlayer->bondtype,fileGetBondForCurrentFolder(),left,light,
        (u8)(alpha*255),adsGripPoint,(void *)pose);
}

static s32 sHipArms = PORT_HIP_ARMS_DEFAULT;

s32 portAdsHandsOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "hip_arms") != 0) return FALSE;
    *value = sHipArms;
    return TRUE;
}

s32 portAdsHandsOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "hip_arms") != 0) return FALSE;
    sHipArms = value != 0;
    return TRUE;
}

/* The gallery's stock cells keep original models. Dual wielding keeps the
 * right gun's trigger hand; its support hand slides away (sAdsSupportHold). */
static s32 adsHipArmsOn(void)
{
    return (sHipArms || portHandGalleryHipArms()) && !portHandGalleryStock();
}

/* Dual wielding (owner's choice, 2026-11-11): when the second gun comes up the
 * support arm slides down out of view, as if letting go to take it, while the
 * right hand keeps its grip; it slides back when the left gun goes away. 1 =
 * on the gun, 0 = lowered. Per player; 300 ms either way at any tick rate. */
static f32 sAdsSupportHold[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
#define ADS_SUPPORT_SLIDE_TICKS 18.0f

f32 portAdsSupportHoldStep(f32 hold, s32 dual, f32 ticks)
{
    hold += (dual ? -ticks : ticks) / ADS_SUPPORT_SLIDE_TICKS;
    return hold < 0.0f ? 0.0f : hold > 1.0f ? 1.0f : hold;
}

void portAdsHandsTick(void)
{
    f32 *hold = &sAdsSupportHold[get_cur_playernum() & 3];
    *hold = portAdsSupportHoldStep(*hold, get_itemtype_in_hand(GUNLEFT) != 0, g_GlobalTimerDelta);
}

/* Down, a little left and towards the camera: below the frame at the hip. */
static void adsSupportLowered(AdsArmPose *pose, f32 hold)
{
    const f32 s = hold*hold*(3.0f-2.0f*hold), away = 1.0f-s;
    const coord3d drop = { -4.0f*away, -30.0f*away, 6.0f*away };
    pose->palm.x += drop.x; pose->palm.y += drop.y; pose->palm.z += drop.z;
    pose->wrist.x += drop.x; pose->wrist.y += drop.y; pose->wrist.z += drop.z;
    pose->elbow.x += drop.x; pose->elbow.y += drop.y; pose->elbow.z += drop.z;
}

f32 portAdsHandAlpha(s32 item)
{
    coord3d trigger, support;
    const s32 grips = portAdsHandGrips(item, &trigger, &support);
    /* A lowered support arm on a support-only gun (the RC-P90) leaves nothing. */
    if (grips && adsGripHands(item) == 1 && sAdsSupportHold[get_cur_playernum() & 3] <= 0.0f) return 0.0f;
    return portAdsHandAlphaFor(grips, adsHipArmsOn(), portAdsBlend());
}

Gfx *portAdsBeginHandLayer(Gfx *gdl, const Mtxf *gunProjection)
{
    Mtxf projection;
    Mtx *matrix = dynAllocateMatrix();
    adsHandProjection(gunProjection, &projection);
    matrix_4x4_f32_to_s32(projection.m, (s32 (*)[4])matrix);
    gSPMatrix(gdl++, osVirtualToPhysical(matrix), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    return gdl;
}

/* lvlRender enables G_ZBUFFER once per frame (dlZBufferGeometry) for every
 * split-screen view; clearing it here left later views without depth tests. */
Gfx *portAdsEndHandLayer(Gfx *gdl, Mtx *sceneProjection)
{
    gSPMatrix(gdl++, osVirtualToPhysical(sceneProjection), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    return gdl;
}

/* The stock gun draws unbuffered, so a later coplanar overlay (the rocket
 * launcher's lettering) always shows. In the compressed hand layer a depth
 * test lost those overlays to the surface under them and flickered.
 * G_NOOP 0x64 turns the compare off (painter's order) but keeps the depth
 * writes, so the arms drawn after the gun still hide behind it. */
Gfx *portAdsGunPainterTag(Gfx *gdl, s32 on)
{
    gDPNoOpTag(gdl++, 0x64000000u | (on ? 1u : 0u));
    return gdl;
}

static Gfx *adsDrawHands(Gfx *gdl, s32 item, const Mtxf *gun, s32 beforeGun)
{
    coord3d trigger, support;
    Mtxf identity;
    Mtx *matrix;
    f32 blend = portAdsBlend();
    f32 alpha = portAdsHandAlpha(item);
    f32 light;
    PortArmMesh native[2];
    AdsArmPose pose[2];
    Mtxf supportGun, settled;
    AdsHipGrip hipGrip;
    const ModelFileHeader *header = &g_CurrentPlayer->copy_of_body_obj_header[GUNRIGHT];
    s32 i, hands, hip, built, behind;
    f32 hold;
    if(portHandGalleryStock()) return gdl;
    if (alpha <= 0.0f || !portAdsHandGrips(item, &trigger, &support)) return gdl;
    /* Hip-Fire Arms below a settled ADS; a settled ADS keeps its own pose. */
    hip = alpha == 1.0f && blend < 1.0f && header->Switches != NULL && header->Switches[3] != NULL
        && adsSettledGun(item, (const coord3d *)header->Switches[3]->Data, &settled);
    if (blend <= 0.0f && !hip) return gdl;
    hipGrip = adsHipGrip(item);
    behind = hip && hipGrip.behind;
    if (beforeGun && !behind) return gdl;
    matrix = dynAllocateMatrix();
    matrix_4x4_set_identity(&identity);
    matrix_scalar_multiply(1.0f/16.0f, identity.m[0]);
    matrix_4x4_f32_to_s32(identity.m, (s32 (*)[4])matrix);
    light = 0.55f + (g_CurrentPlayer->tileColor.r + g_CurrentPlayer->tileColor.g
        + g_CurrentPlayer->tileColor.b) * (0.45f/(255.0f*3.0f));
    hands = adsGripHands(item);
    supportGun = *gun;
    if (item == ITEM_ROCKETLAUNCH) {
        coord3d axis = sAdsRocketTubeAxis;
        axis.z = support.z;
        adsCounterRoll(&supportGun, gun, axis, -portAdsWeaponRoll(item) * blend);
    }
    hold = sAdsSupportHold[get_cur_playernum() & 3];
    for (i = 0, built = 0; i < hands; i++) {
        const Mtxf *handGun = i == 0 ? &supportGun : gun;
        const coord3d grip = i == 0 ? support : trigger;
        if (i == 0 && hold <= 0.0f) continue; /* lowered out of view (dual wield) */
        if (!adsHandInPass(behind, beforeGun, i)) continue;
        pose[i] = hip ? adsHipPose(handGun, gun, &settled, grip, i == 0, blend, adsArmScale(item), adsShoulderedGrip(item), &hipGrip)
            : adsNativePose(handGun, grip, i == 0, blend, adsArmScale(item), adsShoulderedGrip(item));
        if (i == 0 && hold < 1.0f) adsSupportLowered(&pose[i], hold);
        if (!adsNativeGrip(&native[built++], &pose[i], i == 0, hip ? 1.0f : blend, light)) return gdl;
    }
    hands = built;
    if (!hands) return gdl;
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_FOG);
    gSPSetGeometryMode(gdl++, G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);
    gSPTexture(gdl++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gSPMatrix(gdl++, osVirtualToPhysical(matrix), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    if (hip) blend = 1.0f; /* opaque from here on */
    if (blend < 1) {
        /* Geometry-only prepass prevents overlaps from double-blending. */
        gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_INTER|Z_UPD, G_RM_AA_ZB_XLU_INTER2|Z_UPD);
        gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gdl++, 0, 0, 0, 0, 0, 0);
        for (i = 0; i < hands; i++) gSPDisplayList(gdl++, osVirtualToPhysical(native[i].depth));
        gDPPipeSync(gdl++);
    }
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetTextureLOD(gdl++, G_TL_TILE);
    gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    if (blend == 1) { gDPSetRenderMode(gdl++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2); }
    else { gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_INTER, G_RM_AA_ZB_XLU_INTER2); }
    for (i = 0; i < hands; i++) gSPDisplayList(gdl++, osVirtualToPhysical(native[i].color));
    gDPPipeSync(gdl++);
    return gdl;
}

Gfx *portAdsDrawHands(Gfx *gdl, s32 item, const Mtxf *gun)
{
    return adsDrawHands(gdl, item, gun, FALSE);
}

/* Before the painter's-order gun: a hip support hand its fit marks behind. */
Gfx *portAdsDrawHandsBehindGun(Gfx *gdl, s32 item, const Mtxf *gun)
{
    return adsDrawHands(gdl, item, gun, TRUE);
}
