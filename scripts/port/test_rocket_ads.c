/* Included by test_ads.c: the shouldered launcher keeps its tube beside the
 * face while its front converges on clear background at the aim point. */
static coord3d rocketAdsViewPoint(coord3d p, coord3d pose, f32 pitch, f32 yaw)
{
    const f32 x = -p.x*0.1f, z = -(p.y*sinf(pitch) + p.z*cosf(pitch))*0.1f;
    return (coord3d){
        x*cosf(yaw) + z*sinf(yaw) + pose.x,
        (p.y*cosf(pitch) - p.z*sinf(pitch))*0.1f + pose.y,
        -x*sinf(yaw) + z*cosf(yaw) + pose.z
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
    f32 pitch, yaw;
    s32 rate, tick;

    /* Act: use the same pose and pitch supplied to the game renderer. */
    assert(portAdsWeaponPose(ITEM_ROCKETLAUNCH, &muzzle, &pose));
    pitch = portAdsWeaponPitch(ITEM_ROCKETLAUNCH);
    yaw = portAdsWeaponYaw(ITEM_ROCKETLAUNCH);

    /* Assert: the shouldered tube sits right of the eye and turns in, so the
     * far pin and loaded nose converge on the aim point with clear air above
     * them; the rear rim passes beside the face, not in front of the eye.
     * Normalized checks remain meaningful across aspect/FOV changes. */
    assert(yaw > 0.05f && yaw < 0.2f && pose.x > 3.0f);
    pin = rocketAdsViewPoint(nearPin, pose, pitch, yaw);
    forwardPin = rocketAdsViewPoint(farPin, pose, pitch, yaw);
    nose = rocketAdsViewPoint(noseTop, pose, pitch, yaw);
    rim = rocketAdsViewPoint(rearRim, pose, pitch, yaw);
    assert(pin.x > 3.0f && pin.z < -50.0f);
    assert(forwardPin.z < -50.0f && nose.z < -50.0f);
    assert(fabsf(forwardPin.x / -forwardPin.z) < 0.02f && fabsf(nose.x / -nose.z) < 0.02f);
    assert(forwardPin.y / -forwardPin.z < -0.006f);
    assert(nose.y / -nose.z < -0.005f);
    assert(rim.x > 8.0f && rim.z < -5.0f);

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
    puts("Rocket ADS checks passed: shouldered tube, converging nose/pin clearance, aim modes, tank/dual/reload gates and four tick rates.");
}
