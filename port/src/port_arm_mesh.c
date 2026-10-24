/* Shared first-person wardrobe geometry comes from Csuit_lf_handZ at runtime. */
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "game/dyn.h"
#include "game/image.h"
#include "game/objecthandler.h"
#include "port_arm_mesh.h"
#include "port_wardrobe.h"
#include "gbi_extension.h"
#include "port_console.h"
#include "port_ads.h"
extern float sinf(float);
extern float cosf(float);
extern void *memset(void *, int, size_t);
extern int memcmp(const void *, const void *, size_t);
#if defined(VERSION_JP) || defined(VERSION_EU)
extern f32 watch_time_0;
#else
extern s32 watch_time_0;
#endif

#define ARM_PARTS 7
#define ARM_COMMANDS 4096
#define ARM_SOURCE_VERTICES 1024
static ModelFileHeader armHeader;
static u8 *armBuffer;
static const s32 armCuffSwitch[6] = {7,4,8,9,5,6};
static coord3d armWrist[32];
static s32 armWristCount;

static s32 armSkinImage(u32 address, s32 part)
{
    extern struct texpool *portModelTexPool(void);
    static u32 pixels[2];
    static const s32 textures[2] = {1793,1796};
    s32 i;
    if (!(part & (PORT_ARM_HAND|PORT_ARM_FOREARM))) return FALSE;
    for (i = 0; i < 2; i++) {
        if (!pixels[i]) {
            struct tex *texture = texFindInPool(textures[i], portModelTexPool());
            if (texture) pixels[i] = (u32)(uintptr_t)texture->data;
        }
        if (pixels[i] && pixels[i] == address) return TRUE;
    }
    return FALSE;
}

static void armSkinMaterial(Gfx **color, s32 skin, f32 light)
{
    if (light < 0) light = 0;
    if (light > 1) light = 1;
    gDPPipeSync((*color)++);
    /* Watch-pose skin is darker than the retained native hand. Lift only its
     * sampled skin materials, preserving detail, scene light and vertex alpha.
     * Vertex gain alone cannot lift the already-white authored skin vertices. */
    gDPSetPrimColor((*color)++, 0, 0, skin ? (u8)(28*light) : 0,
        skin ? (u8)(25*light) : 0, skin ? (u8)(22*light) : 0, 0);
    if (skin) {
        gDPSetCombineLERP((*color)++, TEXEL0,0,SHADE,PRIMITIVE, TEXEL0,0,SHADE,0,
            TEXEL0,0,SHADE,PRIMITIVE, TEXEL0,0,SHADE,0);
    } else {
        gDPSetCombineMode((*color)++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    }
}

static s32 armSkinCommands(const ModelRoData_DisplayListRecord *source, s32 length, s32 part)
{
    s32 i, skin = FALSE, commands = 0;
    for (i = 0; i < length; i++) if ((source->Primary[i].words.w0 >> 24) == G_SETTIMG) {
        s32 next = armSkinImage(source->Primary[i].words.w1, part);
        if (next != skin) { commands += 3; skin = next; }
    }
    return commands + (skin ? 3 : 0);
}

static ModelFileHeader *armSource(void)
{
    extern ModelFileHeader suit_lf_hand_header;
    extern void *portAllocHigh(u32 size);
    if (!armBuffer) {
        armBuffer = portAllocHigh(0x10000);
        if (!armBuffer) return NULL;
        armHeader = suit_lf_hand_header;
        load_object_fill_header(&armHeader, (u8 *)"Csuit_lf_handZ", armBuffer, 0x10000, NULL);
    }
    if (!armHeader.RootNode || !armHeader.Switches || armHeader.numSwitches != 10) return NULL;
    return &armHeader;
}

static ModelNode *armFirstDisplay(ModelNode *node)
{
    s32 budget = 32;
    while (node && budget--) {
        if ((node->Opcode & 255) == MODELNODE_OPCODE_DL) return node;
        node = node->Child;
    }
    return NULL;
}

s32 portArmWristBounds(coord3d *minimum, coord3d *maximum)
{
    static s32 ready;
    static coord3d low, high;
    if (!ready) {
        ModelFileHeader *header = armSource();
        ModelNode *hand = header ? armFirstDisplay(header->RootNode) : NULL;
        ModelNode *arm = header ? armFirstDisplay(header->Switches[armCuffSwitch[0]]) : NULL;
        ModelRoData_DisplayListRecord *data, *forearm;
        s32 i, j, k, matches = 0;
        if (!hand || !hand->Data || !arm || !arm->Data) return FALSE;
        data = &hand->Data->DisplayList;
        forearm = &arm->Data->DisplayList;
        if (!data->Vertices || !data->numVertices || data->numVertices > ARM_SOURCE_VERTICES
            || !forearm->Vertices || !forearm->numVertices || forearm->numVertices > ARM_SOURCE_VERTICES) return FALSE;
        for (k = 0; k < 3; k++) { low.f[k] = 32767; high.f[k] = -32768; }
        armWristCount = 0;
        /* Shared positions identify the actual tilted interface. A coordinate
         * slab cannot distinguish its vertices from nearby palm geometry. */
        for (i = 0; i < forearm->numVertices; i++) {
            const Vtx *vertex = (const Vtx *)forearm->Vertices+i;
            for (j = 0; j < data->numVertices; j++) {
                const Vtx *other = (const Vtx *)data->Vertices+j;
                if (memcmp(vertex->v.ob,other->v.ob,sizeof(vertex->v.ob))) continue;
                for (k = 0; k < 3; k++) {
                    if (vertex->v.ob[k] < low.f[k]) low.f[k] = vertex->v.ob[k];
                    if (vertex->v.ob[k] > high.f[k]) high.f[k] = vertex->v.ob[k];
                }
                for (k = 0; k < armWristCount; k++) if (armWrist[k].x==vertex->v.ob[0]
                    && armWrist[k].y==vertex->v.ob[1] && armWrist[k].z==vertex->v.ob[2]) break;
                if (k==armWristCount) {
                    if (armWristCount==32) return FALSE;
                    armWrist[armWristCount++]=(coord3d){vertex->v.ob[0],vertex->v.ob[1],vertex->v.ob[2]};
                }
                ++matches;
                break;
            }
        }
        if (!matches || high.y <= low.y || high.z <= low.z || high.x <= low.x) return FALSE;
        ready = TRUE;
    }
    *minimum = low; *maximum = high;
    return TRUE;
}

s32 portArmWristContains(const coord3d *point)
{
    s32 i;
    for (i=0;i<armWristCount;i++) if (point->x==armWrist[i].x && point->y==armWrist[i].y && point->z==armWrist[i].z) return TRUE;
    return FALSE;
}

f32 portArmWatchCenter(void)
{
    ModelFileHeader *header = armSource();
    ModelNode *clock = header ? header->Switches[0] : NULL;
    if (clock && clock->Data && (clock->Opcode&255) == MODELNODE_OPCODE_GROUPSIMPLE)
        return clock->Data->GroupSimple.Origin.x;
    return -420;
}

s32 portArmElbowBounds(s32 cuff, s32 bond, coord3d *minimum, coord3d *maximum)
{
    ModelFileHeader *header = armSource();
    ModelNode *node = header ? armFirstDisplay(header->Switches[armCuffSwitch[portWardrobeVariant(cuff,bond)]]) : NULL;
    ModelRoData_DisplayListRecord *data;
    s32 i,k,minX=32767;
    if (!node || !node->Data) return FALSE;
    data = &node->Data->DisplayList;
    if (!data->Vertices || !data->numVertices || data->numVertices>ARM_SOURCE_VERTICES) return FALSE;
    for (i=0;i<data->numVertices;i++) if (data->Vertices[i].coord.x<minX) minX=data->Vertices[i].coord.x;
    for (k=0;k<3;k++) { minimum->f[k]=32767; maximum->f[k]=-32768; }
    for (i=0;i<data->numVertices;i++) if (data->Vertices[i].coord.x<=minX+100) {
        const Vtx *v=(const Vtx *)data->Vertices+i;
        for (k=0;k<3;k++) {
            if (v->v.ob[k]<minimum->f[k]) minimum->f[k]=v->v.ob[k];
            if (v->v.ob[k]>maximum->f[k]) maximum->f[k]=v->v.ob[k];
        }
    }
    return maximum->y>minimum->y && maximum->z>minimum->z;
}

static s32 armListLength(const ModelRoData_DisplayListRecord *data)
{
    s32 i;
    if (!data || !data->Primary || !data->Vertices || !data->numVertices
        || data->numVertices > ARM_SOURCE_VERTICES) return 0;
    for (i = 0; i < ARM_COMMANDS; i++) {
        const Gfx *command = data->Primary+i;
        u32 op = command->words.w0 >> 24;
        if (op == (u8)G_ENDDL) return i+1;
        if (op == G_DL) return 0;
        if (op == G_MTX && ((command->words.w1 >> 24) != 3 || (command->words.w1 & 0xffffff) > 192)) return 0;
        if (op == G_VTX) {
            uintptr_t address = (uintptr_t)data->BaseAddr+(command->words.w1 & 0xffffff);
            uintptr_t begin = (uintptr_t)data->Vertices;
            s32 count = (command->words.w0 & 0xffff)/sizeof(Vtx);
            s32 first = (command->words.w0 >> 16)&15;
            if ((command->words.w1 >> 24) != 5 || !count || first+count > 16 || address < begin
                || (address-begin)%sizeof(Vtx) || address+count*sizeof(Vtx) > begin+data->numVertices*sizeof(Vtx)) return 0;
        }
    }
    return 0;
}

static void armSectionTriangle(const Vtx **v, s32 a, s32 b, s32 c, f32 x, coord3d *low, coord3d *high)
{
    s32 indices[3]={a,b,c},i,k;
    if (a==b || b==c || c==a || a>=16 || b>=16 || c>=16 || !v[a] || !v[b] || !v[c]) return;
    for (i=0;i<3;i++) {
        const Vtx *p=v[indices[i]], *q=v[indices[(i+1)%3]];
        f32 px=p->v.ob[0],qx=q->v.ob[0],t;
        if ((x<px && x<qx) || (x>px && x>qx)) continue;
        if (px==qx) {
            for (k=1;k<3;k++) {
                if (p->v.ob[k]<low->f[k]) low->f[k]=p->v.ob[k];
                if (p->v.ob[k]>high->f[k]) high->f[k]=p->v.ob[k];
                if (q->v.ob[k]<low->f[k]) low->f[k]=q->v.ob[k];
                if (q->v.ob[k]>high->f[k]) high->f[k]=q->v.ob[k];
            }
        } else {
            t=(x-px)/(qx-px);
            for (k=1;k<3;k++) {
                f32 value=p->v.ob[k]+t*(q->v.ob[k]-p->v.ob[k]);
                if (value<low->f[k]) low->f[k]=value;
                if (value>high->f[k]) high->f[k]=value;
            }
        }
    }
}

s32 portArmSectionBounds(s32 cuff, s32 bond, f32 x, coord3d *minimum, coord3d *maximum)
{
    typedef struct { f32 x; coord3d low,high; } Section;
    static Section cached[6][256];
    static s32 counts[6];
    s32 variant=portWardrobeVariant(cuff,bond),i,length;
    ModelFileHeader *header;
    ModelNode *node;
    ModelRoData_DisplayListRecord *data;
    const Vtx *vertices[16]={0};
    for (i=0;i<counts[variant];i++) if (cached[variant][i].x==x) {
        *minimum=cached[variant][i].low; *maximum=cached[variant][i].high; return TRUE;
    }
    header=armSource();
    node=header ? armFirstDisplay(header->Switches[armCuffSwitch[variant]]) : NULL;
    if (!node || !node->Data) return FALSE;
    data=&node->Data->DisplayList; length=armListLength(data);
    if (!length) return FALSE;
    *minimum=(coord3d){x,32767,32767}; *maximum=(coord3d){x,-32768,-32768};
    for (i=0;i<length;i++) {
        u32 w0=data->Primary[i].words.w0,w1=data->Primary[i].words.w1,op=w0>>24;
        s32 j;
        if (op==G_VTX) {
            s32 n=(w0&0xffff)/sizeof(Vtx),first=(w0>>16)&15;
            const Vtx *v=(const Vtx *)((uintptr_t)data->BaseAddr+(w1&0xffffff));
            for (j=0;j<n;j++) vertices[first+j]=v+j;
        } else if (op==(u8)G_TRI1) armSectionTriangle(vertices,((w1>>16)&255)/10,((w1>>8)&255)/10,(w1&255)/10,x,minimum,maximum);
        else if (op==(u8)G_TRI4) for (j=0;j<4;j++) armSectionTriangle(vertices,(w1>>(8*j))&15,(w1>>(8*j+4))&15,(w0>>(4*j))&15,x,minimum,maximum);
    }
    if (maximum->y<=minimum->y || maximum->z<=minimum->z) return FALSE;
    if (counts[variant]<256) {
        Section *entry=&cached[variant][counts[variant]++]; entry->x=x; entry->low=*minimum; entry->high=*maximum;
    }
    return TRUE;
}

static void armTriangleIndices(const Gfx *command, s32 triangle, s32 *indices)
{
    u32 w0=command->words.w0,w1=command->words.w1;
    if ((w0>>24)==(u8)G_TRI1) {
        indices[0]=((w1>>16)&255)/10; indices[1]=((w1>>8)&255)/10; indices[2]=(w1&255)/10;
    } else {
        indices[0]=(w1>>(triangle*8))&15; indices[1]=(w1>>(triangle*8+4))&15; indices[2]=(w0>>(triangle*4))&15;
    }
}

typedef struct {
    const Gfx *key;
    u32 image;
    s32 count;
    Vtx a[32],b[32],center;
} ArmCuffCap;

typedef struct { const Vtx *a,*b; u32 image; s32 count,used; } ArmBoundaryEdge;
static s32 armSamePosition(const Vtx *a,const Vtx *b) { return !memcmp(a->v.ob,b->v.ob,sizeof(a->v.ob)); }

/* The original sleeve rim is open. Close its nearest substantial clothing
 * boundary in its own material, behind the exposed wrist. Skin passes through
 * the center; the cap fills the otherwise visible annulus inside the cuff. */
static void armFindCuff(const ModelRoData_DisplayListRecord *source, s32 length, ArmCuffCap *cap)
{
    s32 capacity=source->numVertices*6,edges=0,i,j,k;
    ArmBoundaryEdge *edge=calloc(capacity,sizeof(*edge));
    const Vtx *cache[16]={0}; u32 image=0;
    f32 nearest=-32768;
    if (!edge) return;
    cap->count=0;
    for (i=0;i<length;i++) {
        const Gfx *command=source->Primary+i; u32 op=command->words.w0>>24;
        if (op==G_SETTIMG) image=command->words.w1;
        else if (op==G_VTX) {
            s32 n=(command->words.w0&65535)/sizeof(Vtx),first=(command->words.w0>>16)&15;
            const Vtx *v=(const Vtx *)((uintptr_t)source->BaseAddr+(command->words.w1&0xffffff));
            for (j=0;j<n;j++) cache[first+j]=v+j;
        } else if (op==(u8)G_TRI1 || op==(u8)G_TRI4) {
            s32 triangles=op==(u8)G_TRI4 ? 4 : 1;
            for (j=0;j<triangles;j++) {
                s32 ids[3]; armTriangleIndices(command,j,ids);
                if (ids[0]>=16 || ids[1]>=16 || ids[2]>=16 || !cache[ids[0]] || !cache[ids[1]] || !cache[ids[2]]) continue;
                if (armSamePosition(cache[ids[0]],cache[ids[1]]) || armSamePosition(cache[ids[0]],cache[ids[2]]) || armSamePosition(cache[ids[1]],cache[ids[2]])) continue;
                for (k=0;k<3;k++) {
                    const Vtx *a=cache[ids[k]],*b=cache[ids[(k+1)%3]];
                    s32 e;
                    for (e=0;e<edges;e++) if ((armSamePosition(a,edge[e].a) && armSamePosition(b,edge[e].b))
                        || (armSamePosition(a,edge[e].b) && armSamePosition(b,edge[e].a))) break;
                    if (e==edges) {
                        if (edges==capacity) { free(edge); return; }
                        edge[e].a=a; edge[e].b=b; edge[e].image=image; ++edges;
                    }
                    ++edge[e].count;
                }
            }
        }
    }
    for (i=0;i<edges;i++) if (edge[i].count==1 && !edge[i].used && edge[i].image && !armSkinImage(edge[i].image,PORT_ARM_FOREARM)) {
        const Vtx *first=edge[i].a,*point=first;
        Vtx a[32],b[32]; f32 sum[9]={0},lowY=32767,highY=-32768,lowZ=32767,highZ=-32768;
        s32 n=0,e=i,closed=FALSE;
        while (n<32) {
            const Vtx *next;
            edge[e].used=TRUE;
            next=armSamePosition(point,edge[e].a) ? edge[e].b : edge[e].a;
            a[n]=*point; b[n]=*next; ++n;
            for (k=0;k<3;k++) sum[k]+=point->v.ob[k];
            for (k=0;k<2;k++) sum[k+3]+=point->v.tc[k];
            for (k=0;k<4;k++) sum[k+5]+=point->v.cn[k];
            if (point->v.ob[1]<lowY) lowY=point->v.ob[1]; if (point->v.ob[1]>highY) highY=point->v.ob[1];
            if (point->v.ob[2]<lowZ) lowZ=point->v.ob[2]; if (point->v.ob[2]>highZ) highZ=point->v.ob[2];
            point=next;
            if (armSamePosition(point,first)) { closed=TRUE; break; }
            for (e=0;e<edges;e++) if (edge[e].count==1 && !edge[e].used && !armSkinImage(edge[e].image,PORT_ARM_FOREARM)
                && (armSamePosition(point,edge[e].a) || armSamePosition(point,edge[e].b))) break;
            if (e==edges) break;
        }
        if (!closed || n<6 || highY-lowY<100 || highZ-lowZ<100 || sum[0]/n<=nearest) continue;
        nearest=sum[0]/n; cap->count=n; cap->image=edge[i].image;
        memcpy(cap->a,a,n*sizeof(Vtx)); memcpy(cap->b,b,n*sizeof(Vtx)); cap->center=a[0];
        for (k=0;k<3;k++) cap->center.v.ob[k]=(s16)(sum[k]/n);
        for (k=0;k<2;k++) cap->center.v.tc[k]=(s16)(sum[k+3]/n);
        for (k=0;k<4;k++) cap->center.v.cn[k]=(u8)(sum[k+5]/n);
    }
    free(edge);
}

static ArmCuffCap armCuffs[6];

static const ArmCuffCap *armCuff(const ModelRoData_DisplayListRecord *source,s32 length)
{
    ArmCuffCap *caps=armCuffs; s32 i;
    for (i=0;i<6;i++) if (caps[i].key==source->Primary) return caps+i;
    for (i=0;i<6;i++) if (!caps[i].key) {
        caps[i].key=source->Primary; armFindCuff(source,length,caps+i);
        if (getenv("GE_ARM_AUDIT")) portConLogf("ge: arm-cuff: closed rim edges=%d\n",caps[i].count);
        return caps+i;
    }
    return NULL;
}

static void armMapVertex(Vtx *vertex,const coord3d *origin,f32 sine,f32 cosine,s32 part,
    PortArmMap map,void *context,f32 light,u8 alpha)
{
    coord3d local,target; s32 k;
    f32 x=vertex->v.ob[0],z=vertex->v.ob[2];
    local.x=x*cosine+z*sine+origin->x; local.y=vertex->v.ob[1]+origin->y;
    local.z=z*cosine-x*sine+origin->z; map(&local,&target,part,context);
    for (k=0;k<3;k++) {
        f32 value=target.f[k]; if (value!=value) value=0;
        if (value>32767) value=32767; if (value<-32768) value=-32768;
        vertex->v.ob[k]=(s16)value; vertex->v.cn[k]=(u8)(vertex->v.cn[k]*light);
    }
    vertex->v.cn[3]=alpha;
}

static s32 armJoinTriangle(const Vtx **cache, s32 *indices, PortArmJoin join, void *context, PortArmJoinPoint *points)
{
    s32 edge;
    if (indices[0]>=16 || indices[1]>=16 || indices[2]>=16
        || !cache[indices[0]] || !cache[indices[1]] || !cache[indices[2]]) return 0;
    if (indices[0]==indices[1] || indices[0]==indices[2] || indices[1]==indices[2]) return 0;
    for (edge=0;edge<3;edge++) {
        const Vtx *a=cache[indices[edge]], *b=cache[indices[(edge+1)%3]];
        coord3d pa={a->v.ob[0],a->v.ob[1],a->v.ob[2]},pb={b->v.ob[0],b->v.ob[1],b->v.ob[2]};
        s32 count=join(&pa,&pb,points,context);
        if (count>0 && count<=32) {
            s32 ordered[3]={indices[edge],indices[(edge+1)%3],indices[(edge+2)%3]};
            memcpy(indices,ordered,sizeof(ordered)); return count;
        }
    }
    return 0;
}

static void armJoinCounts(const ModelRoData_DisplayListRecord *source, s32 length,
    PortArmJoin join, void *context, s32 *commands, s32 *vertices)
{
    const Vtx *cache[16]={0}; s32 i,j;
    *commands=*vertices=0;
    if (!join) return;
    for (i=0;i<length;i++) {
        const Gfx *command=source->Primary+i; u32 op=command->words.w0>>24;
        if (op==G_VTX) {
            s32 n=(command->words.w0&65535)/sizeof(Vtx),first=(command->words.w0>>16)&15;
            const Vtx *v=(const Vtx *)((uintptr_t)source->BaseAddr+(command->words.w1&0xffffff));
            for (j=0;j<n;j++) cache[first+j]=v+j;
        } else if (op==(u8)G_TRI1 || op==(u8)G_TRI4) {
            s32 triangles=op==(u8)G_TRI4 ? 4 : 1;
            *commands+=triangles-1;
            for (j=0;j<triangles;j++) {
                PortArmJoinPoint points[32]; s32 indices[3],count;
                armTriangleIndices(command,j,indices);
                count=armJoinTriangle(cache,indices,join,context,points);
                if (count) { *commands+=2*count+2; *vertices+=3*(count+1)+16; }
            }
        }
    }
}

/* Authored UV vertices have one material owner. Never resample a vertex also
 * used by clothing or by a different skin texture. */
static void armSurfaceOwners(const ModelRoData_DisplayListRecord *source,s32 length,u32 *owners)
{
    s32 cache[16]={0},i,j; u32 image=0;
    memset(owners,0,source->numVertices*sizeof(u32));
    for(i=0;i<length;i++) {
        const Gfx *command=source->Primary+i; u32 op=command->words.w0>>24;
        if(op==G_SETTIMG) image=command->words.w1;
        else if(op==G_VTX) {
            s32 n=(command->words.w0&65535)/sizeof(Vtx),first=(command->words.w0>>16)&15;
            s32 offset=((uintptr_t)source->BaseAddr+(command->words.w1&0xffffff)-(uintptr_t)source->Vertices)/sizeof(Vtx);
            for(j=0;j<n;j++) cache[first+j]=offset+j;
        } else if(op==(u8)G_TRI1 || op==(u8)G_TRI4) for(j=0;j<(op==(u8)G_TRI4?4:1);j++) {
            s32 ids[3],k; u32 owner=armSkinImage(image,PORT_ARM_FOREARM)?image:1;
            armTriangleIndices(command,j,ids);
            if(ids[0]==ids[1] || ids[1]==ids[2] || ids[0]==ids[2]) continue;
            for(k=0;k<3;k++) {
                u32 *entry=owners+cache[ids[k]];
                if(!*entry) *entry=owner; else if(*entry!=owner) *entry=1;
            }
        }
    }
}

static void armCopyPart(const ModelRoData_DisplayListRecord *source, s32 length,
    const coord3d *origin, f32 angle, s32 part, PortArmMap map, void *context, f32 light, u8 alpha,
    Gfx **color, Gfx **depth, PortArmJoin join, PortArmKeepTriangle keep,s32 matchSkinTone,PortArmSurface surface,s32 nativeMaterial)
{
    s32 i, k, skin = FALSE;
    f32 sine = sinf(angle), cosine = cosf(angle);
    const Vtx *cache[16]={0}, *mapped[16]={0};
    const ArmCuffCap *cap=part==PORT_ARM_FOREARM ? armCuff(source,length) : NULL;
    Vtx *cuff=NULL,*joined=NULL; s32 capped=FALSE; u32 image=0;
    u32 owners[ARM_SOURCE_VERTICES];
    if(surface && part==PORT_ARM_FOREARM) armSurfaceOwners(source,length,owners);
    if (cap && cap->count) {
        cuff=dynAllocate((cap->count*3+16)*sizeof(Vtx));
        for (i=0;i<cap->count;i++) {
            cuff[i*3]=cap->a[i]; cuff[i*3+1]=cap->b[i]; cuff[i*3+2]=cap->center;
            for (k=0;k<3;k++) armMapVertex(cuff+i*3+k,origin,sine,cosine,part,map,context,light,alpha);
        }
    }
    if (join) {
        s32 commands,count; armJoinCounts(source,length,join,context,&commands,&count);
        if (count) joined=dynAllocate(count*sizeof(Vtx));
    }
    Vtx *vertices = dynAllocateVertices(source->numVertices);
    memcpy(vertices, source->Vertices, source->numVertices*sizeof(Vtx));
    for (i=0;i<source->numVertices;i++) {
        armMapVertex(vertices+i,origin,sine,cosine,part,map,context,light,alpha);
        if(surface && part==PORT_ARM_FOREARM && owners[i]>1) {
            const Vtx *v=(const Vtx *)source->Vertices+i;
            coord3d point={v->v.ob[0],v->v.ob[1],v->v.ob[2]};
            surface(&point,owners[i],vertices+i,light,context);
        }
    }
    for (i = 0; i < length; i++) {
        Gfx command = source->Primary[i];
        u32 op = command.words.w0 >> 24;
        if (op == (u8)G_ENDDL) break;
        if (op == G_MTX || op == (u8)G_POPMTX || op == (u8)G_SETGEOMETRYMODE || op == (u8)G_CLEARGEOMETRYMODE
            || op == G_SETCOMBINE || op == (u8)G_SETOTHERMODE_L) continue;
        if (op == (u8)G_SETOTHERMODE_H) {
            u32 shift = (command.words.w0 >> 8)&255;
            if (shift == G_MDSFT_CYCLETYPE || (!nativeMaterial && shift == G_MDSFT_TEXTLOD)) continue;
        }
        if (op == (u8)G_TEXTURE && !nativeMaterial) {
            gSPTexture((*color)++, 0xffff, 0xffff, 0, G_TX_RENDERTILE, G_ON);
            continue;
        }
        if (cuff && !capped && image==cap->image && (op==G_VTX || op==(u8)G_TRI1 || op==(u8)G_TRI4)) {
            for (k=0;k<cap->count;k++) {
                gSPVertex((*color)++,osVirtualToPhysical(cuff+k*3),3,0); gSP1Triangle((*color)++,0,1,2,0);
                if (*depth) { gSPVertex((*depth)++,osVirtualToPhysical(cuff+k*3),3,0); gSP1Triangle((*depth)++,0,1,2,0); }
            }
            for (k=0;k<16;k++) { if (mapped[k]) cuff[cap->count*3+k]=*mapped[k]; else memset(cuff+cap->count*3+k,0,sizeof(Vtx)); }
            gSPVertex((*color)++,osVirtualToPhysical(cuff+cap->count*3),16,0);
            if (*depth) { gSPVertex((*depth)++,osVirtualToPhysical(cuff+cap->count*3),16,0); }
            capped=TRUE;
        }
        if (op == G_VTX) {
            uintptr_t address = (uintptr_t)source->BaseAddr+(command.words.w1 & 0xffffff);
            s32 offset = (address-(uintptr_t)source->Vertices)/sizeof(Vtx);
            if (join || cuff || keep) {
                s32 n=(command.words.w0&65535)/sizeof(Vtx),first=(command.words.w0>>16)&15,j;
                for (j=0;j<n;j++) { cache[first+j]=(const Vtx *)source->Vertices+offset+j; mapped[first+j]=vertices+offset+j; }
            }
            command.words.w1 = osVirtualToPhysical(vertices+offset);
        }
        if ((join || keep) && (op==(u8)G_TRI1 || op==(u8)G_TRI4)) {
            s32 triangle,triangles=op==(u8)G_TRI4 ? 4 : 1;
            for (triangle=0;triangle<triangles;triangle++) {
                PortArmJoinPoint points[32]; s32 indices[3],count,p;
                armTriangleIndices(&command,triangle,indices);
                if (indices[0]==indices[1] || indices[0]==indices[2] || indices[1]==indices[2]) continue;
                if (keep && (indices[0]>=16 || indices[1]>=16 || indices[2]>=16 || !cache[indices[0]] || !cache[indices[1]] || !cache[indices[2]]
                    || !keep(cache[indices[0]],cache[indices[1]],cache[indices[2]],image,context))) continue;
                count=join ? armJoinTriangle(cache,indices,join,context,points) : 0;
                if (!count) {
                    gSP1Triangle((*color)++,indices[0],indices[1],indices[2],0);
                    if (*depth) { gSP1Triangle((*depth)++,indices[0],indices[1],indices[2],0); }
                    continue;
                }
                for (p=0;p<=count;p++) {
                    s32 end;
                    const Vtx *a=mapped[indices[0]], *b=mapped[indices[1]];
                    for (end=0;end<2;end++) {
                        s32 point=p+end-1;
                        Vtx *v=joined+end;
                        if (point<0) *v=*a;
                        else if (point==count) *v=*b;
                        else {
                            f32 t=points[point].fraction;
                            *v=*a;
                            for (k=0;k<3;k++) v->v.ob[k]=(s16)points[point].target.f[k];
                            for (k=0;k<2;k++) v->v.tc[k]=a->v.tc[k]+t*(b->v.tc[k]-a->v.tc[k]);
                            for (k=0;k<4;k++) v->v.cn[k]=a->v.cn[k]+t*(b->v.cn[k]-a->v.cn[k]);
                        }
                    }
                    joined[2]=*mapped[indices[2]];
                    gSPVertex((*color)++,osVirtualToPhysical(joined),3,0);
                    gSP1Triangle((*color)++,0,1,2,0);
                    if (*depth) { gSPVertex((*depth)++,osVirtualToPhysical(joined),3,0); gSP1Triangle((*depth)++,0,1,2,0); }
                    joined+=3;
                }
                /* Splits use slots 0..2. Restore the original vertex cache so
                 * subsequent authored triangles retain their exact bindings. */
                for (k=0;k<16;k++) { if (mapped[k]) joined[k]=*mapped[k]; else memset(joined+k,0,sizeof(Vtx)); }
                gSPVertex((*color)++,osVirtualToPhysical(joined),16,0);
                if (*depth) { gSPVertex((*depth)++,osVirtualToPhysical(joined),16,0); }
                joined+=16;
            }
            continue;
        }
        if (op == G_SETTIMG) {
            s32 next = armSkinImage(command.words.w1, matchSkinTone ? part : 0);
            image=command.words.w1;
            if (next != skin) { armSkinMaterial(color, next, light); skin = next; }
        }
        *(*color)++ = command;
        if (*depth && (op == G_VTX || op == (u8)G_TRI1 || op == (u8)G_TRI4)) *(*depth)++ = command;
    }
    if (skin) armSkinMaterial(color, FALSE, light);
}

s32 portArmMeshBuildJoinedSurface(PortArmMesh *mesh, s32 cuff, s32 bond, s32 parts,
    PortArmMap map, void *context, f32 light, u8 alpha, PortArmJoin join,PortArmSurface surface)
{
    ModelFileHeader *header = armSource();
    ModelNode *nodes[ARM_PARTS], *hand, *node;
    s32 kinds[ARM_PARTS];
    coord3d origins[ARM_PARTS];
    f32 angles[ARM_PARTS] = {0};
    s32 lengths[ARM_PARTS], count = 0, commands = 1, skinCommands = 0, i;
    s32 nativeMaterial=(parts&PORT_ARM_NATIVE_SKIN)!=0;
    s32 matchSkinTone=!nativeMaterial && !surface;
    Gfx *color, *depth;
    if (!header || !map) return FALSE;
    hand = armFirstDisplay(header->RootNode);
    if (!hand) return FALSE;
    if (parts & PORT_ARM_HAND) { kinds[count] = PORT_ARM_HAND; nodes[count++] = hand; }
    if (parts & PORT_ARM_FOREARM) {
        kinds[count] = PORT_ARM_FOREARM;
        nodes[count++] = armFirstDisplay(header->Switches[armCuffSwitch[portWardrobeVariant(cuff,bond)]]);
    }
    if (parts & PORT_ARM_WATCH) {
        s32 budget = 16;
        node = hand->Next;
        while (node && (node->Opcode & 255) != MODELNODE_OPCODE_DL && budget--) node = node->Next;
        if (!node || !budget) return FALSE;
        kinds[count] = PORT_ARM_WATCH; nodes[count++] = node;
        kinds[count] = PORT_ARM_WATCH; nodes[count++] = armFirstDisplay(header->Switches[3]);
        for (i = 0; i < 3; i++) { kinds[count] = PORT_ARM_WATCH; nodes[count++] = armFirstDisplay(header->Switches[i]); }
    }
    if (!count || count > ARM_PARTS) return FALSE;
    for (i = 0; i < count; i++) {
        ModelNode *parent;
        memset(&origins[i], 0, sizeof(origins[i]));
        if (!nodes[i] || !nodes[i]->Data || (nodes[i]->Opcode & 255) != MODELNODE_OPCODE_DL) return FALSE;
        lengths[i] = armListLength(&nodes[i]->Data->DisplayList);
        if (!lengths[i]) {
            static s32 reported;
            if (!reported++ && getenv("GE_ARM_AUDIT")) portConLogf("ge: arm-mesh: rejected part %d vertices=%d\n", i, nodes[i]->Data->DisplayList.numVertices);
            return FALSE;
        }
        commands += lengths[i];
        if (kinds[i]==PORT_ARM_FOREARM) {
            const ArmCuffCap *cap=armCuff(&nodes[i]->Data->DisplayList,lengths[i]);
            if (cap && cap->count) commands+=cap->count*2+1;
        }
        if (join && kinds[i]==PORT_ARM_FOREARM) {
            s32 extra,vertices; armJoinCounts(&nodes[i]->Data->DisplayList,lengths[i],join,context,&extra,&vertices);
            commands+=extra;
        }
        skinCommands += armSkinCommands(&nodes[i]->Data->DisplayList, lengths[i], matchSkinTone ? kinds[i] : 0);
        parent = nodes[i]->Parent;
        if (parent && (parent->Opcode & 255) == MODELNODE_OPCODE_GROUPSIMPLE) {
            s32 clock;
            origins[i] = parent->Data->GroupSimple.Origin;
            for (clock = 0; clock < 3; clock++) if (parent == header->Switches[clock]) {
                /* Same watch clock as the pause animation, rotating in its
                 * authored local plane before retargeting to the wrist. */
                s32 frames = (s32)watch_time_0;
                if(portHandGalleryActive()) frames=0;
                f32 seconds = (frames%3600)/60.0f;
                f32 minutes = (frames/3600)%60;
                f32 secondAngle = -seconds*M_TAU_F/60;
                f32 minuteAngle = -minutes*M_TAU_F/60+secondAngle/60;
                angles[i] = clock == 2 ? secondAngle : clock == 1 ? minuteAngle
                    : -((frames/216000)%12)*M_TAU_F/12+minuteAngle/12+secondAngle/720;
            }
        }
    }
    mesh->color = color = dynAllocate((commands+count+skinCommands)*sizeof(Gfx));
    mesh->depth = depth = alpha < 255 ? dynAllocate(commands*sizeof(Gfx)) : NULL;
    for (i = 0; i < count; i++) armCopyPart(&nodes[i]->Data->DisplayList, lengths[i],
        &origins[i], angles[i], kinds[i], map, context, light, alpha, &color, &depth, kinds[i]==PORT_ARM_FOREARM ? join : NULL,NULL,matchSkinTone,surface,nativeMaterial);
    gSPEndDisplayList(color++);
    if (depth) gSPEndDisplayList(depth++);
    if (getenv("GE_ARM_AUDIT")) {
        static u32 seen[6];
        s32 variant = portWardrobeVariant(cuff, bond);
        if (!(seen[variant] & (1u << parts))) {
            seen[variant] |= 1u << parts;
            portConLogf("ge: arm-mesh: wardrobe=%d parts=%d meshes=%d commands=%d\n", variant, parts, count, commands);
        }
    }
    return TRUE;
}

s32 portArmMeshBuildJoined(PortArmMesh *mesh,s32 cuff,s32 bond,s32 parts,
    PortArmMap map,void *context,f32 light,u8 alpha,PortArmJoin join)
{
    return portArmMeshBuildJoinedSurface(mesh,cuff,bond,parts,map,context,light,alpha,join,NULL);
}

s32 portArmMeshBuild(PortArmMesh *mesh, s32 cuff, s32 bond, s32 parts,
    PortArmMap map, void *context, f32 light, u8 alpha)
{
    return portArmMeshBuildJoined(mesh,cuff,bond,parts,map,context,light,alpha,NULL);
}

s32 portArmMeshBuildFiltered(PortArmMesh *mesh,const ModelRoData_DisplayListRecord *source,
    PortArmMap map,void *context,f32 light,u8 alpha,PortArmKeepTriangle keep,s32 matchSkinTone)
{
    s32 length=armListLength(source),commands,i; Gfx *color,*depth; coord3d origin={0};
    s32 part=matchSkinTone ? PORT_ARM_HAND : 0;
    if (!length || !map || !keep) return FALSE;
    commands=length+armSkinCommands(source,length,part);
    for (i=0;i<length;i++) if ((source->Primary[i].words.w0>>24)==(u8)G_TRI4) commands+=3;
    mesh->color=color=dynAllocate(commands*sizeof(Gfx));
    mesh->depth=depth=alpha<255 ? dynAllocate(commands*sizeof(Gfx)) : NULL;
    armCopyPart(source,length,&origin,0,part,map,context,light,alpha,&color,&depth,NULL,keep,TRUE,NULL,FALSE);
    gSPEndDisplayList(color++); if (depth) { gSPEndDisplayList(depth++); }
    return TRUE;
}
