#ifndef PORT_ADS_H
#define PORT_ADS_H

#include <ultra64.h>

/* All queries refer to the current player. */
s32 portAdsActive(void);
f32 portAdsBlend(void);
void portAdsTick(void);
s32 portAdsUpdateAim(s32 held, s32 pressed, s32 toggle);
struct MoveData;
void portAdsConfigureMovement(struct MoveData *move, s32 digitalX, s32 digitalY);
struct coord3d;
s32 portAdsWeaponPose(s32 item, const struct coord3d *muzzle, struct coord3d *pose);
f32 portAdsWeaponPitch(s32 item);

#endif
