/* ADS Spread and ADS Recoil (FORK-045). gunfire.c keeps the native spread and
 * recoil math; these only scale the weapon stats it reads while in ADS. */
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "port_ads.h"
#include "port_ads_accuracy.h"

extern s32 get_is_ramrom_flag(void);

static s32 adsSpread = PORT_ADS_ACCURACY_DEFAULT;
static s32 adsRecoil = PORT_ADS_ACCURACY_DEFAULT;

static s32 *adsAccuracyOption(const char *key)
{
    if (strcmp(key, "ads_spread") == 0) return &adsSpread;
    if (strcmp(key, "ads_recoil") == 0) return &adsRecoil;
    return NULL;
}

s32 portAdsAccuracyOptionGet(const char *key, s32 *value)
{
    s32 *option = adsAccuracyOption(key);
    if (option == NULL) return FALSE;
    *value = *option;
    return TRUE;
}

s32 portAdsAccuracyOptionSet(const char *key, s32 value)
{
    s32 *option = adsAccuracyOption(key);
    if (option == NULL) return FALSE;
    *option = value < 0 ? 0 : value > 100 ? 100 : value;
    return TRUE;
}

f32 portAdsAccuracyScale(s32 percent, s32 item, f32 blend)
{
    /* The shotguns' spread is their design; they also keep the hip view in ADS. */
    if (item == ITEM_SHOTGUN || item == ITEM_AUTOSHOT) return 1.0f;
    return 1.0f + ((f32) percent / 100.0f - 1.0f) * blend;
}

/* Demos replay recorded input through the native spread, so they stay unscaled. */
static f32 adsAccuracyLive(s32 percent, s32 item)
{
    if (get_is_ramrom_flag() != 0) return 1.0f;
    return portAdsAccuracyScale(percent, item, portAdsBlend());
}

f32 portAdsSpreadScale(s32 item) { return adsAccuracyLive(adsSpread, item); }
f32 portAdsRecoilScale(s32 item) { return adsAccuracyLive(adsRecoil, item); }
