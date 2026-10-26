#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <math.h>
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
    { ITEM_SKORPION,      { -0.6f,-0.5f,-1.0f }, { 1.1f, 1.0f, 12.0f } },
    { ITEM_AK47,         { -0.6f,-1.8f,-2.0f }, { 1.1f, 0.5f, 30.0f } },
    { ITEM_UZI,          { -0.6f,-3.5f,-1.0f }, { 1.1f, 0.0f, 12.0f } },
    { ITEM_MP5K,         { -0.6f,-1.5f,-2.0f }, { 1.1f,-1.0f, 23.0f } },
    { ITEM_MP5KSIL,      { -0.6f,-1.5f,-2.0f }, { 1.1f,-1.0f, 23.0f } },
    { ITEM_SPECTRE,      { -0.6f,-2.0f,-6.0f }, { 1.1f,-1.0f, 24.0f } },
    { ITEM_M16,          { -0.6f, 2.0f,-3.0f }, { 1.1f, 5.0f, 36.0f } },
    { ITEM_FNP90,        { -4.5f, 5.0f,-4.0f }, { 4.5f, 5.0f, 11.0f } },
    { ITEM_SHOTGUN,      { -0.6f,-2.0f,-3.0f }, { 1.1f, 0.0f, 33.0f } },
    { ITEM_AUTOSHOT,     { -0.6f,-1.0f,-2.0f }, { 1.1f, 1.0f, 30.0f } },
    { ITEM_SNIPERRIFLE,  { -1.2f, 5.0f,-2.0f }, { 1.7f, 6.0f, 35.0f } },
    { ITEM_LASER,        { -0.6f,-1.0f,-2.0f }, { 1.1f, 1.0f, 15.0f } },
    { ITEM_GRENADELAUNCH,{ -0.6f,-1.0f,-2.0f }, { 1.1f, 0.5f, 25.0f } },
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

typedef struct { coord3d wrist, palm, elbow, axis, back, side; } AdsArmPose;

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
} sAdsArmProfile={.052f,40,35,2.5f};

static void adsGripPoint(const coord3d *source,coord3d *target,s32 part,const PortGripSpace *space,void *context)
{
    const AdsArmPose *pose=context;
    const f32 scale=sAdsArmProfile.handScale;
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
        f32 bulk;
        if (grow>1) grow=1;
        bulk=1+(sAdsArmProfile.forearmBulk-1)*grow*grow*(3-2*grow);
        for (k=0;k<3;k++) {
            f32 start=pose->palm.f[k]+scale*(nearZ*pose->axis.f[k]-near.x*pose->back.f[k]+near.y*pose->side.f[k]);
            target->f[k]=16*(start+(pose->elbow.f[k]-start)*t
                +scale*bulk*(-(source->x-center.x)*pose->back.f[k]+(source->y-center.y)*pose->side.f[k]));
        }
    } else for (k=0;k<3;k++) target->f[k]=16*(pose->palm.f[k]+scale*
        (source->z*pose->axis.f[k]-source->x*pose->back.f[k]+source->y*pose->side.f[k]));
}

static AdsArmPose adsNativePose(const Mtxf *gun, coord3d grip, s32 left, f32 blend)
{
    AdsArmPose pose = {0};
    f32 along = 0;
    s32 k;
    f32 side = left ? 1 : -1;
    pose.palm = grip; pose.palm.x += side*1.25f;
    pose.wrist = pose.palm;
    pose.wrist.x += side*1.1f; pose.wrist.y -= left ? 2.8f : 3.4f; pose.wrist.z -= 2;
    pose.palm = adsHandPoint(gun, &pose.palm); pose.wrist = adsHandPoint(gun, &pose.wrist);
    pose.elbow = (coord3d){-side*(left ? 20.0f : 17.0f), -28.0f-(1-blend)*18.0f, -12.0f};
    pose.axis = (coord3d){pose.wrist.x-pose.elbow.x,pose.wrist.y-pose.elbow.y,pose.wrist.z-pose.elbow.z};
    adsUnit(&pose.axis);
    /* Grip-driven forearm roll: the support palm faces into/under the
     * fore-end, putting the watch on the outside of the wrist. Do not turn
     * the watch towards the camera. The trigger hand's knuckles face out. */
    for (k = 0; k < 3; k++) {
        pose.back.f[k] = gun->m[0][k]*(left ? 0.85f : -1.0f)
            + gun->m[1][k]*(left ? -0.55f : 0.15f);
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
    return pose;
}

static s32 adsNativeGrip(PortArmMesh *mesh, const Mtxf *gun, coord3d grip,
    s32 left, f32 blend, f32 light)
{
    AdsArmPose pose = adsNativePose(gun, grip, left, blend);
    return portWardrobeBuildGrip(mesh,g_CurrentPlayer->bondtype,fileGetBondForCurrentFolder(),left,light,(u8)(blend*255),adsGripPoint,&pose);
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

Gfx *portAdsDrawHands(Gfx *gdl, s32 item, const Mtxf *gun)
{
    coord3d trigger, support;
    Mtxf identity;
    Mtx *matrix;
    f32 blend = portAdsBlend();
    f32 light;
    PortArmMesh native[2];
    s32 i;
    if(portHandGalleryStock()) return gdl;
    if (blend <= 0.0f || !portAdsHandGrips(item, &trigger, &support)) return gdl;
    matrix = dynAllocateMatrix();
    matrix_4x4_set_identity(&identity);
    matrix_scalar_multiply(1.0f/16.0f, identity.m[0]);
    matrix_4x4_f32_to_s32(identity.m, (s32 (*)[4])matrix);
    light = 0.55f + (g_CurrentPlayer->tileColor.r + g_CurrentPlayer->tileColor.g
        + g_CurrentPlayer->tileColor.b) * (0.45f/(255.0f*3.0f));
    if (!adsNativeGrip(&native[0], gun, support, TRUE, blend, light)
        || !adsNativeGrip(&native[1], gun, trigger, FALSE, blend, light)) return gdl;
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_FOG);
    gSPSetGeometryMode(gdl++, G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);
    gSPTexture(gdl++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gSPMatrix(gdl++, osVirtualToPhysical(matrix), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    if (blend < 1) {
        /* Geometry-only prepass prevents overlaps from double-blending. */
        gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_INTER|Z_UPD, G_RM_AA_ZB_XLU_INTER2|Z_UPD);
        gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gdl++, 0, 0, 0, 0, 0, 0);
        for (i = 0; i < 2; i++) gSPDisplayList(gdl++, osVirtualToPhysical(native[i].depth));
        gDPPipeSync(gdl++);
    }
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetTextureLOD(gdl++, G_TL_TILE);
    gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    if (blend == 1) { gDPSetRenderMode(gdl++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2); }
    else { gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_INTER, G_RM_AA_ZB_XLU_INTER2); }
    for (i = 0; i < 2; i++) gSPDisplayList(gdl++, osVirtualToPhysical(native[i].color));
    gDPPipeSync(gdl++);
    return gdl;
}
