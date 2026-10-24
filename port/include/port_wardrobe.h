#ifndef PORT_WARDROBE_H
#define PORT_WARDROBE_H
#include <ultra64.h>
#include <bondtypes.h>
s32 portWardrobeVariant(s32 cuff, s32 folderBond);
s32 portWardrobeCuff(s32 variant);
/* Scoped to native first-person hands, never world/NPC models. */
void portWardrobeBegin(s32 item);
void portWardrobeBeginHand(s32 item, s32 hand);
void portWardrobeEnd(void);
s32 portWardrobeRenderNode(ModelRenderData *renderdata, ModelNode *node);
#endif
