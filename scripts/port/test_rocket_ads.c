/* Included by test_ads.c: the native sight openings must share the aim ray. */
static void testRocketAds(void)
{
    /* Arrange: measured native muzzle and the two textured sight openings.
     * Model X/Z face away from the camera in the settled weapon basis. */
    const coord3d muzzle = {-3.076880f, 127.545708f, 436.002136f};
    const coord3d sights[] = {{-3,200,25}, {-3,190,399}};
    const s32 rates[] = {30,60,120,240};
    coord3d pose;
    f32 pitch;
    s32 i, rate, tick;

    /* Act: use the same pose and pitch supplied to the game renderer. */
    assert(portAdsWeaponPose(ITEM_ROCKETLAUNCH, &muzzle, &pose));
    pitch = portAdsWeaponPitch(ITEM_ROCKETLAUNCH);

    /* Assert: both openings project onto the center, at positive eye distance.
     * This catches relative sight/pitch drift as well as a misplaced assembly. */
    for (i = 0; i < 2; i++) {
        const f32 x = -sights[i].x * 0.1f + pose.x;
        const f32 y = (sights[i].y*cosf(pitch) - sights[i].z*sinf(pitch))*0.1f + pose.y;
        const f32 z = -(sights[i].y*sinf(pitch) + sights[i].z*cosf(pitch))*0.1f + pose.z;
        assert(fabsf(x) < 0.001f && fabsf(y) < 0.002f && z < -50.0f);
    }
    /* The tube's rearmost authored vertex must not cross the eye plane. */
    assert(pose.z + 53.2f < -5.0f);

    /* Arrange/act/assert: normal hold/toggle aim and the dedicated binding all
     * admit the launcher, while tank/dual/reload retain their existing gates. */
    selectPlayer(0);
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_ROCKETLAUNCH;
    normalAim(1, 1, 0);
    assert(portAdsActive() && !g_CurrentPlayer->insightaimmode);
    normalAim(0, 0, 0);
    assert(!portAdsActive());
    normalAim(1, 1, 1);
    normalAim(0, 0, 1);
    assert(portAdsActive());
    normalAim(1, 1, 1);
    assert(!portAdsActive());
    assert(portJoyPortOptionSet("aim_mode", 0));
    normalAim(1, 1, 0);
    assert(g_CurrentPlayer->insightaimmode && !portAdsActive());
    pollAds(0, 1);
    assert(portAdsActive());
    pollAds(0, 0);
    normalAim(0, 0, 0);
    assert(portJoyPortOptionSet("aim_mode", 1));
    for (rate = 0; rate < 4; rate++) {
        g_GlobalTimerDelta = 60.0f / rates[rate];
        pollAds(0, 1);
        for (tick = 0; tick < rates[rate]; tick++) portAdsTick();
        assert(portAdsBlend() == 1.0f && portAdsScopeBlend() == 0.0f);
        g_PlayerIsInTank = 1;
        assert(!portAdsActive());
        g_PlayerIsInTank = 0;
        g_CurrentPlayer->hand_item[GUNLEFT] = ITEM_ROCKETLAUNCH;
        assert(!portAdsActive());
        g_CurrentPlayer->hand_item[GUNLEFT] = ITEM_UNARMED;
        g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_RELOAD_LOWER;
        assert(!portAdsActive());
        g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_IDLE;
        pollAds(0, 0);
        for (tick = 0; tick < rates[rate]; tick++) portAdsTick();
        assert(portAdsBlend() == 0.0f);
    }
    portStateResetInput();
    selectPlayer(0);
    puts("Rocket ADS checks passed: native sight ray, eye clearance, aim modes, tank/dual/reload gates and four tick rates.");
}
