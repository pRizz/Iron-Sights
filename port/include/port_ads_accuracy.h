#ifndef PORT_ADS_ACCURACY_H
#define PORT_ADS_ACCURACY_H
#include <ultra64.h>

/* ADS Spread and ADS Recoil (FORK-045): while aiming down sights, a weapon's
 * native bullet spread and visual recoil kick shrink to a percentage of their
 * hip values, eased in with the ADS blend. Shotguns keep their pellet spread
 * and kick, and demo playback keeps the native values. */
#define PORT_ADS_ACCURACY_DEFAULT 50

s32 portAdsAccuracyOptionGet(const char *key, s32 *value);
s32 portAdsAccuracyOptionSet(const char *key, s32 value);
/* Multiplier for `percent` of the native value at an ADS blend of 0 (hip) to 1 (settled). */
f32 portAdsAccuracyScale(s32 percent, s32 item, f32 blend);
/* gunfire.c: the current player's spread (Inaccuracy) and recoil (RecoilBack/RecoilUp) multipliers. */
f32 portAdsSpreadScale(s32 item);
f32 portAdsRecoilScale(s32 item);

#endif
