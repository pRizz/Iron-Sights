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
/* Radians about the view axis, applied with the ADS pitch; the settled pose
 * origin is rolled by the same angle, keeping on-axis sights centered. */
f32 portAdsWeaponRoll(s32 item);
/* Ease an item's native hip rotation (XYZ radians: the taser and watch laser)
 * to its ADS orientation as the blend rises, each angle the shorter way round. */
void portAdsBlendRotation(s32 item, struct coord3d *angles, f32 blend);
/* Largest ADS field of view, as a fraction of the default (1 = no zoom). */
f32 portAdsWeaponFovScale(s32 item);
s32 portAdsHandGrips(s32 item, struct coord3d *trigger, struct coord3d *support);
/* Preserve the selected gun pass's XY projection when reserving near depth. */
Gfx *portAdsBeginHandLayer(Gfx *gdl, const Mtxf *gunProjection);
/* Reload the scene projection; G_ZBUFFER stays on for later split views. */
Gfx *portAdsEndHandLayer(Gfx *gdl, Mtx *sceneProjection);
Gfx *portAdsDrawHands(Gfx *gdl, s32 item, const Mtxf *gun);
Gfx *portAdsDrawHandsBehindGun(Gfx *gdl, s32 item, const Mtxf *gun);
/* Brackets a long gun's own draw in the hand layer: stock painter's order with
 * depth writes kept for the arms drawn after it (FORK-075, G_NOOP 0x64). */
Gfx *portAdsGunPainterTag(Gfx *gdl, s32 on);

/* Hip-Fire Arms (FORK-075, [display] hip_arms, 0 off by default): the long
 * guns' generated ADS arms also show at the hip, opaque and riding rigidly
 * with the gun in their settled ADS shape. Off, they fade in with the ADS
 * blend as before. Dual wielding keeps the right gun's trigger hand while the
 * support arm slides down out of view (and back once the left gun goes). */
#define PORT_HIP_ARMS_DEFAULT 0
s32 portAdsHandsOptionGet(const char *key, s32 *value);
s32 portAdsHandsOptionSet(const char *key, s32 value);
/* Per player tick, every tick (dual wield blocks the ADS tick's later steps). */
void portAdsHandsTick(void);
/* The support hold after `ticks`: towards 0 while dual wielding, else 1. */
f32 portAdsSupportHoldStep(f32 hold, s32 dual, f32 ticks);
/* The arms' opacity for the current player's right gun: 0 = not drawn. */
f32 portAdsHandAlpha(s32 item);
static inline f32 portAdsHandAlphaFor(s32 hasGrips, s32 hipArms, f32 blend)
{
    if (!hasGrips) return 0.0f;
    return hipArms ? 1.0f : blend;
}

/* Isolated deterministic review harness; always false in ordinary gameplay. */
s32 portHandGalleryActive(void);
s32 portHandGalleryStock(void);
/* The gallery's hip-arms cells turn Hip-Fire Arms on for that capture only. */
s32 portHandGalleryHipArms(void);

#endif
