#ifndef PORT_WARDROBE_H
#define PORT_WARDROBE_H
#include <ultra64.h>
#include <bondtypes.h>
enum { PORT_SKIN, PORT_SLEEVE, PORT_CUFF, PORT_MATERIALS };
typedef struct {
    sImageTableEntry material[PORT_MATERIALS];
    f32 sleeveEnd, cuffEnd;
    s32 rolled;
} PortWardrobe;
PortWardrobe portWardrobeStyle(s32 cuff, s32 folderBond);
s32 portWardrobeVariant(s32 cuff, s32 folderBond);
s32 portWardrobeCuff(s32 variant);
/* Scoped to native first-person hands, never world/NPC models. */
void portWardrobeBegin(s32 item);
void portWardrobeEnd(void);
s32 portWardrobeRenderNode(ModelRenderData *renderdata, ModelNode *node);
#endif
