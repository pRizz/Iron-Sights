/* Included by test_ads.c: both sight pins stack on the centered ray while the
 * tube cants about it onto the right shoulder. The pose origin arrives already
 * rolled (portAdsWeaponPose); the gun body rolls with it in the renderer. */
static coord3d rocketAdsViewPoint(coord3d p, coord3d pose, f32 pitch, f32 roll)
{
    const f32 x = -p.x*0.1f, y = (p.y*cosf(pitch) - p.z*sinf(pitch))*0.1f;
    return (coord3d){
        x*cosf(roll) - y*sinf(roll) + pose.x,
        x*sinf(roll) + y*cosf(roll) + pose.y,
        -(p.y*sinf(pitch) + p.z*cosf(pitch))*0.1f + pose.z
    };
}

static void testRocketAds(void)
{
    /* Arrange: measured native muzzle and the two textured sight pins.
     * Model X/Z face away from the camera in the settled weapon basis. */
    const coord3d muzzle = {-3.076880f, 127.545708f, 436.002136f};
    const coord3d nearPin = {-3,200,25}, farPin = {-3,190,399};
    const coord3d rearRim = {-3,196,-532};
    const s32 rates[] = {30,60,120,240};
    coord3d pose, pin, forwardPin, rim, tube;
    f32 pitch, roll;
    s32 rate, tick;

    /* Act: use the same pose, pitch and roll supplied to the game renderer. */
    assert(portAdsWeaponPose(ITEM_ROCKETLAUNCH, &muzzle, &pose));
    pitch = portAdsWeaponPitch(ITEM_ROCKETLAUNCH);
    roll = portAdsWeaponRoll(ITEM_ROCKETLAUNCH);

    /* Assert: both pins sit on the center ray, so they line up at the
     * crosshair; the canted tube runs down and right onto the shoulder, with
     * its rear rim below and right of the eye and still ahead of it. */
    assert(roll > 0.2f && roll < 0.6f);
    pin = rocketAdsViewPoint(nearPin, pose, pitch, roll);
    forwardPin = rocketAdsViewPoint(farPin, pose, pitch, roll);
    rim = rocketAdsViewPoint(rearRim, pose, pitch, roll);
    tube = rocketAdsViewPoint(muzzle, pose, pitch, roll);
    assert(fabsf(pin.x) < 0.005f && fabsf(pin.y) < 0.005f && pin.z < -50.0f);
    assert(fabsf(forwardPin.x) < 0.005f && fabsf(forwardPin.y) < 0.005f && forwardPin.z < -50.0f);
    assert(tube.x > 1.5f && tube.y < -4.0f);
    assert(rim.x > 0.3f && rim.y < -1.0f && rim.z < -5.0f);
    /* Other guns are held upright. */
    assert(portAdsWeaponRoll(ITEM_AK47) == 0.0f && portAdsWeaponRoll(ITEM_WPPK) == 0.0f);

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
    puts("Rocket ADS checks passed: stacked centered pins, shoulder cant, aim modes, tank/dual/reload gates and four tick rates.");
}
