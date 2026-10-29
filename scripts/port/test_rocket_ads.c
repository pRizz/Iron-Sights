/* Included by test_ads.c: the near aiming pin needs clear background. */
static coord3d rocketAdsViewPoint(coord3d p, coord3d pose, f32 pitch)
{
    return (coord3d){
        -p.x*0.1f + pose.x,
        (p.y*cosf(pitch) - p.z*sinf(pitch))*0.1f + pose.y,
        -(p.y*sinf(pitch) + p.z*cosf(pitch))*0.1f + pose.z
    };
}

static void testRocketAds(void)
{
    /* Arrange: measured native muzzle and the two textured sight pins.
     * Model X/Z face away from the camera in the settled weapon basis. */
    const coord3d muzzle = {-3.076880f, 127.545708f, 436.002136f};
    const coord3d nearPin = {-3,200,25}, farPin = {-3,190,399};
    /* Native loaded rocket's upper shoulder: muzzle plus (0,62,152).
     * Its upper silhouette previously overlapped the centered pins. */
    const coord3d noseTop = {-3.076880f,189.545708f,588.002136f};
    const coord3d rearRim = {-3,196,-532};
    const s32 rates[] = {30,60,120,240};
    coord3d pose, pin, forwardPin, nose, rim;
    f32 pitch;
    s32 rate, tick;

    /* Act: use the same pose and pitch supplied to the game renderer. */
    assert(portAdsWeaponPose(ITEM_ROCKETLAUNCH, &muzzle, &pose));
    pitch = portAdsWeaponPitch(ITEM_ROCKETLAUNCH);

    /* Assert: near pin stays on the ray, with clear air above both the far
     * pin and loaded nose; the rear tube also remains below it and ahead of us.
     * Normalized clearance checks remain meaningful across aspect/FOV changes. */
    pin = rocketAdsViewPoint(nearPin, pose, pitch);
    forwardPin = rocketAdsViewPoint(farPin, pose, pitch);
    nose = rocketAdsViewPoint(noseTop, pose, pitch);
    rim = rocketAdsViewPoint(rearRim, pose, pitch);
    assert(fabsf(pin.x) < 0.001f && fabsf(pin.y) < 0.002f && pin.z < -50.0f);
    assert(forwardPin.z < -50.0f && nose.z < -50.0f);
    assert(forwardPin.y / -forwardPin.z < -0.007f);
    assert(nose.y / -nose.z < -0.006f);
    assert(rim.y < -0.5f && rim.z < -5.0f);

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
    puts("Rocket ADS checks passed: centered pin, nose/tube clearance, aim modes, tank/dual/reload gates and four tick rates.");
}
