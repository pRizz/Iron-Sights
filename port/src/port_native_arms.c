/* Retarget the watch's authored forearm onto native pistol/chop bones. */
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <stdint.h>
#include <math.h>
#include "game/dyn.h"
#include "game/file.h"
#include "game/player.h"
#include "game/model.h"
#include "game/math_atan2f.h"
#include "gbi_extension.h"
#include "port_wardrobe.h"
#include "port_arm_mesh.h"
#include "port_ads.h"
#include "port_console.h"
#include <stdlib.h>
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

typedef struct { f32 cutoff; coord3d ring[32]; s32 count,kept,removed; } NativeArmLayout;

static s32 paKeepHand(const Vtx *a,const Vtx *b,const Vtx *c,u32 image,void *context)
{
    const NativeArmLayout *layout=context;
    return a->v.ob[2]>=layout->cutoff && b->v.ob[2]>=layout->cutoff && c->v.ob[2]>=layout->cutoff;
}

/* Retain stock hand commands and vertices; only suppress proximal arm triangles. */
static Gfx *paStockHandList(const ModelRoData_DisplayListRecord *data,const NativeArmLayout *layout)
{
    const Vtx *cache[16]={0}; Gfx *copy; s32 length,i,j;
    for(length=0;length<4096;length++) if((data->Primary[length].words.w0>>24)==(u8)G_ENDDL) break;
    if(length==4096) return NULL;
    copy=dynAllocate((length+1)*sizeof(Gfx)); memcpy(copy,data->Primary,(length+1)*sizeof(Gfx));
    for(i=0;i<length;i++) {
        u32 w0=copy[i].words.w0,w1=copy[i].words.w1,op=w0>>24;
        if(op==G_VTX) {
            s32 n=(w0&65535)/sizeof(Vtx),first=(w0>>16)&15;
            const Vtx *v=(const Vtx *)((uintptr_t)data->BaseAddr+(w1&0xffffff));
            for(j=0;j<n;j++) cache[first+j]=v+j;
        } else if(op==(u8)G_TRI1 || op==(u8)G_TRI4) for(j=0;j<(op==(u8)G_TRI4?4:1);j++) {
            s32 a,b,c;
            if(op==(u8)G_TRI1) {a=((w1>>16)&255)/10;b=((w1>>8)&255)/10;c=(w1&255)/10;}
            else {a=(w1>>(j*8))&15;b=(w1>>(j*8+4))&15;c=(w0>>(j*4))&15;}
            if(a==b || b==c || a==c || paKeepHand(cache[a],cache[b],cache[c],0,(void *)layout)) continue;
            if(op==(u8)G_TRI1) copy[i].words.w1=0;
            else {copy[i].words.w0&=~(15u<<(j*4));copy[i].words.w1&=~(255u<<(j*8));}
        }
    }
    return copy;
}

/* Pistol palms share their DL with the arm. The large empty interval between
 * wrist and elbow rings identifies the long forearm triangles, not the palm's
 * many nonplanar edges. Preserve everything distal to that interface. */
static s32 paLayout(const ModelRoData_DisplayListRecord *data,NativeArmLayout *layout)
{
    s16 sorted[1024]; const Vtx *cache[16]={0};
    s32 i,j,n=0,gap=0; layout->count=layout->kept=layout->removed=0;
    if (!data->numVertices || data->numVertices>1024) return FALSE;
    for (i=0;i<data->numVertices;i++) {
        s16 z=data->Vertices[i].coord.z;
        for (j=0;j<n && sorted[j]<z;j++);
        if (j<n && sorted[j]==z) continue;
        for (s32 k=n;k>j;k--) sorted[k]=sorted[k-1]; sorted[j]=z; ++n;
    }
    for (i=1;i<n;i++) if (sorted[i]<=0 && sorted[i-1]-sorted[0]<(sorted[n-1]-sorted[0])*.5f
        && sorted[i]-sorted[i-1]>gap) {
        gap=sorted[i]-sorted[i-1]; layout->cutoff=(sorted[i]+sorted[i-1])*.5f;
    }
    if (gap<30) return FALSE;
    for (i=0;i<4096;i++) {
        const Gfx *command=data->Primary+i; u32 w0=command->words.w0,w1=command->words.w1,op=w0>>24;
        if (op==(u8)G_ENDDL) return layout->count>=3;
        if (op==G_VTX) {
            s32 count=(w0&65535)/sizeof(Vtx),first=(w0>>16)&15;
            const Vtx *v=(const Vtx *)((uintptr_t)data->BaseAddr+(w1&0xffffff));
            for (j=0;j<count;j++) cache[first+j]=v+j;
        } else if (op==(u8)G_TRI1 || op==(u8)G_TRI4) {
            s32 triangles=op==(u8)G_TRI4 ? 4 : 1;
            for (j=0;j<triangles;j++) {
                s32 ids[3],k;
                if (op==(u8)G_TRI1) { ids[0]=((w1>>16)&255)/10; ids[1]=((w1>>8)&255)/10; ids[2]=(w1&255)/10; }
                else { ids[0]=(w1>>(8*j))&15; ids[1]=(w1>>(8*j+4))&15; ids[2]=(w0>>(4*j))&15; }
                if (ids[0]==ids[1] || ids[0]==ids[2] || ids[1]==ids[2]) continue;
                if (paKeepHand(cache[ids[0]],cache[ids[1]],cache[ids[2]],0,layout)) { ++layout->kept; continue; }
                ++layout->removed;
                for (k=0;k<3;k++) {
                    const Vtx *a=cache[ids[k]],*b=cache[ids[(k+1)%3]];
                    if (a->v.ob[2]<layout->cutoff || b->v.ob[2]<layout->cutoff) continue;
                    for (s32 end=0;end<2;end++) {
                        const Vtx *v=end ? b : a; s32 r;
                        for (r=0;r<layout->count;r++) if (layout->ring[r].x==v->v.ob[0] && layout->ring[r].y==v->v.ob[1] && layout->ring[r].z==v->v.ob[2]) break;
                        if (r==layout->count) {
                            if (r==32) return FALSE;
                            layout->ring[layout->count++]=(coord3d){v->v.ob[0],v->v.ob[1],v->v.ob[2]};
                        }
                    }
                }
            }
        }
    }
    return FALSE;
}

typedef struct { Vtx v[3]; u32 image; } NativeSkinTriangle;
typedef struct {
    coord3d sourceMin, sourceMax, targetMin, targetMax;
    coord3d sourceElbowMin, sourceElbowMax, targetElbowMin, targetElbowMax;
    f32 scale, width, depth, elbowWidth, elbowDepth, watchCenter, extension;
    s32 cuff,bond,ringCount,stitch;
    coord3d ring[32];
    f32 angles[32];
    NativeSkinTriangle skin[64];
    s32 skinCount;
} NativeArmPose;

static s32 paNativeFitWithLayout(const ModelRoData_DisplayListRecord *data, f32 minZ, f32 wrist,
    s32 cuff, s32 bond, NativeArmPose *pose,const NativeArmLayout *layout)
{
    f32 band = (wrist-minZ)*0.04f;
    s32 i, k;
    pose->ringCount=pose->skinCount=0; pose->stitch=FALSE;
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
        if (layout) {
            s32 r;
            for (r=0;r<layout->count;r++) if (layout->ring[r].x==vertex->v.ob[0] && layout->ring[r].y==vertex->v.ob[1] && layout->ring[r].z==vertex->v.ob[2]) break;
            if (r==layout->count) continue;
        } else if (vertex->v.ob[2] < wrist-band || vertex->v.ob[2] > wrist+band) continue;
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

static s32 paNativeFit(const ModelRoData_DisplayListRecord *data,f32 minZ,f32 wrist,s32 cuff,s32 bond,NativeArmPose *pose)
{ return paNativeFitWithLayout(data,minZ,wrist,cuff,bond,pose,NULL); }

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


/* Sample the native forearm's material mapping, never its deformed geometry.
 * The watch wrist keeps its fitted shape, while UVs and vertex shade agree
 * with the unchanged hand on the other side of their shared interface. */
static void paSkinSetup(NativeArmPose *pose,const ModelRoData_DisplayListRecord *data,const NativeArmLayout *layout)
{
    const Vtx *cache[16]={0}; u32 image=0; s32 i,j;
    pose->skinCount=0;
    for(i=0;i<4096;i++) {
        const Gfx *cmd=data->Primary+i; u32 op=cmd->words.w0>>24,w0=cmd->words.w0,w1=cmd->words.w1;
        if(op==(u8)G_ENDDL) return;
        if(op==G_SETTIMG) image=w1;
        else if(op==G_VTX) {
            s32 n=(w0&65535)/sizeof(Vtx),first=(w0>>16)&15;
            const Vtx *v=(const Vtx *)((uintptr_t)data->BaseAddr+(w1&0xffffff));
            for(j=0;j<n;j++) cache[first+j]=v+j;
        } else if(op==(u8)G_TRI1 || op==(u8)G_TRI4) for(j=0;j<(op==(u8)G_TRI4?4:1);j++) {
            s32 a,b,c;
            if(op==(u8)G_TRI1) {a=((w1>>16)&255)/10;b=((w1>>8)&255)/10;c=(w1&255)/10;}
            else {a=(w1>>(j*8))&15;b=(w1>>(j*8+4))&15;c=(w0>>(j*4))&15;}
            if(a==b || b==c || a==c || paKeepHand(cache[a],cache[b],cache[c],0,(void *)layout)) continue;
            if(pose->skinCount==64) {pose->skinCount=0;return;}
            NativeSkinTriangle *triangle=pose->skin+pose->skinCount++;
            triangle->image=image; triangle->v[0]=*cache[a];triangle->v[1]=*cache[b];triangle->v[2]=*cache[c];
        }
    }
    pose->skinCount=0;
}

static f32 paDot(const coord3d *a,const coord3d *b) {return a->x*b->x+a->y*b->y+a->z*b->z;}
static coord3d paSubtract(coord3d a,coord3d b) {return (coord3d){a.x-b.x,a.y-b.y,a.z-b.z};}
static f32 paSkinWeights(const NativeSkinTriangle *triangle,coord3d point,f32 *weights)
{
    coord3d v[3],ab,ac,ap; f32 best=1e30f; s32 i,k;
    for(i=0;i<3;i++) for(k=0;k<3;k++) v[i].f[k]=triangle->v[i].v.ob[k];
    ab=paSubtract(v[1],v[0]);ac=paSubtract(v[2],v[0]);ap=paSubtract(point,v[0]);
    { f32 aa=paDot(&ab,&ab),cc=paDot(&ac,&ac),cross=paDot(&ab,&ac),d=aa*cc-cross*cross;
      if(d>.0001f) {
        f32 u=(cc*paDot(&ab,&ap)-cross*paDot(&ac,&ap))/d;
        f32 w=(aa*paDot(&ac,&ap)-cross*paDot(&ab,&ap))/d;
        if(u>=0 && w>=0 && u+w<=1) {
            coord3d delta={ap.x-u*ab.x-w*ac.x,ap.y-u*ab.y-w*ac.y,ap.z-u*ab.z-w*ac.z};
            best=paDot(&delta,&delta);weights[0]=1-u-w;weights[1]=u;weights[2]=w;
        }
      }
    }
    for(i=0;i<3;i++) {
        s32 next=(i+1)%3; coord3d edge=paSubtract(v[next],v[i]),offset=paSubtract(point,v[i]),delta;
        f32 length=paDot(&edge,&edge),t=length>0 ? paDot(&edge,&offset)/length : 0,distance;
        if(t<0)t=0;if(t>1)t=1;
        delta=(coord3d){offset.x-t*edge.x,offset.y-t*edge.y,offset.z-t*edge.z};distance=paDot(&delta,&delta);
        if(distance<best) {best=distance;weights[0]=weights[1]=weights[2]=0;weights[i]=1-t;weights[next]=t;}
    }
    return best;
}

static void paNativeSurface(const coord3d *source,u32 image,Vtx *vertex,f32 light,void *context)
{
    const NativeArmPose *pose=context; NativeArmPose sample; coord3d point;
    f32 best=1e30f,weights[3]; const NativeSkinTriangle *selected=NULL; s32 i,j;
    if(!pose->skinCount) return;
    sample=*pose;sample.extension=1;paNativePoint(source,&point,PORT_ARM_FOREARM,&sample);
    for(i=0;i<pose->skinCount;i++) if(pose->skin[i].image==image) {
        f32 candidate[3],distance=paSkinWeights(pose->skin+i,point,candidate);
        if(distance<best) {best=distance;selected=pose->skin+i;memcpy(weights,candidate,sizeof(weights));}
    }
    if(!selected) return;
    for(i=0;i<2;i++) {
        f32 value=0;for(j=0;j<3;j++) value+=weights[j]*selected->v[j].v.tc[i];
        vertex->v.tc[i]=(s16)value;
    }
    for(i=0;i<3;i++) {
        f32 value=0;for(j=0;j<3;j++) value+=weights[j]*selected->v[j].v.cn[i];
        value*=light;if(value<0)value=0;if(value>255)value=255;vertex->v.cn[i]=(u8)value;
    }
}

s32 portWardrobeRenderNode(ModelRenderData *renderdata, ModelNode *node,
    ModelRoData_DisplayListRecord *nativeHand)
{
    ModelRoData_DisplayListRecord *data = &node->Data->DisplayList;
    f32 minZ = 32767, maxZ = -32768, light;
    s32 i;
    u32 matrix;
    NativeArmPose pose;
    Gfx *stockHand;
    PortArmMesh native; NativeArmLayout layout;
    const char *stockAudit=getenv("GE_ARM_AUDIT_STOCK"),*adsAudit=getenv("GE_ADS_AUDIT");
    s32 preserve=paItem!=ITEM_FIST;
    Gfx *gdl;
    if (!paItem || renderdata->unk18 || !(renderdata->flags&1) || !data->Primary
        || !data->Vertices || !data->numVertices || data->Secondary) return FALSE;
    if(portHandGalleryStock()) return FALSE;
    /* Paired local comparisons only; never a persisted gameplay setting. */
    if (paItem!=ITEM_FIST && stockAudit && *stockAudit && adsAudit && *adsAudit) return FALSE;
    for (i = 0; i < data->numVertices; i++) {
        f32 z = data->Vertices[i].coord.z;
        if (z < minZ) minZ = z;
        if (z > maxZ) maxZ = z;
    }
    /* Only proximal forearms extend behind -150. Distal hands/fingers and
     * weapon metal keep their native geometry, grip bones and animations. */
    if (minZ >= -150 || maxZ > 50 || paDecode(data, &matrix) <= 0) return FALSE;
    if (preserve && !paLayout(data,&layout)) return FALSE;
    if (preserve && !(stockHand=paStockHandList(data,&layout))) return FALSE;
    if (!paNativeFitWithLayout(data,minZ,paItem==ITEM_FIST ? 50 : 0,
        g_CurrentPlayer->bondtype,fileGetBondForCurrentFolder(),&pose,preserve ? &layout : NULL)) return FALSE;
    pose.stitch=TRUE;
    if(preserve) paSkinSetup(&pose,data,&layout);
    if (getenv("GE_ARM_AUDIT") && preserve) {
        static u32 seen[32];
        s32 variant=portWardrobeVariant(g_CurrentPlayer->bondtype,fileGetBondForCurrentFolder());
        u32 flag=1u<<(variant*2+(paHand==GUNLEFT));
        if (!(seen[paItem]&flag)) {
            seen[paItem]|=flag;
            portConLogf("ge: native-skin: item=%d wardrobe=%d left=%d hand_triangles=%d forearm_triangles=%d corners=%d cutoff=%.1f\n",
                paItem,variant,paHand==GUNLEFT,layout.kept,layout.removed,layout.count,layout.cutoff);
        }
    }
    /* Native pistol fingers use authored viewmodel lighting. Match that on
     * their replacement arm; retain the existing chopping/export lighting. */
    light = preserve ? 1 : 0.55f+(g_CurrentPlayer->tileColor.r+g_CurrentPlayer->tileColor.g
        +g_CurrentPlayer->tileColor.b)*(0.45f/(255.0f*3.0f));
    if (!portArmMeshBuildJoinedSurface(&native, g_CurrentPlayer->bondtype, fileGetBondForCurrentFolder(),
        PORT_ARM_FOREARM|(paHand == GUNLEFT ? PORT_ARM_WATCH : 0)|(preserve ? PORT_ARM_NATIVE_SKIN : 0),
        paNativePoint, &pose, light, 255, pose.stitch ? paWristJoin : NULL,preserve ? paNativeSurface : NULL)) return FALSE;
    gdl = renderdata->gdl;
    gDPPipeSync(gdl++);
    gSPMatrix(gdl++, matrix, G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH|G_LIGHTING|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR|G_FOG);
    gSPSetGeometryMode(gdl++, G_SHADE|G_SHADING_SMOOTH|(renderdata->zbufferenabled ? G_ZBUFFER : 0));
    if(preserve) {
        renderdata->gdl=gdl;modelApplyRenderModeType3(renderdata,TRUE);gdl=renderdata->gdl;
        if(renderdata->zbufferenabled) {gDPSetRenderMode(gdl++,G_RM_FOG_PRIM_A|Z_UPD,G_RM_AA_ZB_OPA_SURF2);}
        else {gDPSetRenderMode(gdl++,G_RM_FOG_PRIM_A,G_RM_AA_OPA_SURF2);}
    } else {
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
        if (renderdata->zbufferenabled) { gDPSetRenderMode(gdl++, G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2); }
        else { gDPSetRenderMode(gdl++, G_RM_AA_OPA_SURF,G_RM_AA_OPA_SURF2); }
    }
    gSPDisplayList(gdl++, osVirtualToPhysical(native.color));
    if (preserve) {
        /* Render the hand last through model.c's unchanged native setup,
         * retaining authored mip sampling after the matched wrist draw. */
        gDPPipeSync(gdl++);
        gDPSetTextureLOD(gdl++,G_TL_LOD);
        *nativeHand=*data; nativeHand->Primary=stockHand;
    }
    renderdata->gdl = gdl;
    return preserve ? PORT_WARDROBE_NATIVE_HAND : PORT_WARDROBE_DRAWN;
}

typedef struct {
    NativeArmPose pose;
    NativeArmLayout layout;
    PortGripSpace space;
    PortGripMap map;
    void *context;
} NativeGrip;

static void paGripHand(const coord3d *source,coord3d *target,s32 part,void *context)
{
    NativeGrip *grip=context;
    grip->map(source,target,PORT_ARM_HAND,&grip->space,grip->context);
}
static void paGripArm(const coord3d *source,coord3d *target,s32 part,void *context)
{
    NativeGrip *grip=context; coord3d native;
    paNativePoint(source,&native,part,&grip->pose);
    grip->map(&native,target,part,&grip->space,grip->context);
}
static s32 paGripJoin(const coord3d *a,const coord3d *b,PortArmJoinPoint *points,void *context)
{
    NativeGrip *grip=context; s32 i,count=paWristJoin(a,b,points,&grip->pose);
    for (i=0;i<count;i++) { coord3d native=points[i].target;
        grip->map(&native,&points[i].target,PORT_ARM_FOREARM,&grip->space,grip->context);
    }
    return count;
}
static s32 paGripKeep(const Vtx *a,const Vtx *b,const Vtx *c,u32 image,void *context)
{
    NativeGrip *grip=context;
    return portArmGripSkin(image) && paKeepHand(a,b,c,image,&grip->layout);
}
static void paGripSurface(const coord3d *source,u32 image,Vtx *vertex,f32 light,void *context)
{
    NativeGrip *grip=context;paNativeSurface(source,image,vertex,light,&grip->pose);
}

s32 portWardrobeBuildGrip(PortArmMesh *mesh,s32 cuff,s32 bond,s32 left,f32 light,u8 alpha,PortGripMap map,void *context)
{
    const PortGripSource *source=portArmGripSource();
    NativeGrip grip; PortArmMesh parts[26]; s32 i,count=0;
    f32 minZ=32767;
    Gfx *color,*depth;
    if (!source || !map || !paLayout(source->arm,&grip.layout)) return FALSE;
    for (i=0;i<source->arm->numVertices;i++) if (source->arm->Vertices[i].coord.z<minZ) minZ=source->arm->Vertices[i].coord.z;
    if (!paNativeFitWithLayout(source->arm,minZ,0,cuff,bond,&grip.pose,&grip.layout)) {
        if (getenv("GE_ARM_AUDIT")) portConLogf("ge: grip fit failed ring=%d minz=%.1f\n",grip.layout.count,minZ);
        return FALSE;
    }
    grip.pose.stitch=TRUE; grip.pose.extension=1;
    paSkinSetup(&grip.pose,source->arm,&grip.layout);
    grip.map=map; grip.context=context;
    for (i=0;i<3;i++) {
        grip.space.wrist.f[i]=(grip.pose.targetMin.f[i]+grip.pose.targetMax.f[i])*.5f;
        grip.space.elbow.f[i]=(grip.pose.targetElbowMin.f[i]+grip.pose.targetElbowMax.f[i])*.5f;
    }
    grip.space.elbow.z=minZ;
    for (i=0;i<source->count;i++) {
        if (!portArmMeshBuildFiltered(parts+count,source->parts[i],paGripHand,&grip,light,alpha,paGripKeep,FALSE)) {
            if (getenv("GE_ARM_AUDIT")) portConLogf("ge: grip part rejected index=%d vertices=%d\n",i,source->parts[i]->numVertices);
            return FALSE;
        }
        ++count;
    }
    if (!portArmMeshBuildJoinedSurface(parts+count,cuff,bond,PORT_ARM_FOREARM|(left ? PORT_ARM_WATCH : 0),paGripArm,&grip,light,alpha,paGripJoin,paGripSurface)) return FALSE;
    ++count;
    if(left && portArmMeshBuildWatchBack(parts+count,cuff,bond,paGripArm,&grip,light,alpha)) ++count;
    mesh->color=color=dynAllocate((count+1)*sizeof(Gfx));
    mesh->depth=depth=alpha<255 ? dynAllocate((count+1)*sizeof(Gfx)) : NULL;
    for (i=0;i<count;i++) {
        gSPDisplayList(color++,osVirtualToPhysical(parts[i].color));
        if (depth) { gSPDisplayList(depth++,osVirtualToPhysical(parts[i].depth)); }
    }
    gSPEndDisplayList(color++); if (depth) { gSPEndDisplayList(depth++); }
    return TRUE;
}
