/* Fog (FORK-074): turn the level's distance fog off.
 *
 * With Fog off, fogLoadCurrentEnvironment takes the fogless path the
 * GE_NOFOG diagnostic already used: no fog colour on the level, the sky
 * drawn unfogged and no distance fade on characters and props. The far clip
 * and the fog radius guards see within (g_ScaledFarFogIntensity, derived
 * before that path) are unchanged, so guards' sight and the visible distance
 * stay as they were. Changing it reloads the level's environment on the next
 * MoveBond tick (portFogReapply). Levels without fog are unaffected. */
#ifndef PORT_FOG_H
#define PORT_FOG_H

#include <ultra64.h>

#define PORT_FOG_DEFAULT 1

s32 portFogOptionGet(const char *key, s32 *value);
s32 portFogOptionSet(const char *key, s32 value);
/* bgfog.c fogLoadCurrentEnvironment: whether to load with fog, remembered as
 * the state applied. */
s32 portFogApply(void);
/* port_noclip.c tick: the setting differs from what the level was loaded with. */
s32 portFogNeedsReload(void);

#endif
