/* Included by test_ads.c: Weapon Kick (FORK-061) springs each shot, settles at
 * any tick rate, caps the climb, owns the native kick only while it is on,
 * scales with ADS Recoil, and Ludicrous and Cartoon kick far harder. */
static f32 kickAbs(f32 value) { return value < 0.0f ? -value : value; }

/* Largest value `channel` reaches in `seconds` after one shot from rest. */
static f32 kickPeakAfterShot(const PortGunKickProfile *profile, s32 channel, f32 seconds)
{
    PortGunKickState state;
    f32 peak = 0.0f;
    f32 t;

    memset(&state, 0, sizeof(state));
    portGunKickImpulse(&state, profile);
    for (t = 0.0f; t < seconds; t += 0.0005f)
    {
        portGunKickStep(&state, profile, 0.0005f);
        if (kickAbs(state.pos[channel]) > peak) peak = kickAbs(state.pos[channel]);
    }
    return peak;
}

static void testGunKickOption(void)
{
    s32 value;

    /* Enhanced by default, a gameplay row, values clamped to Original..Cartoon. */
    assert(portOptFind("weapon_kick") >= 0 && portOptCategoryOf(portOptFind("weapon_kick")) == PORTOPT_CAT_GAMEPLAY);
    assert(portJoyPortOptionGet("weapon_kick", &value) && value == PORT_GUN_KICK_ENHANCED);
    assert(portJoyPortSetOption("weapon_kick", "2") && portJoyPortOptionGet("weapon_kick", &value) && value == PORT_GUN_KICK_LUDICROUS);
    assert(portJoyPortSetOption("weapon_kick", "3") && portJoyPortOptionGet("weapon_kick", &value) && value == PORT_GUN_KICK_CARTOON);
    assert(portJoyPortSetOption("weapon_kick", "9") && portJoyPortOptionGet("weapon_kick", &value) && value == PORT_GUN_KICK_CARTOON);
    assert(portJoyPortOptionSet("weapon_kick", -1) && portJoyPortOptionGet("weapon_kick", &value) && value == 0);
    assert(portJoyPortOptionSet("weapon_kick", 0) && portJoyPortOptionGet("weapon_kick", &value) && value == 0);
    assert(portJoyPortOptionSet("weapon_kick", PORT_GUN_KICK_DEFAULT));
    assert(!portGunKickOptionGet("ads_recoil", &value));
}

static void testGunKickProfiles(void)
{
    /* Firearms and launchers kick; blades, the taser and lasers stay native. */
    assert(portGunKickMaybeProfile(ITEM_AK47) && portGunKickMaybeProfile(ITEM_FNP90));
    assert(portGunKickMaybeProfile(ITEM_WPPK) && portGunKickMaybeProfile(ITEM_RUGER));
    assert(portGunKickMaybeProfile(ITEM_SHOTGUN) && portGunKickMaybeProfile(ITEM_SKORPION));
    assert(!portGunKickMaybeProfile(ITEM_KNIFE) && !portGunKickMaybeProfile(ITEM_TASER));
    assert(portGunKickMaybeProfile(ITEM_ROCKETLAUNCH) && portGunKickMaybeProfile(ITEM_GRENADELAUNCH));
    assert(!portGunKickMaybeProfile(ITEM_LASER) && !portGunKickMaybeProfile(ITEM_WATCHLASER));
    assert(!portGunKickMaybeProfile(ITEM_UNARMED));
    /* A magnum shot kicks harder than a pistol shot, which kicks harder than one SMG round. */
    assert(portGunKickMaybeProfile(ITEM_RUGER)->pitch > portGunKickMaybeProfile(ITEM_WPPK)->pitch);
    assert(portGunKickMaybeProfile(ITEM_WPPK)->pitch > portGunKickMaybeProfile(ITEM_UZI)->pitch);
    /* Shotguns and launchers shove harder than any pistol. */
    assert(portGunKickMaybeProfile(ITEM_SHOTGUN)->back > portGunKickMaybeProfile(ITEM_TT33)->back);
    assert(portGunKickMaybeProfile(ITEM_GRENADELAUNCH)->back > portGunKickMaybeProfile(ITEM_TT33)->back);
    assert(portGunKickMaybeProfile(ITEM_ROCKETLAUNCH)->back > portGunKickMaybeProfile(ITEM_TT33)->back);
}

static void testGunKickLudicrous(void)
{
    const PortGunKickProfile *rifle = portGunKickMaybeProfile(ITEM_AK47);
    PortGunKickProfile big, cartoon, same;

    /* Enhanced is the table itself. */
    portGunKickExaggerate(rifle, PORT_GUN_KICK_ENHANCED, &same);
    assert(memcmp(&same, rifle, sizeof(same)) == 0);

    /* Cartoon: 5.5x turns, twice the travel, a softer spring than Ludicrous. */
    portGunKickExaggerate(rifle, PORT_GUN_KICK_CARTOON, &cartoon);
    assert(nearlyEqual(cartoon.pitch, rifle->pitch * 5.5f) && nearlyEqual(cartoon.climbMax, rifle->climbMax * 5.5f));
    assert(nearlyEqual(cartoon.back, rifle->back * 2.0f));

    /* Ludicrous: turns and climb triple, travel grows by half, the spring softens. */
    portGunKickExaggerate(rifle, PORT_GUN_KICK_LUDICROUS, &big);
    assert(cartoon.omega < big.omega);
    assert(nearlyEqual(big.pitch, rifle->pitch * 3.0f) && nearlyEqual(big.roll, rifle->roll * 3.0f));
    assert(nearlyEqual(big.climbMax, rifle->climbMax * 3.0f) && nearlyEqual(big.climbStep, rifle->climbStep * 3.0f));
    assert(nearlyEqual(big.back, rifle->back * 1.5f) && nearlyEqual(big.rise, rifle->rise * 1.5f));
    assert(big.omega < rifle->omega && big.damping == rifle->damping);
    /* A Ludicrous shot still peaks at its own profile's value. */
    assert(kickAbs(kickPeakAfterShot(&big, KICK_RISE, 0.4f) - big.rise) < big.rise * 0.02f);
}

static void testGunKickSpring(void)
{
    const PortGunKickProfile *rifle = portGunKickMaybeProfile(ITEM_AK47);
    PortGunKickState coarse, fine;
    s32 i;

    /* One shot from rest peaks at the profile's value (rise has no random spread). */
    assert(kickAbs(kickPeakAfterShot(rifle, KICK_RISE, 0.3f) - rifle->rise) < rifle->rise * 0.02f);

    /* The same motion at 60 and 240 ticks a second. */
    memset(&coarse, 0, sizeof(coarse));
    portGunKickImpulse(&coarse, rifle);
    fine = coarse;
    portGunKickStep(&coarse, rifle, 1.0f / 60.0f);
    for (i = 0; i < 4; i++) portGunKickStep(&fine, rifle, 1.0f / 240.0f);
    for (i = 0; i < KICK_CHANNELS; i++) assert(kickAbs(coarse.pos[i] - fine.pos[i]) < 0.0005f);

    /* Half a second later the gun is back at rest, and a zero step changes nothing. */
    for (i = 0; i < 30; i++) portGunKickStep(&coarse, rifle, 1.0f / 60.0f);
    assert(kickAbs(coarse.pos[KICK_BACK]) < rifle->back * 0.01f);
    assert(kickAbs(coarse.pos[KICK_PITCH]) < rifle->pitch * 0.01f);
    fine = coarse;
    portGunKickStep(&fine, rifle, 0.0f);
    assert(memcmp(&fine, &coarse, sizeof(fine)) == 0);
}

static void testGunKickClimb(void)
{
    const PortGunKickProfile *rifle = portGunKickMaybeProfile(ITEM_AK47);
    PortGunKickState state;
    s32 shot, tick;

    /* Ten rounds a second for ten seconds: climb builds, stays under its cap. */
    memset(&state, 0, sizeof(state));
    for (shot = 0; shot < 100; shot++)
    {
        portGunKickImpulse(&state, rifle);
        for (tick = 0; tick < 6; tick++) portGunKickStep(&state, rifle, 1.0f / 60.0f);
        assert(state.climbTarget < rifle->climbMax && state.climb < rifle->climbMax);
    }
    assert(state.climb > rifle->climbStep);

    /* It settles once the trigger is let go. */
    for (tick = 0; tick < 120; tick++) portGunKickStep(&state, rifle, 1.0f / 60.0f);
    assert(state.climb < 0.05f);
}

static void testGunKickHooks(void)
{
    PortGunKickPose hip, ads;
    s32 tick;

    selectPlayer(0);
    sAdsBlend[0] = 0.0f;

    /* Enhanced owns the kick of profiled weapons only. */
    assert(portGunKickNativeScale(ITEM_AK47) == 0.0f);
    assert(portGunKickNativeScale(ITEM_LASER) == 1.0f);

    /* A shot shows on the drawn gun; ADS Recoil (50%) halves it once settled. */
    portGunKickFire(GUNRIGHT, ITEM_AK47);
    for (tick = 0; tick < 2; tick++) assert(portGunKickAdvance(GUNRIGHT, ITEM_AK47, 1.0f, &hip));
    assert(hip.z > 0.0f && hip.pitch > 0.0f);
    sAdsBlend[0] = 1.0f;
    assert(portGunKickAdvance(GUNRIGHT, ITEM_AK47, 0.0f, &ads));
    assert(nearlyEqual(ads.z, hip.z * 0.5f) && nearlyEqual(ads.pitch, hip.pitch * 0.5f));
    sAdsBlend[0] = 0.0f;

    /* Changing weapon drops the old spring. */
    assert(portGunKickAdvance(GUNRIGHT, ITEM_UZI, 0.0f, &hip) && hip.z == 0.0f && hip.pitch == 0.0f);

    /* Ludicrous draws the same shot (same fresh spring and seed) far bigger.
     * Switching weapon and back resets the spring to an identical start. */
    portGunKickFire(GUNRIGHT, ITEM_UZI);
    assert(portGunKickAdvance(GUNRIGHT, ITEM_UZI, 1.0f, &hip));
    assert(portGunKickAdvance(GUNRIGHT, ITEM_M16, 0.0f, &ads));
    assert(portGunKickAdvance(GUNRIGHT, ITEM_UZI, 0.0f, &ads));
    portGunKickOptionSet("weapon_kick", PORT_GUN_KICK_LUDICROUS);
    assert(portGunKickNativeScale(ITEM_UZI) == 0.0f);
    portGunKickFire(GUNRIGHT, ITEM_UZI);
    assert(portGunKickAdvance(GUNRIGHT, ITEM_UZI, 1.0f, &ads));
    assert(ads.pitch > hip.pitch * 2.0f && ads.z > hip.z);
    portGunKickOptionSet("weapon_kick", PORT_GUN_KICK_ENHANCED);

    /* Original: native kick back, no spring, no offset. */
    portGunKickOptionSet("weapon_kick", PORT_GUN_KICK_ORIGINAL);
    portGunKickFire(GUNRIGHT, ITEM_UZI);
    assert(portGunKickNativeScale(ITEM_AK47) == 1.0f);
    assert(!portGunKickAdvance(GUNRIGHT, ITEM_UZI, 1.0f, &hip) && hip.z == 0.0f);
    portGunKickOptionSet("weapon_kick", PORT_GUN_KICK_DEFAULT);
}

static void testGunKick(void)
{
    testGunKickOption();
    testGunKickProfiles();
    testGunKickLudicrous();
    testGunKickSpring();
    testGunKickClimb();
    testGunKickHooks();
}
