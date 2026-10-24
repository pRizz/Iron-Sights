#ifndef PORT_WARDROBE_H
#define PORT_WARDROBE_H
#include <ultra64.h>
#include <bondtypes.h>
#include "port_arm_mesh.h"
typedef struct { coord3d wrist,elbow; } PortGripSpace;
typedef void (*PortGripMap)(const coord3d *source,coord3d *target,s32 part,const PortGripSpace *space,void *context);
s32 portWardrobeBuildGrip(PortArmMesh *mesh,s32 cuff,s32 bond,s32 left,f32 light,u8 alpha,PortGripMap map,void *context);
s32 portWardrobeVariant(s32 cuff, s32 folderBond);
s32 portWardrobeCuff(s32 variant);
/* Native pistol hands and added sleeves must occlude by depth, not draw order. */
s32 portWardrobeDepthRequired(s32 item);
/* Scoped to native first-person hands, never world/NPC models. */
void portWardrobeBegin(s32 item);
void portWardrobeBeginHand(s32 item, s32 hand);
void portWardrobeEnd(void);
enum { PORT_WARDROBE_PASS, PORT_WARDROBE_DRAWN, PORT_WARDROBE_NATIVE_HAND };
/* NATIVE_HAND supplies a triangle-filtered record for the original renderer.
 * Stock hand vertices/materials stay untouched; only proximal arm triangles
 * are replaced, with their UV/shade transferred to the fitted wrist surface. */
s32 portWardrobeRenderNode(ModelRenderData *renderdata, ModelNode *node,
    ModelRoData_DisplayListRecord *nativeHand);
#endif
