#ifndef PORT_ADS_H
#define PORT_ADS_H

#include <ultra64.h>
#include <bondtypes.h>

/* All queries refer to the current player. */
s32 portAdsActive(void);
f32 portAdsBlend(void);
/* Sniper scope transition; zero for other weapons or unavailable controls. */
f32 portAdsScopeBlend(void);
/* Mouse input belongs to player 1, regardless of who ticked last. */
s32 portAdsP1SniperActive(void);
/* Returns true when the wheel was consumed by sniper ADS. */
s32 portAdsMouseWheel(s32 notches);
f32 portAdsWheelZoom(f32 fov, f32 maxFov);
void portAdsTick(void);
s32 portAdsUpdateAim(s32 held, s32 pressed, s32 toggle);
struct MoveData;
void portAdsConfigureMovement(struct MoveData *move, s32 digitalX, s32 digitalY);
struct coord3d;
s32 portAdsWeaponPose(s32 item, const struct coord3d *muzzle, struct coord3d *pose);
f32 portAdsWeaponPitch(s32 item);
s32 portAdsHandGrips(s32 item, struct coord3d *trigger, struct coord3d *support);
Gfx *portAdsBeginHandLayer(Gfx *gdl);
Gfx *portAdsDrawHands(Gfx *gdl, s32 item, const Mtxf *gun);
/* Isolated deterministic review harness; always false in ordinary gameplay. */
s32 portHandGalleryActive(void);
s32 portHandGalleryStock(void);

#endif
