#ifndef PORT_KNIFE_ADS_H
#define PORT_KNIFE_ADS_H
#include <ultra64.h>
#include <bondtypes.h>

/* 0: stock blade grip/butt sight (default); 1: handle grip/tip sight. */
s32 portKnifeAdsOptionGet(const char *key, s32 *value);
s32 portKnifeAdsOptionSet(const char *key, s32 value);
void portKnifeAdsReset(void);
void portKnifeAdsTick(s32 player, s32 available, f32 blend, f32 delta);
void portKnifeAdsPose(s32 hand, s32 item, const Mtxf *basis, coord3d *origin);
void portKnifeAdsDraw(ModelRenderData *renderdata, Model *model, s32 item, s32 hand);
/* Isolated real-throw/transition audit; inert without GE_ADS_AUDIT=3. */
s32 portKnifeAdsAudit(s32 available, s32 *aim);
s32 portKnifeAdsAuditTrigger(s32 normal);

#endif
