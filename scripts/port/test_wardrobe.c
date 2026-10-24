/* Synthetic authored meshes: no game geometry, textures or ROM required. */
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../../port/src/port_wardrobe.c"
#include "../../port/src/port_arm_mesh.c"
#include "../../port/src/port_native_arms.c"
extern int strcmp(const char *, const char *);
static struct player player;
struct player *g_CurrentPlayer = &player;
ModelFileHeader suit_lf_hand_header = {.numSwitches=10};
s32 watch_time_0;
static ModelNode meshNodes[12], selectors[10], *switches[10];
static union ModelRoData meshData[12], selectorData[10];
static Vtx source[12][5], original[12][5];
static Gfx lists[12][11];
static union { u64 align; u8 bytes[131072]; } arena;
static s32 used, copies, loads;
static Vtx *copied[16];
static struct tex skinTextures[2];
struct texpool *portModelTexPool(void) { static struct texpool pool; return &pool; }
struct tex *texFindInPool(s32 id, struct texpool *pool) {
    assert(pool==portModelTexPool());
    if (id==1793) return &skinTextures[0];
    if (id==1796) return &skinTextures[1];
    return NULL;
}
void osSyncPrintf(const char *fmt, ...) {
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args); abort();
}
void portConLogf(const char *fmt, ...) { (void)fmt; }
void *dynAllocate(s32 size) { void *out=arena.bytes+used; used+=(size+15)&~15; assert(used<sizeof(arena)); return out; }
Vtx *dynAllocateVertices(s32 count) { Vtx *out=dynAllocate(count*sizeof(Vtx)); assert(copies<16); copied[copies++]=out; return out; }
u32 osVirtualToPhysical(void *p) { return (u32)p; }
void *portAllocHigh(u32 size) { static u8 model[65536]; assert(size<=sizeof(model)); return model; }
s32 fileGetBondForCurrentFolder(void) { return BOND_CONNERY; }
void load_object_fill_header(ModelFileHeader *header, u8 *name, u8 *dst, s32 size, struct texpool *pool)
{ assert(!strcmp((char *)name,"Csuit_lf_handZ")); header->RootNode=meshNodes; header->Switches=switches; ++loads; }
static void identity(const coord3d *s, coord3d *d, s32 part, void *context) { *d=*s; }
static void reset(void) { used=copies=0; }
static s32 images(const Gfx *dl, s32 *ids) {
    s32 n=0, i;
    Gfx skin, normal;
    gDPSetCombineLERP(&skin,TEXEL0,0,SHADE,PRIMITIVE,TEXEL0,0,SHADE,0,
        TEXEL0,0,SHADE,PRIMITIVE,TEXEL0,0,SHADE,0);
    gDPSetCombineMode(&normal,G_CC_MODULATEIA,G_CC_MODULATEIA);
    for (i=0;i<256;i++) {
        u32 op=dl[i].words.w0>>24;
        if (op==(u8)G_ENDDL) return n;
        if (op==G_SETTIMG) ids[n++]=dl[i].words.w1;
        if (op==G_SETCOMBINE) assert(!memcmp(dl+i,&skin,sizeof(Gfx)) || !memcmp(dl+i,&normal,sizeof(Gfx)));
        assert(op!=G_MTX && op!=(u8)G_SETOTHERMODE_L
            && op!=(u8)G_SETGEOMETRYMODE && op!=(u8)G_CLEARGEOMETRYMODE);
        if (op==(u8)G_TEXTURE) assert(((dl[i].words.w0>>11)&7)==0);
        if (op==(u8)G_SETOTHERMODE_H) assert(((dl[i].words.w0>>8)&255)!=G_MDSFT_TEXTLOD);
    }
    assert(0); return 0;
}
static void fixture(void) {
    s32 i,j;
    skinTextures[0].data=(u8 *)1001; skinTextures[1].data=(u8 *)1002;
    for (i=0;i<12;i++) {
        for (j=0;j<3;j++) {
            source[i][j].v.ob[0]=-180-j*100;
            source[i][j].v.ob[1]=j*30;
            source[i][j].v.ob[2]=j*20;
            source[i][j].v.tc[0]=100+i*13+j; source[i][j].v.tc[1]=200+i*13+j;
            memset(source[i][j].v.cn,200,4);
        }
        if (i>=1 && i<=6) {
            source[i][2].v.ob[0]=-3707;
            source[i][3]=source[i][2]; source[i][4]=source[i][2];
            source[i][3].v.ob[1]=-30; source[i][3].v.ob[2]=-20;
            source[i][4].v.ob[1]=-20; source[i][4].v.ob[2]=60;
        }
        meshNodes[i].Opcode=MODELNODE_OPCODE_DL; meshNodes[i].Data=&meshData[i];
        meshData[i].DisplayList.Primary=lists[i];
        meshData[i].DisplayList.Vertices=(Vertex *)source[i];
        meshData[i].DisplayList.BaseAddr=source[i]; meshData[i].DisplayList.numVertices=i>=1 && i<=6 ? 5 : 3;
        gSPMatrix(&lists[i][0],0x03000000,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
        gDPSetTextureLOD(&lists[i][1],G_TL_LOD);
        gDPSetCombineMode(&lists[i][2],G_CC_SHADE,G_CC_SHADE);
        gSPTexture(&lists[i][3],65535,65535,5,0,G_ON);
        gDPSetTextureImage(&lists[i][4],G_IM_FMT_RGBA,G_IM_SIZ_16b,1,1000+i);
        gSPVertex(&lists[i][5],0x05000000,3,0);
        gSP1Triangle(&lists[i][6],0,1,2,0);
        gSPSetGeometryMode(&lists[i][7],G_CULL_BACK);
        gSPClearGeometryMode(&lists[i][8],G_LIGHTING);
        gDPSetRenderMode(&lists[i][9],G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);
        gSPEndDisplayList(&lists[i][10]);
    }
    /* A palm outlier sits in the old proximal band but not on the arm seam. */
    source[0][2].v.ob[1]=-400; source[0][2].v.ob[2]=-400;
    memcpy(original,source,sizeof(source));
    for (i=0;i<10;i++) {
        selectors[i].Opcode=MODELNODE_OPCODE_SWITCH; selectors[i].Data=&selectorData[i]; switches[i]=selectors+i;
        selectors[i].Child=meshNodes+(i<3 ? 9+i : i==3 ? 8 : i-3);
        selectors[i].Child->Parent=selectors+i;
    }
    meshNodes[0].Next=selectors+4;
    for (i=4;i<9;i++) selectors[i].Next=selectors+i+1;
    selectors[9].Next=meshNodes+7;
}
int main(void) {
    static const s32 selected[6]={4,1,5,6,2,3}; /* mesh IDs reached by actual cuff switches */
    static const s32 items[]={ITEM_FIST,ITEM_WPPK,ITEM_WPPKSIL,ITEM_TT33,ITEM_RUGER,ITEM_GOLDENGUN,ITEM_SILVERWPPK,ITEM_GOLDWPPK};
    PortArmMesh mesh;
    s32 variant,i,j,ids[16];
    Gfx nativeList[4],output[32];
    Vtx nativeVertices[7]={{{{-20,-20,-200},0,{0,0},{255,255,255,255}}},
        {{{20,20,0},0,{512,512},{180,180,180,255}}},{{{-20,-20,0},0,{0,512},{220,220,220,255}}},
        {{{40,30,-200},0,{0,0},{255,255,255,255}}},{{{-40,-30,-200},0,{0,0},{255,255,255,255}}},
        {{{20,-20,0},0,{0,0},{255,255,255,255}}},{{{-20,20,0},0,{0,0},{255,255,255,255}}}};
    union ModelRoData nativeData={0}; ModelNode nativeNode={.Data=&nativeData};
    ModelRenderData render={.flags=1,.gdl=output};
    fixture();
    { coord3d low,high;
      assert(portArmWristBounds(&low,&high));
      assert(low.x==-280 && high.x==-180 && low.y==0 && high.y==30 && low.z==0 && high.z==20); }
    { /* Material switches brighten only skin; finish with neutral state. */
        Gfx mixed[8], *color, *depth=NULL, expected;
        ModelRoData_DisplayListRecord data=meshData[0].DisplayList;
        s32 prims=0, combos=0, n;
        gDPSetTextureImage(&mixed[0],G_IM_FMT_RGBA,G_IM_SIZ_16b,1,1001);
        gSPVertex(&mixed[1],0x05000000,3,0);
        gSP1Triangle(&mixed[2],0,1,2,0);
        gDPSetTextureImage(&mixed[3],G_IM_FMT_RGBA,G_IM_SIZ_16b,1,1040);
        gSP1Triangle(&mixed[4],0,1,2,0);
        gDPSetTextureImage(&mixed[5],G_IM_FMT_RGBA,G_IM_SIZ_16b,1,1002);
        gSP1Triangle(&mixed[6],0,1,2,0); gSPEndDisplayList(&mixed[7]);
        data.Primary=mixed;
        assert(armSkinCommands(&data,8,PORT_ARM_HAND)==12);
        assert(armSkinCommands(&data,8,PORT_ARM_WATCH)==0);
        for (n=0;n<2;n++) {
            f32 light=n ? 1 : 0.5f;
            coord3d origin={0};
            reset(); color=dynAllocate((8+12)*sizeof(Gfx)); mesh.color=color;
            armCopyPart(&data,8,&origin,0,PORT_ARM_HAND,identity,NULL,light,127,&color,&depth,NULL);
            assert(color==mesh.color+19); gSPEndDisplayList(color);
            assert(images(mesh.color,ids)==3 && ids[0]==1001 && ids[1]==1040 && ids[2]==1002);
            prims=combos=0;
            for (i=0;i<19;i++) {
                u32 op=mesh.color[i].words.w0>>24;
                if (op==G_SETPRIMCOLOR) {
                    s32 skin=(prims++%2)==0;
                    gDPSetPrimColor(&expected,0,0,skin ? (u8)(28*light):0,
                        skin ? (u8)(25*light):0,skin ? (u8)(22*light):0,0);
                    assert(!memcmp(mesh.color+i,&expected,sizeof(Gfx)));
                }
                if (op==G_SETCOMBINE) ++combos;
            }
            assert(prims==4 && combos==4 && copied[0][0].v.cn[3]==127);
            /* The final two commands restore primitive zero and normal sampling. */
            assert(mesh.color[17].words.w1==0);
            gDPSetCombineMode(&expected,G_CC_MODULATEIA,G_CC_MODULATEIA);
            assert(!memcmp(mesh.color+18,&expected,sizeof(Gfx)));
        }
        reset(); color=dynAllocate(8*sizeof(Gfx)); mesh.color=color;
        armCopyPart(&data,8,&(coord3d){0},0,PORT_ARM_WATCH,identity,NULL,1,255,&color,&depth,NULL);
        assert(color==mesh.color+7 && !memcmp(original,source,sizeof(source)));
    }
    { /* The broader wrist blends into the palm and a moderated forearm. */
        s32 x;
        assert(portArmForearmBulk(-180)==1.25f);
        assert(portArmForearmBulk(-800)==1.25f);
        assert(portArmForearmBulk(650)==1 && portArmForearmBulk(-3707)==1);
        for (x=-4000;x<1600;x++) {
            f32 bulk=portArmForearmBulk(x), next=portArmForearmBulk(x+1);
            assert(bulk>=1 && bulk<=1.25f);
            assert(next-bulk<0.002f && next-bulk>-0.002f);
        }
    }
    for (variant=0;variant<6;variant++) {
        reset();
        assert(portWardrobeVariant(portWardrobeCuff(variant),BOND_CONNERY)==variant);
        assert(portArmMeshBuild(&mesh,portWardrobeCuff(variant),BOND_CONNERY,7,identity,NULL,0.5f,127));
        assert(copies==7 && images(mesh.color,ids)==7 && ids[0]==1000 && ids[1]==1000+selected[variant]);
        assert(ids[2]==1007 && ids[3]==1008 && ids[4]==1009 && ids[6]==1011);
        for (i=0;i<7;i++) for (j=0;j<3;j++) {
            s32 src=i==0 ? 0 : i==1 ? selected[variant] : i+5;
            assert(!memcmp(copied[i][j].v.ob,source[src][j].v.ob,6));
            assert(!memcmp(copied[i][j].v.tc,source[src][j].v.tc,4));
            assert(copied[i][j].v.cn[0]==100 && copied[i][j].v.cn[3]==127);
        }
        for (i=0;(mesh.depth[i].words.w0>>24)!=(u8)G_ENDDL;i++) {
            u32 op=mesh.depth[i].words.w0>>24;
            assert(op==G_VTX || op==(u8)G_TRI1 || op==(u8)G_TRI4);
        }
        assert(i==14 && !memcmp(original,source,sizeof(source)));
    }
    assert(loads==1); /* Persistent source is loaded once; wardrobe is never cached. */
    reset(); selectors[2].Opcode=MODELNODE_OPCODE_GROUPSIMPLE;
    selectorData[2].GroupSimple.Origin=(coord3d){10,20,30}; watch_time_0=900;
    assert(portArmMeshBuild(&mesh,CUFF_BLUE,BOND_BROSNAN,PORT_ARM_WATCH,identity,NULL,1,255));
    assert(copies==5 && !mesh.depth); /* Opaque draws need no duplicate depth list. */
    assert(abs(copied[4][0].v.ob[0]-10)<=1 && copied[4][0].v.ob[1]==20 && abs(copied[4][0].v.ob[2]+150)<=1);
    selectors[2].Opcode=MODELNODE_OPCODE_SWITCH; watch_time_0=0;
    assert(portWardrobeVariant(CUFF_FOLDER,BOND_CONNERY)==5 && portWardrobeVariant(CUFF_FOLDER,BOND_BROSNAN)==4);
    reset(); lists[0][5].words.w1+=16;
    assert(!portArmMeshBuild(&mesh,CUFF_BLUE,BOND_BROSNAN,3,identity,NULL,1,255) && used==0);
    lists[0][5].words.w1-=16;
    nativeData.DisplayList.Primary=nativeList; nativeData.DisplayList.Vertices=(Vertex *)nativeVertices;
    nativeData.DisplayList.BaseAddr=nativeVertices; nativeData.DisplayList.numVertices=7;
    gSPMatrix(&nativeList[0],0x03000040,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPVertex(&nativeList[1],0x05000000,3,0); gSP1Triangle(&nativeList[2],0,1,2,0); gSPEndDisplayList(&nativeList[3]);
    player.tileColor.r=player.tileColor.g=player.tileColor.b=255;
    for (variant=0;variant<6;variant++) for (i=0;i<8;i++) for (j=0;j<2;j++) {
        reset(); player.bondtype=portWardrobeCuff(variant); render.gdl=output;
        nativeVertices[1].v.ob[2]=items[i]==ITEM_FIST ? 50 : 0;
        nativeVertices[2].v.ob[2]=items[i]==ITEM_FIST ? 47 : -3;
        nativeVertices[5].v.ob[2]=nativeVertices[6].v.ob[2]=items[i]==ITEM_FIST ? 50 : 0;
        { /* Width/depth fit actual native joints independently of arm length. */
            NativeArmPose fit; coord3d a,b,pa,pb;
            assert(paNativeFit(&nativeData.DisplayList,-200,items[i]==ITEM_FIST ? 50 : 0,portWardrobeCuff(variant),BOND_CONNERY,&fit));
            a=fit.sourceMin; b=fit.sourceMax;
            paNativePoint(&a,&pa,PORT_ARM_FOREARM,&fit);
            paNativePoint(&b,&pb,PORT_ARM_FOREARM,&fit);
            assert(abs((int)(pa.x-fit.targetMax.x))<=1 && abs((int)(pb.x-fit.targetMin.x))<=1);
            assert(abs((int)(pa.y-fit.targetMin.y))<=1 && abs((int)(pb.y-fit.targetMax.y))<=1);
            assert(pa.z==fit.targetMin.z && pb.z==fit.targetMax.z);
            a=fit.sourceElbowMin; b=fit.sourceElbowMax;
            paNativePoint(&a,&pa,PORT_ARM_FOREARM,&fit); paNativePoint(&b,&pb,PORT_ARM_FOREARM,&fit);
            { f32 w=1.06f*(fit.targetElbowMax.x-fit.targetElbowMin.x);
              f32 h=1.06f*(fit.targetElbowMax.y-fit.targetElbowMin.y);
              assert(pa.x-pb.x-w<0.001f && pa.x-pb.x-w>-0.001f);
              assert(pb.y-pa.y-h<0.001f && pb.y-pa.y-h>-0.001f); }
            { coord3d lo,hi;
              assert(portArmSectionBounds(portWardrobeCuff(variant),BOND_CONNERY,-900,&lo,&hi));
              assert(lo.y>12 && lo.y<13 && hi.y>35 && hi.y<36);
              assert(lo.z>8 && lo.z<9 && hi.z>23 && hi.z<24); }

            a=(coord3d){fit.watchCenter,0,0}; b=a; b.x+=20;
            paNativePoint(&a,&pa,PORT_ARM_WATCH,&fit); paNativePoint(&b,&pb,PORT_ARM_WATCH,&fit);
            { f32 length=pb.z-pa.z; b=a; b.z+=20;
              paNativePoint(&b,&pb,PORT_ARM_WATCH,&fit);
              assert(length-(pb.y-pa.y)<0.001f && length-(pb.y-pa.y)>-0.001f); }
        }
        portWardrobeBeginHand(items[i],j);
        assert(portWardrobeRenderNode(&render,&nativeNode));
        assert(copies==(j==GUNLEFT ? 6 : 1)); /* Joined forearm; watch only on left. */
        assert(output[1].words.w1==0x03000040 && render.gdl<output+32);
        assert(copied[0][0].v.ob[2]==(items[i]==ITEM_FIST ? 50 : 0));
        portWardrobeEnd(); assert(!portWardrobeRenderNode(&render,&nativeNode));
    }
    { /* Stitch a different native polygon outline without changing its hand,
       * adding skin patches or losing material/vertex-cache state. */
        Vtx ring[15]={0},saved[15];
        ModelRoData_DisplayListRecord data={0}; NativeArmPose fit;
        PortArmJoinPoint forward[32],reverse[32];
        coord3d a,b,mapped; s32 count,n,seen[32]={0};
        for (i=0;i<12;i++) {
            ring[i].v.ob[0]=(s16)(20*cosf(i*M_TAU_F/12));
            ring[i].v.ob[1]=(s16)(30*sinf(i*M_TAU_F/12));
            ring[i].v.ob[2]=i%3;
        }
        for (i=12;i<15;i++) {
            ring[i].v.ob[0]=(i-13)*30; ring[i].v.ob[1]=(i-13)*40;
            ring[i].v.ob[2]=-200;
        }
        memcpy(saved,ring,sizeof(ring));
        data.Vertices=(Vertex *)ring; data.numVertices=15;
        assert(paNativeFit(&data,-200,2,CUFF_BLUE,BOND_BROSNAN,&fit));
        assert(fit.ringCount==12); fit.stitch=TRUE;
        a=(coord3d){-180,0,0}; b=(coord3d){-280,30,20};
        count=paWristJoin(&a,&b,forward,&fit); n=paWristJoin(&b,&a,reverse,&fit);
        assert(count>0 && count==n);
        for (i=0;i<count;i++) {
            assert(forward[i].fraction>0 && forward[i].fraction<1);
            if (i) assert(forward[i].fraction>forward[i-1].fraction);
            assert(fabsf(forward[i].fraction+reverse[count-i-1].fraction-1)<.0001f);
            assert(!memcmp(&forward[i].target,&reverse[count-i-1].target,sizeof(coord3d)));
        }
        reset(); assert(portArmMeshBuildJoined(&mesh,CUFF_BLUE,BOND_BROSNAN,PORT_ARM_FOREARM,paNativePoint,&fit,1,255,paWristJoin));
        assert(copies==1 && !mesh.depth && images(mesh.color,ids)==1);
        for (i=0;(mesh.color[i].words.w0>>24)!=(u8)G_ENDDL;i++) if ((mesh.color[i].words.w0>>24)==G_VTX) {
            const Vtx *v=(const Vtx *)(uintptr_t)mesh.color[i].words.w1;
            s32 length=(mesh.color[i].words.w0&65535)/sizeof(Vtx),c;
            for (j=0;j<length;j++) for (c=0;c<count;c++) if (v[j].v.ob[0]==(s16)forward[c].target.x
                && v[j].v.ob[1]==(s16)forward[c].target.y && v[j].v.ob[2]==(s16)forward[c].target.z) seen[c]=1;
            if (length==16) for (j=0;j<3;j++) assert(!memcmp(v+j,copied[0]+j,sizeof(Vtx)));
        }
        for (i=0;i<count;i++) assert(seen[i]);
        paWristProject(&fit,&a,&mapped);
        assert(mapped.z>=0 && mapped.z<=2); /* Preserve the native seam's tilt. */
        { /* A complete seven-sided source ring must include all twelve native
           * corners exactly, including the angular wrap in either direction. */
            coord3d savedWrist[32]; s32 savedCount=armWristCount,covered[32]={0},splits=0;
            memcpy(savedWrist,armWrist,sizeof(armWrist)); armWristCount=7;
            for (i=0;i<7;i++) armWrist[i]=(coord3d){-180,15-15*cosf(i*M_TAU_F/7),10+10*sinf(i*M_TAU_F/7)};
            for (i=0;i<7;i++) {
                a=armWrist[i]; b=armWrist[(i+1)%7];
                paWristProject(&fit,&a,&mapped);
                for (j=0;j<12;j++) if (!memcmp(&mapped,fit.ring+j,sizeof(mapped))) covered[j]=1;
                count=paWristJoin(&a,&b,forward,&fit); n=paWristJoin(&b,&a,reverse,&fit);
                assert(count==n); splits+=count;
                for (j=0;j<count;j++) {
                    assert(!memcmp(&forward[j].target,&reverse[count-j-1].target,sizeof(coord3d)));
                    for (n=0;n<12;n++) if (!memcmp(&forward[j].target,fit.ring+n,sizeof(coord3d))) covered[n]=1;
                }
            }
            for (i=0;i<12;i++) assert(covered[i]);
            assert(splits>0); memcpy(armWrist,savedWrist,sizeof(armWrist)); armWristCount=savedCount;
        }
        assert(!memcmp(saved,ring,sizeof(ring)) && !memcmp(original,source,sizeof(source)));
    }
    { /* Close the nearest clothing rim, not skin or the elbow. The cap uses
       * the existing edge positions/UVs and also enters the depth-only pass. */
        Vtx tube[12]={0}; Gfx dl[9],*color,*depth;
        ModelRoData_DisplayListRecord data={0}; ArmCuffCap cap={0},savedCap;
        for (i=0;i<12;i++) {
            tube[i].v.ob[0]=i<6 ? -700 : -2000;
            tube[i].v.ob[1]=(s16)(200*cosf(i*M_TAU_F/6));
            tube[i].v.ob[2]=(s16)(200*sinf(i*M_TAU_F/6));
            tube[i].v.tc[0]=i%6*128; tube[i].v.tc[1]=i<6 ? 512 : 0;
            memset(tube[i].v.cn,255,4);
        }
        data.Primary=dl; data.Vertices=(Vertex *)tube; data.BaseAddr=tube; data.numVertices=12;
        gDPSetTextureImage(&dl[0],G_IM_FMT_RGBA,G_IM_SIZ_16b,1,1050);
        gSPVertex(&dl[1],0x05000000,12,0);
        for (i=0;i<6;i++) { s32 next=(i+1)%6;
            gSP2Triangles(&dl[i+2],i,next,i+6,0,next,next+6,i+6,0);
        }
        gSPEndDisplayList(&dl[8]); armFindCuff(&data,9,&cap);
        assert(cap.count==6 && cap.image==1050 && cap.center.v.ob[0]==-700);
        for (i=0;i<6;i++) {
            assert(cap.a[i].v.ob[0]==-700 && cap.b[i].v.ob[0]==-700);
            assert(cap.a[i].v.tc[1]==512 && cap.b[i].v.tc[1]==512);
        }
        savedCap=armCuffs[0]; cap.key=dl; armCuffs[0]=cap;
        reset(); color=dynAllocate(22*sizeof(Gfx)); depth=dynAllocate(22*sizeof(Gfx));
        mesh.color=color; mesh.depth=depth;
        armCopyPart(&data,9,&(coord3d){0},0,PORT_ARM_FOREARM,identity,NULL,.5f,127,&color,&depth,NULL);
        gSPEndDisplayList(color); gSPEndDisplayList(depth);
        assert(color==mesh.color+21 && images(mesh.color,ids)==1 && ids[0]==1050);
        assert(depth==mesh.depth+20);
        for (i=0;i<20;i++) { u32 op=mesh.depth[i].words.w0>>24;
            assert(op==G_VTX || op==(u8)G_TRI1 || op==(u8)G_TRI4);
        }
        { const Vtx *v=(const Vtx *)(uintptr_t)mesh.color[1].words.w1;
          assert(v[0].v.cn[0]==127 && v[0].v.cn[3]==127); }
        armCuffs[0]=savedCap;
        gDPSetTextureImage(&dl[0],G_IM_FMT_RGBA,G_IM_SIZ_16b,1,1001);
        armFindCuff(&data,9,&cap); assert(cap.count==0);
    }
    portWardrobeBegin(ITEM_AK47); assert(!portWardrobeRenderNode(&render,&nativeNode));
    portWardrobeBegin(ITEM_WPPK); nativeList[1].words.w1+=128;
    assert(!portWardrobeRenderNode(&render,&nativeNode));
    assert(!memcmp(original,source,sizeof(source)));
    puts("Wardrobe checks passed: six outfits, shared wrist bounds, stitched irregular native wrists, closed clothing rims, skin-only light correction/state reset, UV/alpha/source preservation, left-only watch, native bones, depth-only lists and invalid vertex rejection.");
    return 0;
}
