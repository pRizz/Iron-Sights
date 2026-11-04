/* Included by test_ads.c: ADS Spread and ADS Recoil scale the native weapon
 * stats by the ADS blend, leave shotguns and demos alone, and bridge to menus. */
static s32 ramromFlag;
s32 get_is_ramrom_flag(void) { return ramromFlag; }

static s32 nearlyEqual(f32 a, f32 b) { return a - b < 0.0001f && b - a < 0.0001f; }

static void testAdsAccuracy(void)
{
    s32 value;
    const s32 opt = portOptFind("ads_spread");

    /* Both settings default to half and bridge through the menus' option table. */
    assert(opt >= 0 && portOptFind("ads_recoil") >= 0 && portOptCategoryOf(opt) == PORTOPT_CAT_GAMEPLAY);
    assert(portJoyPortOptionGet("ads_spread", &value) && value == PORT_ADS_ACCURACY_DEFAULT);
    assert(portJoyPortOptionGet("ads_recoil", &value) && value == PORT_ADS_ACCURACY_DEFAULT);
    assert(portJoyPortSetOption("ads_recoil", "130") && portJoyPortOptionGet("ads_recoil", &value) && value == 100);
    assert(portJoyPortOptionSet("ads_recoil", -5) && portJoyPortOptionGet("ads_recoil", &value) && value == 0);
    assert(portJoyPortOptionSet("ads_recoil", PORT_ADS_ACCURACY_DEFAULT));
    assert(!portAdsAccuracyOptionGet("aim_mode", &value));

    /* The multiplier eases from native at the hip to the setting once settled. */
    assert(nearlyEqual(portAdsAccuracyScale(50, ITEM_AK47, 0.0f), 1.0f));
    assert(nearlyEqual(portAdsAccuracyScale(50, ITEM_AK47, 0.5f), 0.75f));
    assert(nearlyEqual(portAdsAccuracyScale(50, ITEM_AK47, 1.0f), 0.5f));
    assert(nearlyEqual(portAdsAccuracyScale(0, ITEM_WPPK, 1.0f), 0.0f));
    assert(nearlyEqual(portAdsAccuracyScale(100, ITEM_WPPK, 1.0f), 1.0f));
    assert(portAdsAccuracyScale(50, ITEM_SHOTGUN, 1.0f) == 1.0f);
    assert(portAdsAccuracyScale(50, ITEM_AUTOSHOT, 1.0f) == 1.0f);

    /* Live scaling follows the current player's ADS blend, except in demos. */
    selectPlayer(0);
    assert(portAdsSpreadScale(ITEM_WPPKSIL) == 1.0f);
    sAdsBlend[0] = 1.0f;
    assert(nearlyEqual(portAdsSpreadScale(ITEM_WPPKSIL), 0.5f));
    assert(nearlyEqual(portAdsRecoilScale(ITEM_WPPKSIL), 0.5f));
    selectPlayer(1);
    assert(portAdsSpreadScale(ITEM_WPPKSIL) == 1.0f); /* player 2 is not aiming */
    selectPlayer(0);
    ramromFlag = 1;
    assert(portAdsSpreadScale(ITEM_WPPKSIL) == 1.0f);
    ramromFlag = 0;
    sAdsBlend[0] = 0.0f;
}
