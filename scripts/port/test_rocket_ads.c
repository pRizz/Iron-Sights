/* Included by test_ads.c: both sight pins stack on the centered ray while the
 * tube cants about it onto the right shoulder, at the launcher's hip depth and
 * unzoomed view. The pose origin arrives already rolled (portAdsWeaponPose);
 * the gun body rolls with it in the renderer. */
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
    const coord3d rearAxis = {-3,127,-532};
    coord3d pose, pin, forwardPin, rim, tube, rear;
    f32 pitch, roll;
    s32 rate, tick;

    /* Act: use the same pose, pitch and roll supplied to the game renderer. */
    assert(portAdsWeaponPose(ITEM_ROCKETLAUNCH, &muzzle, &pose));
    pitch = portAdsWeaponPitch(ITEM_ROCKETLAUNCH);
    roll = portAdsWeaponRoll(ITEM_ROCKETLAUNCH);

    /* Assert: both pins sit on the center ray, so they line up at the
     * crosshair. The launcher keeps its hip depth (WeaponStats PosZ -14.5)
     * rather than travelling forward, and the canted tube runs down and right
     * onto the shoulder: as at the hip, its rear passes below and right of the
     * eye, behind the camera. ADS keeps the unzoomed field of view. */
    assert(roll > 0.2f && roll < 0.6f && pose.z == -14.5f);
    pin = rocketAdsViewPoint(nearPin, pose, pitch, roll);
    forwardPin = rocketAdsViewPoint(farPin, pose, pitch, roll);
    rim = rocketAdsViewPoint(rearRim, pose, pitch, roll);
    tube = rocketAdsViewPoint(muzzle, pose, pitch, roll);
    rear = rocketAdsViewPoint(rearAxis, pose, pitch, roll);
    assert(fabsf(pin.x) < 0.005f && fabsf(pin.y) < 0.005f && pin.z < -10.0f);
    assert(fabsf(forwardPin.x) < 0.005f && fabsf(forwardPin.y) < 0.005f && forwardPin.z < -50.0f);
    assert(tube.x > 1.5f && tube.y < -4.0f);
    assert(rim.x > 0.3f && rim.y < -1.0f && rim.z > 0.0f);
    assert(rear.x > 2.0f && rear.y < -6.0f && rear.z > 0.0f);
    assert(portAdsWeaponFovScale(ITEM_ROCKETLAUNCH) == 1.0f && portAdsWeaponFovScale(ITEM_AK47) == 0.75f);
    /* The spread-firing shotguns share the launcher's unzoomed view. */
    assert(portAdsWeaponFovScale(ITEM_SHOTGUN) == 1.0f && portAdsWeaponFovScale(ITEM_AUTOSHOT) == 1.0f);
    assert(portAdsWeaponFovScale(ITEM_GRENADELAUNCH) > 0.75f && portAdsWeaponFovScale(ITEM_GRENADELAUNCH) < 1.0f);
    /* Other guns are held upright. */
    assert(portAdsWeaponRoll(ITEM_AK47) == 0.0f && portAdsWeaponRoll(ITEM_WPPK) == 0.0f);
    {   /* The support hand keeps its unrolled orientation and offset from the
         * tube axis, so it stays under the launcher relative to the ground. */
        const Mtxf base = {.m={{-1,0,0,0},{0,1,0,0},{0,0,-1,0},{1,-6,-14.5f,1}}};
        const f32 c = cosf(roll), s = sinf(roll);
        Mtxf rolled = base, held;
        coord3d trigger, grip, axis = sAdsRocketTubeAxis, a, p, a0, p0;
        s32 k;
        for (k = 0; k < 4; k++) {
            rolled.m[k][0] = base.m[k][0]*c - base.m[k][1]*s;
            rolled.m[k][1] = base.m[k][0]*s + base.m[k][1]*c;
        }
        assert(portAdsHandGrips(ITEM_ROCKETLAUNCH, &trigger, &grip));
        axis.z = grip.z;
        adsCounterRoll(&held, &rolled, axis, -roll);
        for (k = 0; k < 3; k++) {
            assert(fabsf(held.m[k][0] - base.m[k][0]) < 1e-4f && fabsf(held.m[k][1] - base.m[k][1]) < 1e-4f);
            assert(held.m[k][2] == base.m[k][2]);
        }
        a = adsHandPoint(&rolled, &axis); p = adsHandPoint(&held, &grip);
        a0 = adsHandPoint(&base, &axis); p0 = adsHandPoint(&base, &grip);
        assert(fabsf((p.x - a.x) - (p0.x - a0.x)) < 1e-3f && fabsf((p.y - a.y) - (p0.y - a0.y)) < 1e-3f);
        assert(p.y < a.y);
    }

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
    /* The launcher's raise takes 250 ms and eases out onto the pins. */
    g_GlobalTimerDelta = 1.0f;
    pollAds(0, 1);
    for (tick = 0; tick < 7; tick++) portAdsTick();
    assert(portAdsBlend() > 0.84f && portAdsBlend() < 0.86f); /* raw 7/15 */
    for (tick = 0; tick < 9; tick++) portAdsTick();
    assert(portAdsBlend() == 1.0f);
    pollAds(0, 0);
    for (tick = 0; tick < 16; tick++) portAdsTick();
    assert(portAdsBlend() == 0.0f);
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
    puts("Rocket ADS checks passed: stacked centered pins, shoulder cant at hip depth, level support hand, eased raise, no zoom, aim modes, tank/dual/reload gates and four tick rates.");
}
