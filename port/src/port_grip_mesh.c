/* The original PP7's closed hand supplies coherent rifle grip geometry.
 * Only skin-textured triangles are used; no weapon geometry is included. */
#include <ultra64.h>
#include <bondtypes.h>
#include <stdint.h>
#include <stdlib.h>
#include "game/image.h"
#include "game/objecthandler.h"
#include "port_arm_mesh.h"
#include "port_console.h"

s32 portArmGripSkin(u32 image)
{
    extern struct texpool *portModelTexPool(void);
    static u32 pixels[6]; s32 i;
    for (i=0;i<6;i++) {
        if (!pixels[i]) {
            struct tex *tex=texFindInPool(1793+i,portModelTexPool());
            if (tex) pixels[i]=(u32)(uintptr_t)tex->data;
        }
        if (pixels[i] && pixels[i]==image) return TRUE;
    }
    return FALSE;
}

const PortGripSource *portArmGripSource(void)
{
    extern ModelFileHeader wppk_header;
    extern void *portAllocHigh(u32 size);
    static ModelFileHeader header;
    static u8 *buffer;
    static PortGripSource grip;
    static s32 ready;
    ModelNode *stack[128]; s32 used=0,budget=128;
    if (buffer) return ready ? &grip : NULL;
    buffer=portAllocHigh(0x10000);
    if (!buffer) return NULL;
    header=wppk_header;
    load_object_fill_header(&header,(u8 *)"GwppkZ",buffer,0x10000,NULL);
    if (!header.RootNode) return NULL;
    stack[used++]=header.RootNode;
    while (used && budget--) {
        ModelNode *node=stack[--used];
        if (node->Next && used<128) stack[used++]=node->Next;
        if (node->Child && used<128) stack[used++]=node->Child;
        if ((node->Opcode&255)==MODELNODE_OPCODE_DL && node->Data) {
            const ModelRoData_DisplayListRecord *data=&node->Data->DisplayList;
            s32 i,skin=FALSE,ended=FALSE,gripBone=TRUE,minZ=32767,maxZ=-32768;
            if (!data->Primary || !data->Vertices || !data->numVertices || data->numVertices>1024) continue;
            for (i=0;i<4096;i++) {
                u32 op=data->Primary[i].words.w0>>24;
                if (op==(u8)G_ENDDL) { ended=TRUE; break; }
                if (op==G_SETTIMG && portArmGripSkin(data->Primary[i].words.w1)) skin=TRUE;
                /* Other bones contain the reload hand/loose magazine. The
                 * stationary grip is authored in the main gun bone only. */
                if (op==G_MTX && data->Primary[i].words.w1!=0x03000000) gripBone=FALSE;
            }
            if (!ended || !skin || !gripBone) continue;
            if (grip.count==24) return NULL;
            grip.parts[grip.count++]=data;
            for (i=0;i<data->numVertices;i++) {
                s32 z=data->Vertices[i].coord.z;
                if (z<minZ) minZ=z; if (z>maxZ) maxZ=z;
            }
            if (minZ<-150 && maxZ<=50) grip.arm=data;
        }
    }
    if (getenv("GE_ARM_AUDIT")) portConLogf("ge: closed-grip: parts=%d arm=%p budget=%d\n",grip.count,grip.arm,budget);
    ready=budget>=0 && grip.arm && grip.count;
    return ready ? &grip : NULL;
}
