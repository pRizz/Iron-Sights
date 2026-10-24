/* Retarget the watch's authored forearm onto native pistol/chop bones. */
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <stdint.h>
#include <math.h>
#include "game/dyn.h"
#include "game/file.h"
#include "game/player.h"
#include "game/math_atan2f.h"
#include "gbi_extension.h"
#include "port_wardrobe.h"
#include "port_arm_mesh.h"
extern f32 fabsf(f32);

static s32 paItem, paHand, paFirstPerson;
void portWardrobeBegin(s32 item)
{
    paItem = 0;
    paHand = GUNRIGHT;
    paFirstPerson = FALSE;
    switch (item) {
    case ITEM_FIST: case ITEM_WPPK: case ITEM_WPPKSIL: case ITEM_TT33:
    case ITEM_RUGER: case ITEM_GOLDENGUN: case ITEM_SILVERWPPK: case ITEM_GOLDWPPK:
        paItem = item;
        break;
    default: break;
    }
}

void portWardrobeEnd(void) { paItem = 0; paFirstPerson = FALSE; }


void portWardrobeBeginHand(s32 item, s32 hand)
{
    portWardrobeBegin(item);
    paHand = hand;
    paFirstPerson = TRUE;
}

static s32 paTriangle(const Vtx **cache, s32 a, s32 b, s32 c, s32 *count)
{
    if (a == b || b == c || c == a) return TRUE;
    if (a >= 16 || b >= 16 || c >= 16 || !cache[a] || !cache[b] || !cache[c]) return FALSE;
    ++*count;
    return TRUE;
}

static s32 paDecode(const ModelRoData_DisplayListRecord *data, u32 *matrix)
{
    const Vtx *cache[16] = {0};
    const Gfx *dl = data->Primary;
    s32 count = 0, i;
    *matrix = 0;
    for (i = 0; i < 4096; i++) {
        u32 w0 = dl[i].words.w0, w1 = dl[i].words.w1;
        u32 op = w0 >> 24;
        if (op == (u8)G_ENDDL) return *matrix ? count : -1;
        if (op == G_DL) return -1; /* Unexpected nested geometry: retain native draw. */
        if (op == G_MTX) {
            if ((w1 >> 24) != 3 || (*matrix && *matrix != w1)) return -1;
            *matrix = w1;
        } else if (op == G_VTX) {
            s32 n = (w0 & 0xffff)/sizeof(Vtx), first = (w0 >> 16)&15, j;
            uintptr_t ptr = (uintptr_t)data->BaseAddr + (w1 & 0xffffff);
            uintptr_t begin = (uintptr_t)data->Vertices;
            if ((w1 >> 24) != 5 || !n || first+n > 16 || ptr < begin
                || ptr+n*sizeof(Vtx) > begin+data->numVertices*sizeof(Vtx)) return -1;
            for (j = 0; j < n; j++) cache[first+j] = (const Vtx *)ptr+j;
        } else if (op == (u8)G_TRI1) {
            if (!paTriangle(cache, ((w1>>16)&255)/10, ((w1>>8)&255)/10, (w1&255)/10, &count)) return -1;
        } else if (op == (u8)G_TRI4) {
            s32 j;
            for (j = 0; j < 4; j++) if (!paTriangle(cache, (w1>>(8*j))&15,
                (w1>>(8*j+4))&15, (w0>>(4*j))&15, &count)) return -1;
        }
    }
    return -1;
}

typedef struct {
    coord3d sourceMin, sourceMax, targetMin, targetMax;
    coord3d sourceElbowMin, sourceElbowMax, targetElbowMin, targetElbowMax;
    f32 scale, width, depth, elbowWidth, elbowDepth, watchCenter, extension;
    s32 cuff,bond,ringCount,stitch;
    coord3d ring[32];
    f32 angles[32];
} NativeArmPose;

static s32 paNativeFit(const ModelRoData_DisplayListRecord *data, f32 minZ, f32 wrist,
    s32 cuff, s32 bond, NativeArmPose *pose)
{
    f32 band = (wrist-minZ)*0.04f;
    s32 i, k;
    pose->ringCount=0; pose->stitch=FALSE;
    if (!portArmWristBounds(&pose->sourceMin,&pose->sourceMax)) return FALSE;
    if (!portArmElbowBounds(cuff,bond,&pose->sourceElbowMin,&pose->sourceElbowMax)) return FALSE;
    if (band < 4) band = 4;
    for (k = 0; k < 3; k++) {
        pose->targetMin.f[k] = pose->targetElbowMin.f[k] = 32767;
        pose->targetMax.f[k] = pose->targetElbowMax.f[k] = -32768;
    }
    for (i = 0; i < data->numVertices; i++) {
        const Vtx *vertex = (const Vtx *)data->Vertices+i;
        if (vertex->v.ob[2] <= minZ+band) for (k = 0; k < 3; k++) {
            if (vertex->v.ob[k]<pose->targetElbowMin.f[k]) pose->targetElbowMin.f[k]=vertex->v.ob[k];
            if (vertex->v.ob[k]>pose->targetElbowMax.f[k]) pose->targetElbowMax.f[k]=vertex->v.ob[k];
        }
        if (vertex->v.ob[2] < wrist-band || vertex->v.ob[2] > wrist+band) continue;
        for (k=0;k<pose->ringCount;k++) if (pose->ring[k].x==vertex->v.ob[0]
            && pose->ring[k].y==vertex->v.ob[1] && pose->ring[k].z==vertex->v.ob[2]) break;
        if (k==pose->ringCount) {
            if (pose->ringCount==32) return FALSE;
            pose->ring[pose->ringCount++]=(coord3d){vertex->v.ob[0],vertex->v.ob[1],vertex->v.ob[2]};
        }
        for (k = 0; k < 3; k++) {
            if (vertex->v.ob[k] < pose->targetMin.f[k]) pose->targetMin.f[k] = vertex->v.ob[k];
            if (vertex->v.ob[k] > pose->targetMax.f[k]) pose->targetMax.f[k] = vertex->v.ob[k];
        }
    }
    if (pose->targetMax.x <= pose->targetMin.x || pose->targetMax.y <= pose->targetMin.y) return FALSE;
    if (pose->targetElbowMax.x <= pose->targetElbowMin.x || pose->targetElbowMax.y <= pose->targetElbowMin.y) return FALSE;
    if (pose->ringCount<3) return FALSE;
    for (i=0;i<pose->ringCount;i++) {
        coord3d point=pose->ring[i];
        f32 angle=atan2f(point.y-(pose->targetMin.y+pose->targetMax.y)*0.5f,
            point.x-(pose->targetMin.x+pose->targetMax.x)*0.5f);
        for (k=i;k>0 && pose->angles[k-1]>angle;k--) {
            pose->angles[k]=pose->angles[k-1]; pose->ring[k]=pose->ring[k-1];
        }
        pose->angles[k]=angle; pose->ring[k]=point;
    }
    /* Fit each cross-section axis to the actual native wrist. An arm's bone
     * length cannot determine its wrist width/depth. Keep the tilted seam. */
    /* Native hands have their palm width along Y and their back along -X;
     * the watch model uses Z for width and +Y for the back of the wrist. */
    pose->width = (pose->targetMax.y-pose->targetMin.y)/(pose->sourceMax.z-pose->sourceMin.z);
    pose->depth = (pose->targetMax.x-pose->targetMin.x)/(pose->sourceMax.y-pose->sourceMin.y);
    /* A little room for clothing; avoid magnifying a rolled sleeve into a
     * large block. Fit both the elbow section and its native centerline. */
    pose->elbowWidth = 1.06f*(pose->targetElbowMax.y-pose->targetElbowMin.y)/(pose->sourceElbowMax.z-pose->sourceElbowMin.z);
    pose->elbowDepth = 1.06f*(pose->targetElbowMax.x-pose->targetElbowMin.x)/(pose->sourceElbowMax.y-pose->sourceElbowMin.y);
    if (pose->sourceMin.x <= pose->sourceElbowMin.x) return FALSE;
    pose->scale = (pose->targetMin.z-minZ)/(pose->sourceMin.x-pose->sourceElbowMin.x);
    pose->watchCenter = portArmWatchCenter();
    pose->cuff=cuff; pose->bond=bond;
    /* The pause model ends at the elbow. In first person the forearm must
     * continue below/behind the camera, rather than showing its end cap. */
    pose->extension=paFirstPerson ? 2.5f : 1;
    return TRUE;
}

static f32 paNativeLength(const NativeArmPose *pose, f32 x)
{
    f32 t = (x-pose->sourceMin.x)/(pose->sourceMax.x-pose->sourceMin.x);
    if (t < 0) {
        f32 distance=pose->sourceMin.x-x;
        f32 total=pose->sourceMin.x-pose->sourceElbowMin.x;
        /* Keep the wrist/cuff/watch spacing intact. Extend only the
         * proximal sleeve so its elbow cap stays outside first-person view. */
        if (distance>1000 && total>1000)
            distance=1000+(distance-1000)*(total*pose->extension-1000)/(total-1000);
        return pose->targetMin.z-distance*pose->scale;
    }
    if (t > 1) t = 1;
    return pose->targetMin.z+t*(pose->targetMax.z-pose->targetMin.z);
}

/* Use actual native seam vertices, including the authored Z tilt. Snapping
 * avoids new rounded points on native edges; the edge splitter inserts every
 * skipped corner so the two polygons share exactly the same boundary. */
static f32 paWristAngle(const NativeArmPose *pose, const coord3d *source)
{
    return atan2f((source->z-(pose->sourceMin.z+pose->sourceMax.z)*0.5f)*pose->width,
        -(source->y-(pose->sourceMin.y+pose->sourceMax.y)*0.5f)*pose->depth);
}

static s32 paWristIndex(const NativeArmPose *pose, const coord3d *source)
{
    f32 angle=paWristAngle(pose,source),nearest=1e20f;
    s32 i,index=0;
    for (i=0;i<pose->ringCount;i++) {
        f32 delta=pose->angles[i]-angle;
        if (delta>M_TAU_F*.5f) delta-=M_TAU_F;
        if (delta<-M_TAU_F*.5f) delta+=M_TAU_F;
        if (fabsf(delta)<nearest) { nearest=fabsf(delta); index=i; }
    }
    return index;
}

static void paWristProject(const NativeArmPose *pose, const coord3d *source, coord3d *target)
{
    *target=pose->ring[paWristIndex(pose,source)];
}

static s32 paWristJoin(const coord3d *a, const coord3d *b, PortArmJoinPoint *points, void *context)
{
    const NativeArmPose *pose=context;
    f32 angle,span; s32 i,count=0;
    if (!portArmWristContains(a) || !portArmWristContains(b)) return 0;
    angle=pose->angles[paWristIndex(pose,a)]; span=pose->angles[paWristIndex(pose,b)]-angle;
    if (span>M_TAU_F*.5f) span-=M_TAU_F;
    if (span<-M_TAU_F*.5f) span+=M_TAU_F;
    if (fabsf(span)<0.00001f) return 0;
    for (i=0;i<pose->ringCount;i++) {
        f32 delta=pose->angles[i]-angle,t;
        s32 j;
        if (delta>M_TAU_F*.5f) delta-=M_TAU_F;
        if (delta<-M_TAU_F*.5f) delta+=M_TAU_F;
        t=delta/span;
        if (t<=.00001f || t>=.99999f) continue;
        for (j=count;j>0 && points[j-1].fraction>t;j--) points[j]=points[j-1];
        points[j].target=pose->ring[i]; points[j].fraction=t; ++count;
    }
    return count;
}

static void paNativePoint(const coord3d *source, coord3d *target, s32 part, void *context)
{
    const NativeArmPose *pose = context;
    f32 t = (pose->sourceMin.x-(part==PORT_ARM_WATCH ? pose->watchCenter : source->x))
        /(pose->sourceMin.x-pose->sourceElbowMin.x);
    f32 e,sourceY,sourceZ,targetX,targetY,width,depth;
    coord3d sectionMin,sectionMax;
    f32 sampleX=part==PORT_ARM_WATCH ? pose->watchCenter : source->x;
    if (t<0) t=0;
    if (t>1) t=1;
    e=t*t*(3-2*t);
    sourceY=((1-t)*(pose->sourceMin.y+pose->sourceMax.y)+t*(pose->sourceElbowMin.y+pose->sourceElbowMax.y))*0.5f;
    sourceZ=((1-t)*(pose->sourceMin.z+pose->sourceMax.z)+t*(pose->sourceElbowMin.z+pose->sourceElbowMax.z))*0.5f;
    targetX=((1-t)*(pose->targetMin.x+pose->targetMax.x)+t*(pose->targetElbowMin.x+pose->targetElbowMax.x))*0.5f;
    targetY=((1-t)*(pose->targetMin.y+pose->targetMax.y)+t*(pose->targetElbowMin.y+pose->targetElbowMax.y))*0.5f;
    width=pose->width+(pose->elbowWidth-pose->width)*e;
    depth=pose->depth+(pose->elbowDepth-pose->depth)*e;
    if (sampleX<pose->sourceMin.x && portArmSectionBounds(pose->cuff,pose->bond,sampleX,&sectionMin,&sectionMax)) {
        f32 room=1+0.06f*e;
        sourceY=(sectionMin.y+sectionMax.y)*0.5f; sourceZ=(sectionMin.z+sectionMax.z)*0.5f;
        width=((1-e)*(pose->targetMax.y-pose->targetMin.y)+e*(pose->targetElbowMax.y-pose->targetElbowMin.y))*room/(sectionMax.z-sectionMin.z);
        depth=((1-e)*(pose->targetMax.x-pose->targetMin.x)+e*(pose->targetElbowMax.x-pose->targetElbowMin.x))*room/(sectionMax.y-sectionMin.y);
    }
    /* Retarget the watch's left wrist to the native right-hand anatomical
     * axes. Left weapon bones mirror it back, including the watch's back. */
    target->x = targetX-(source->y-sourceY)*depth;
    target->y = targetY+(source->z-sourceZ)*width;
    if (part == PORT_ARM_WATCH) {
        /* Case/face/clock retain a uniform scale in their X/Z plane, while
         * the strap depth fits the native wrist's elliptical cross-section. */
        target->z = paNativeLength(pose,pose->watchCenter)+(source->x-pose->watchCenter)*width;
    } else target->z = paNativeLength(pose,source->x);
    if (pose->stitch && part==PORT_ARM_FOREARM && portArmWristContains(source)) paWristProject(pose,source,target);
}


s32 portWardrobeRenderNode(ModelRenderData *renderdata, ModelNode *node)
{
    ModelRoData_DisplayListRecord *data = &node->Data->DisplayList;
    f32 minZ = 32767, maxZ = -32768, light;
    s32 i;
    u32 matrix;
    NativeArmPose pose;
    PortArmMesh native;
    Gfx *gdl;
    if (!paItem || renderdata->unk18 || !(renderdata->flags&1) || !data->Primary
        || !data->Vertices || !data->numVertices || data->Secondary) return FALSE;
    for (i = 0; i < data->numVertices; i++) {
        f32 z = data->Vertices[i].coord.z;
        if (z < minZ) minZ = z;
        if (z > maxZ) maxZ = z;
    }
    /* Only proximal forearms extend behind -150. Distal hands/fingers and
     * weapon metal keep their native geometry, grip bones and animations. */
    if (minZ >= -150 || maxZ > 50 || paDecode(data, &matrix) <= 0) return FALSE;
    if (!paNativeFit(data,minZ,paItem == ITEM_FIST ? 50 : 0,
        g_CurrentPlayer->bondtype,fileGetBondForCurrentFolder(),&pose)) return FALSE;
    pose.stitch=paItem==ITEM_FIST;
    light = 0.55f+(g_CurrentPlayer->tileColor.r+g_CurrentPlayer->tileColor.g
        +g_CurrentPlayer->tileColor.b)*(0.45f/(255.0f*3.0f));
    if (!portArmMeshBuildJoined(&native, g_CurrentPlayer->bondtype, fileGetBondForCurrentFolder(),
        PORT_ARM_FOREARM|(paHand == GUNLEFT ? PORT_ARM_WATCH : 0), paNativePoint, &pose, light, 255, pose.stitch ? paWristJoin : NULL)) return FALSE;
    gdl = renderdata->gdl;
    gDPPipeSync(gdl++);
    gSPMatrix(gdl++, matrix, G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH|G_LIGHTING|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR|G_FOG);
    gSPSetGeometryMode(gdl++, G_SHADE|G_SHADING_SMOOTH);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetTextureLOD(gdl++, G_TL_TILE);
    gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    if (renderdata->zbufferenabled) { gDPSetRenderMode(gdl++, G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2); }
    else { gDPSetRenderMode(gdl++, G_RM_AA_OPA_SURF,G_RM_AA_OPA_SURF2); }
    gSPDisplayList(gdl++, osVirtualToPhysical(native.color));
    renderdata->gdl = gdl;
    return TRUE;
}
