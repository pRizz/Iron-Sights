/* Included by test_ads.c: Hip-Fire Arms (FORK-075). */

/* The game's assert is a bare if: brace it inside if/else. */
static void hipArmsNear(coord3d a, coord3d b, f32 eps)
{
    assert(fabsf(a.x - b.x) < eps && fabsf(a.y - b.y) < eps && fabsf(a.z - b.z) < eps);
}

/* Dual wield (owner's option 3): the right gun keeps its trigger hand, and the
 * support arm slides down out of view over 300 ms, then back. */
static void testHipArmsDual(void)
{
    const s32 rates[] = { 30, 60, 120, 240 };
    AdsArmPose pose = { 0 }, lowered;
    s32 r, k;
    /* The pure step: 18 sixtieth-second ticks either way at every tick rate. */
    for (r = 0; r < 4; r++) {
        const f32 delta = 60.0f / rates[r];
        f32 hold = 1.0f;
        for (k = 0; k < rates[r] * 3 / 10; k++) hold = portAdsSupportHoldStep(hold, TRUE, delta);
        assert(hold < 0.01f);
        for (k = 0; k < rates[r] * 3 / 20; k++) hold = portAdsSupportHoldStep(hold, FALSE, delta);
        assert(fabsf(hold - k * delta / 18.0f) < 0.001f && hold > 0.4f && hold < 0.6f);
    }
    assert(portAdsSupportHoldStep(0.0f, TRUE, 5.0f) == 0.0f && portAdsSupportHoldStep(1.0f, FALSE, 5.0f) == 1.0f);
    /* Lowering moves the whole support arm rigidly down and out of view. */
    pose.palm = (coord3d){ -8, -14, -30 }; pose.wrist = (coord3d){ -9, -17, -29 }; pose.elbow = (coord3d){ -20, -28, -12 };
    lowered = pose; adsSupportLowered(&lowered, 1.0f);
    assert(!memcmp(&lowered, &pose, sizeof(pose)));
    lowered = pose; adsSupportLowered(&lowered, 0.0f);
    assert(lowered.palm.y == pose.palm.y - 30.0f && lowered.elbow.y == pose.elbow.y - 30.0f);
    assert(lowered.palm.x - pose.palm.x == lowered.elbow.x - pose.elbow.x);
    /* The game tick follows the left hand, per player. */
    g_GlobalTimerDelta = 1.0f;
    g_CurrentPlayer->hand_item[GUNLEFT] = ITEM_SKORPION;
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_SKORPION;
    for (k = 0; k < 18; k++) portAdsHandsTick();
    assert(sAdsSupportHold[0] == 0.0f && sAdsSupportHold[1] == 1.0f);
    /* Two-handed guns keep the trigger hand; the support-only RC-P90 has none. */
    assert(portAdsHandAlpha(ITEM_SKORPION) == 1.0f);
    g_CurrentPlayer->hand_item[GUNLEFT] = g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_FNP90;
    assert(portAdsHandAlpha(ITEM_FNP90) == 0.0f);
    g_CurrentPlayer->hand_item[GUNLEFT] = ITEM_UNARMED;
    for (k = 0; k < 18; k++) portAdsHandsTick();
    assert(sAdsSupportHold[0] == 1.0f && portAdsHandAlpha(ITEM_FNP90) == 1.0f);
}

/* A tilted support hand's forearm: the hand, wrist seam and elbow point map as
 * without the tilt's extras; at the elbow the sleeve's cross-section lies in
 * the untilted forearm's own frame, girth times fuller. */
static void testHipArmsForearm(const AdsArmPose *tilted, const AdsArmPose *neutral)
{
    const PortGripSpace space = { { -6, -25, -67 }, { -3, -54, -182 } };
    const coord3d handPoint = { -10, 2, 10 };
    coord3d rim = space.elbow, a, b, elbow;
    AdsArmPose plain = *tilted;
    f32 forearm[3], length = 0, along = 0, offBack = 0, offSide = 0, plainLength = 0;
    s32 k;
    plain.untwist = FALSE; plain.girth = 1.0f;
    adsGripPoint(&handPoint, &a, PORT_ARM_HAND, &space, (void *)tilted);
    adsGripPoint(&handPoint, &b, PORT_ARM_HAND, &space, &plain);
    assert(!memcmp(&a, &b, sizeof(a)));
    adsGripPoint(&space.wrist, &a, PORT_ARM_FOREARM, &space, (void *)tilted);
    adsGripPoint(&space.wrist, &b, PORT_ARM_FOREARM, &space, &plain);
    assert(!memcmp(&a, &b, sizeof(a)));
    adsGripPoint(&space.elbow, &elbow, PORT_ARM_FOREARM, &space, (void *)tilted);
    hipArmsNear(elbow, (coord3d){ 16*tilted->elbow.x, 16*tilted->elbow.y, 16*tilted->elbow.z }, 1e-2f);
    rim.x += 10.0f;
    adsGripPoint(&rim, &a, PORT_ARM_FOREARM, &space, (void *)tilted);
    adsGripPoint(&rim, &b, PORT_ARM_FOREARM, &space, &plain);
    /* The elbow frame is the hand's without its fit roll or tilt: square to
     * the untilted forearm. */
    hipArmsNear(tilted->elbowBack, neutral->back, 1e-4f);
    hipArmsNear(tilted->elbowSide, neutral->side, 1e-4f);
    for (k = 0; k < 3; k++) {
        forearm[k] = neutral->axis.f[k];
        along += tilted->elbowBack.f[k]*forearm[k] + tilted->elbowSide.f[k]*forearm[k];
        offBack += (a.f[k]-elbow.f[k])*tilted->elbowBack.f[k];
        offSide += (a.f[k]-elbow.f[k])*tilted->elbowSide.f[k];
        length += (a.f[k]-elbow.f[k])*(a.f[k]-elbow.f[k]);
        plainLength += (b.f[k]-elbow.f[k])*(b.f[k]-elbow.f[k]);
    }
    assert(fabsf(along) < 1e-3f);
    /* A source x offset runs along -back at the elbow, in the elbow frame. */
    assert(offBack < 0.0f && fabsf(offSide) < 1e-2f * sqrtf(length));
    assert(fabsf(sqrtf(length/plainLength) - tilted->girth) < 1e-3f);
}

/* A fit's wrist scale: the wrist, cuff and watch widen about the arm's centre
 * line by the fit's depth (palm to back) and width, easing in over the hand's
 * heel. The grip ahead of the heel and the centre line keep their places, the
 * sleeve leaves the cuff at the wrist's size, and the elbow keeps its own. */
static void testHipArmsWrist(const AdsArmPose *scaled)
{
    const PortGripSpace space = { { -6, -25, -67 }, { -3, -54, -182 } };
    const f32 ramp = sAdsArmProfile.wristRamp, cuffZ = space.wrist.z - sAdsArmProfile.protectedWrist;
    const f32 depth = scaled->wristDepth, width = scaled->wristWidth;
    const coord3d grip = { -10, 2, space.wrist.z + ramp };
    const coord3d ring = { space.wrist.x + 8, space.wrist.y - 6, space.wrist.z };
    const coord3d half = { space.wrist.x + 8, space.wrist.y - 6, space.wrist.z + ramp/2 };
    coord3d cuff = { 0, 0, cuffZ }, sleeve, rim = space.elbow, centre, a, b;
    AdsArmPose plain = *scaled;
    s32 k;
    plain.wristDepth = plain.wristWidth = 1.0f;
    assert(depth > 1.0f && width > 1.0f);
    /* The grip ahead of the heel, and every point without a scale, are untouched. */
    a = adsWristPoint(&grip, &space, scaled);
    assert(!memcmp(&a, &grip, sizeof(a)));
    a = adsWristPoint(&ring, &space, &plain);
    assert(!memcmp(&a, &ring, sizeof(a)));
    /* At the wrist the scale is whole, about the centre line, which stays put. */
    a = adsWristPoint(&ring, &space, scaled);
    assert(fabsf(a.x - (space.wrist.x + 8*depth)) < 1e-4f && fabsf(a.y - (space.wrist.y - 6*width)) < 1e-4f);
    assert(a.z == ring.z);
    a = adsWristPoint(&space.wrist, &space, scaled);
    hipArmsNear(a, space.wrist, 1e-5f);
    /* Halfway along the heel it is half. */
    centre = adsHandLerp(space.wrist, space.elbow, (space.wrist.z-half.z)/(space.wrist.z-space.elbow.z));
    a = adsWristPoint(&half, &space, scaled);
    assert(fabsf((a.x-centre.x) - (half.x-centre.x)*(1 + (depth-1)*0.5f)) < 1e-4f);
    assert(fabsf((a.y-centre.y) - (half.y-centre.y)*(1 + (width-1)*0.5f)) < 1e-4f);
    /* Hand, cuff and watch points at the wrist move as one, along the hand's
     * back and side. */
    adsGripPoint(&ring, &a, PORT_ARM_WATCH, &space, (void *)scaled);
    adsGripPoint(&ring, &b, PORT_ARM_HAND, &space, (void *)scaled);
    assert(!memcmp(&a, &b, sizeof(a)));
    adsGripPoint(&ring, &b, PORT_ARM_FOREARM, &space, (void *)scaled);
    assert(!memcmp(&a, &b, sizeof(a)));
    adsGripPoint(&space.wrist, &b, PORT_ARM_HAND, &space, (void *)scaled);
    for (k = 0; k < 3; k++) {
        const f32 expect = 16*scaled->scale*(-8*depth*scaled->back.f[k] - 6*width*scaled->side.f[k]);
        assert(fabsf(a.f[k] - b.f[k] - expect) < 0.05f);
    }
    /* The sleeve meets the scaled cuff without a step. */
    centre = adsHandLerp(space.wrist, space.elbow, (space.wrist.z-cuffZ)/(space.wrist.z-space.elbow.z));
    cuff.x = centre.x + 8; cuff.y = centre.y - 6;
    sleeve = cuff; sleeve.z -= 0.001f;
    adsGripPoint(&cuff, &a, PORT_ARM_FOREARM, &space, (void *)scaled);
    adsGripPoint(&sleeve, &b, PORT_ARM_FOREARM, &space, (void *)scaled);
    hipArmsNear(a, b, 0.1f);
    adsGripPoint(&cuff, &b, PORT_ARM_FOREARM, &space, &plain);
    assert(fabsf(a.x-b.x) + fabsf(a.y-b.y) + fabsf(a.z-b.z) > 16*scaled->scale);
    /* The elbow is as it is without the scale. */
    rim.x += 10; rim.y += 7;
    adsGripPoint(&rim, &a, PORT_ARM_FOREARM, &space, (void *)scaled);
    adsGripPoint(&rim, &b, PORT_ARM_FOREARM, &space, &plain);
    assert(!memcmp(&a, &b, sizeof(a)));
}

static void testHipArms(void)
{
    const coord3d muzzle = { 0.0f, 23.273382f, 705.7365f };
    coord3d trigger, support;
    Mtxf identity, turn, settled, hipGun;
    s32 value = -1, grip, left;

    /* Arrange: off by default, and the ini and menu paths reach the setting. */
    assert(portJoyPortOptionGet("hip_arms", &value) && value == PORT_HIP_ARMS_DEFAULT && value == 0);
    assert(portJoyPortSetOption("hip_arms", "1") && portJoyPortOptionGet("hip_arms", &value) && value == 1);
    assert(portJoyPortOptionSet("hip_arms", 0) && portJoyPortOptionGet("hip_arms", &value) && value == 0);

    /* The pure rule: no grips, nothing; otherwise opaque with the setting,
     * else the ADS fade. */
    assert(portAdsHandAlphaFor(FALSE, TRUE, 1.0f) == 0.0f);
    assert(portAdsHandAlphaFor(TRUE, FALSE, 0.25f) == 0.25f);
    assert(portAdsHandAlphaFor(TRUE, TRUE, 0.0f) == 1.0f);

    /* Act/assert on the player's guns: at the hip (blend 0) the arms show only
     * with the setting, never on guns with their own hands. */
    selectPlayer(0);
    assert(portAdsBlend() == 0.0f);
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_AK47;
    assert(portAdsHandAlpha(ITEM_AK47) == 0.0f);
    portJoyPortOptionSet("hip_arms", 1);
    assert(portAdsHandAlpha(ITEM_AK47) == 1.0f);
    assert(portAdsHandAlpha(ITEM_WPPK) == 0.0f);
    testHipArmsDual();
    portJoyPortOptionSet("hip_arms", 0);
    selectPlayer(0);

    /* Every grip: the hip arm is the settled ADS arm carried rigidly with the
     * gun. Hands are the ADS hands on that gun; only the elbow target moves. */
    matrix_4x4_set_identity(&identity);
    for (grip = 0; grip < sizeof(sAdsGrips)/sizeof(sAdsGrips[0]); grip++) {
        const s32 item = sAdsGrips[grip].item;
        const f32 scale = adsArmScale(item);
        const s32 shouldered = adsShoulderedGrip(item);
        s32 k;
        assert(adsSettledGun(item, &muzzle, &settled));
        for (k = 0; k < 3; k++) { /* an orthonormal basis, so the carry is rigid */
            const f32 length = settled.m[k][0]*settled.m[k][0] + settled.m[k][1]*settled.m[k][1]
                + settled.m[k][2]*settled.m[k][2];
            assert(length > 0.999f && length < 1.001f);
        }
        /* A lowered, pitched hip gun: down, right and further out. */
        matrix_4x4_set_rotation_around_x(-0.35f, &turn);
        matrix_4x4_multiply_homogeneous(&settled, &turn, &hipGun);
        hipGun.m[3][0] += 9.0f; hipGun.m[3][1] -= 7.0f; hipGun.m[3][2] -= 5.0f;
        const AdsHipGrip none = { item }, tuned = adsHipGrip(item);
        assert(tuned.item == item);
        for (left = 0; left < 2; left++) {
            const coord3d at = left ? sAdsGrips[grip].support : sAdsGrips[grip].trigger;
            const coord3d ads = adsElbow(left, 1.0f, shouldered);
            AdsArmPose hip = adsHipPose(&hipGun, &hipGun, &settled, at, left, 0.0f, scale, shouldered, &none);
            AdsArmPose native = adsNativePose(&hipGun, at, left, 0.0f, scale, shouldered);
            AdsArmPose same = adsHipPose(&settled, &settled, &settled, at, left, 0.0f, scale, shouldered, &none);
            AdsArmPose half = adsHipPose(&hipGun, &hipGun, &settled, at, left, 0.5f, scale, shouldered, &none);
            AdsArmPose raised = adsHipPose(&hipGun, &hipGun, &settled, at, left, 1.0f, scale, shouldered, &none);
            AdsArmPose fitted = adsHipPose(&hipGun, &hipGun, &settled, at, left, 0.0f, scale, shouldered, &tuned);
            AdsArmPose fittedRaised = adsHipPose(&hipGun, &hipGun, &settled, at, left, 1.0f, scale, shouldered, &tuned);
            /* The per-gun hip fit moves only the support hand and its elbow,
             * keeps the rigid hand scale, and is gone once the gun is raised. */
            if (!left) {
                assert(!memcmp(&fitted, &hip, sizeof(fitted)));
            } else {
                coord3d moved = at;
                moved.x += tuned.support.x; moved.y += tuned.support.y; moved.z += tuned.support.z;
                hipArmsNear(fitted.palm, adsNativePose(&hipGun, moved, TRUE, 0.0f, scale, shouldered).palm, 1e-4f);
            }
            assert(fitted.scale == hip.scale);
            hipArmsNear(fittedRaised.palm, raised.palm, 1e-4f);
            hipArmsNear(fittedRaised.elbow, raised.elbow, 1e-4f);
            hipArmsNear(fittedRaised.back, raised.back, 1e-4f);
            hipArmsNear(fittedRaised.axis, raised.axis, 1e-4f);
            assert(fabsf(fitted.back.x*fitted.axis.x + fitted.back.y*fitted.axis.y + fitted.back.z*fitted.axis.z) < 1e-3f);
            /* Elbow fixed to the gun, as at the settled ADS pose. */
            hipArmsNear(adsGunCarried(&identity, &hipGun, hip.elbow), adsGunCarried(&identity, &settled, ads), 1e-3f);
            assert(!memcmp(&hip.palm, &native.palm, sizeof(hip.palm)));
            assert(!memcmp(&hip.wrist, &native.wrist, sizeof(hip.wrist)));
            assert(hip.scale == native.scale);
            /* On the settled gun the hip arm is the ADS arm; aiming eases the
             * elbow onto its camera-space target. */
            hipArmsNear(same.elbow, ads, 1e-3f);
            hipArmsNear(raised.elbow, ads, 1e-4f);
            hipArmsNear(half.elbow, adsHandLerp(hip.elbow, ads, 0.5f), 1e-4f);
            /* a valid forearm basis */
            assert(fabsf(hip.back.x*hip.axis.x + hip.back.y*hip.axis.y + hip.back.z*hip.axis.z) < 1e-3f);
        }
    }
    /* Every fitted gun has generated grips; the rocket's support elbow bends
     * lower and nearer the body instead of reaching for the lower left. */
    for (grip = 0; grip < sizeof(sAdsHipGrips)/sizeof(sAdsHipGrips[0]); grip++) {
        assert(portAdsHandGrips(sAdsHipGrips[grip].item, &trigger, &support));
    }
    {
        const AdsHipGrip none = { ITEM_ROCKETLAUNCH }, tuned = adsHipGrip(ITEM_ROCKETLAUNCH);
        assert(adsSettledGun(ITEM_ROCKETLAUNCH, &muzzle, &settled));
        assert(portAdsHandGrips(ITEM_ROCKETLAUNCH, &trigger, &support));
        hipGun = settled;
        hipGun.m[3][0] += 9.0f; hipGun.m[3][1] -= 7.0f;
        {
            AdsArmPose straight = adsHipPose(&hipGun, &hipGun, &settled, support, TRUE, 0.0f, 1.0f, FALSE, &none);
            AdsArmPose bent = adsHipPose(&hipGun, &hipGun, &settled, support, TRUE, 0.0f, 1.0f, FALSE, &tuned);
            /* In the gun's own frame: lower, and towards its right (the body). */
            const coord3d b = adsGunCarried(&identity, &hipGun, bent.elbow);
            const coord3d s = adsGunCarried(&identity, &hipGun, straight.elbow);
            assert(b.y < s.y - 10.0f && b.x < s.x - 10.0f);
        }
    }
    /* The KF7's closed finger tips pass through its fore-end, so its hip
     * support hand alone draws before the gun, which then covers it. Its
     * trigger hand, and both hands of every other gun, follow the gun. */
    for (grip = 0; grip < sizeof(sAdsGrips)/sizeof(sAdsGrips[0]); grip++) {
        assert(adsHipGrip(sAdsGrips[grip].item).behind == (sAdsGrips[grip].item == ITEM_AK47));
    }
    assert(adsHandInPass(TRUE, TRUE, 0) && !adsHandInPass(TRUE, TRUE, 1));
    assert(!adsHandInPass(TRUE, FALSE, 0) && adsHandInPass(TRUE, FALSE, 1));
    assert(!adsHandInPass(FALSE, TRUE, 0) && !adsHandInPass(FALSE, TRUE, 1));
    assert(adsHandInPass(FALSE, FALSE, 0) && adsHandInPass(FALSE, FALSE, 1));
    /* The AR33's hip support hand alone tilts: its axis, along which the thumb
     * lies, leans towards the gun's up while the palm and elbow stay put, and
     * the lean fades through the raise. Its forearm alone is fuller, and eases
     * from the tilted hand's frame to the plain palm-up frame at the elbow. */
    for (grip = 0; grip < sizeof(sAdsGrips)/sizeof(sAdsGrips[0]); grip++) {
        assert((adsHipGrip(sAdsGrips[grip].item).tilt != 0.0f) == (sAdsGrips[grip].item == ITEM_M16));
        assert((adsHipGrip(sAdsGrips[grip].item).girth != 0.0f) == (sAdsGrips[grip].item == ITEM_M16));
        assert((adsHipGrip(sAdsGrips[grip].item).wristDepth != 0.0f) == (sAdsGrips[grip].item == ITEM_M16));
        assert((adsHipGrip(sAdsGrips[grip].item).wristWidth != 0.0f) == (sAdsGrips[grip].item == ITEM_M16));
    }
    {
        const AdsHipGrip tuned = adsHipGrip(ITEM_M16);
        AdsHipGrip flat = tuned, unrolled;
        f32 up[3];
        s32 k, b;
        flat.tilt = 0.0f;
        unrolled = flat; unrolled.cant = 0.0f;
        assert(adsSettledGun(ITEM_M16, &muzzle, &settled));
        assert(portAdsHandGrips(ITEM_M16, &trigger, &support));
        hipGun = settled;
        hipGun.m[3][0] += 9.0f; hipGun.m[3][1] -= 7.0f;
        for (b = 0; b < 3; b++) {
            const f32 blend = b * 0.5f;
            AdsArmPose level = adsHipPose(&hipGun, &hipGun, &settled, support, TRUE, blend, 1.0f, FALSE, &flat);
            AdsArmPose tilted = adsHipPose(&hipGun, &hipGun, &settled, support, TRUE, blend, 1.0f, FALSE, &tuned);
            AdsArmPose right = adsHipPose(&hipGun, &hipGun, &settled, trigger, FALSE, blend, 1.0f, FALSE, &tuned);
            AdsArmPose rightLevel = adsHipPose(&hipGun, &hipGun, &settled, trigger, FALSE, blend, 1.0f, FALSE, &flat);
            up[b] = 0.0f;
            for (k = 0; k < 3; k++) up[b] += (tilted.axis.f[k] - level.axis.f[k]) * hipGun.m[1][k];
            assert(!memcmp(&tilted.palm, &level.palm, sizeof(level.palm)));
            assert(!memcmp(&tilted.elbow, &level.elbow, sizeof(level.elbow)));
            assert(fabsf(tilted.back.x*tilted.axis.x + tilted.back.y*tilted.axis.y + tilted.back.z*tilted.axis.z) < 1e-3f);
            assert(!memcmp(&right, &rightLevel, sizeof(right)));
            assert(right.girth == 1.0f && !right.untwist && !level.untwist);
            assert(fabsf(tilted.girth - (1.0f + (tuned.girth - 1.0f) * (1.0f - blend))) < 1e-6f);
            /* Its wrist alone is thicker and wider; that fades with the raise too. */
            assert(right.wristDepth == 1.0f && right.wristWidth == 1.0f);
            assert(fabsf(tilted.wristDepth - (1.0f + (tuned.wristDepth - 1.0f) * (1.0f - blend))) < 1e-6f);
            assert(fabsf(tilted.wristWidth - (1.0f + (tuned.wristWidth - 1.0f) * (1.0f - blend))) < 1e-6f);
            if (b < 2) testHipArmsWrist(&tilted);
            else assert(tilted.wristDepth == 1.0f && tilted.wristWidth == 1.0f);
            assert(tilted.untwist == (b < 2));
            if (tilted.untwist) {
                AdsArmPose neutral = adsHipPose(&hipGun, &hipGun, &settled, support, TRUE, blend, 1.0f, FALSE, &unrolled);
                testHipArmsForearm(&tilted, &neutral);
            }
        }
        assert(tuned.girth == 1.3f && tuned.wristDepth == 1.5f && tuned.wristWidth == 1.25f);
        assert(up[0] > 0.2f && up[1] > 0.05f && up[1] < up[0] && up[2] == 0.0f);
    }
    /* Off, the ADS fade keeps its own elbow drop exactly. */
    {
        const coord3d a = adsElbow(TRUE, 0.25f, FALSE), b = adsElbow(FALSE, 0.25f, TRUE);
        assert(a.x == -20.0f && a.y == -28.0f - 0.75f*18.0f && a.z == -12.0f);
        assert(b.x == 14.0f && b.y == -12.0f - 0.75f*18.0f && b.z == 6.0f);
    }
    { /* The long gun's painter's-order bracket is one renderer no-op tag each way. */
        Gfx list[2];
        assert(portAdsGunPainterTag(list, TRUE) == list + 1 && portAdsGunPainterTag(list + 1, FALSE) == list + 2);
        assert((list[0].words.w0 >> 24) == G_NOOP && list[0].words.w1 == 0x64000001u);
        assert((list[1].words.w0 >> 24) == G_NOOP && list[1].words.w1 == 0x64000000u);
    }
    printf("Hip-Fire Arms checks passed: default off, ini/menu keys, opacity rule, dual/native-hand gates, rigid settled-ADS carry and fading per-gun hip fits for all %d grips, bent rocket elbow, the KF7 hand's behind-the-gun pass, the AR33 hand's fading thumb-up tilt, thicker wrist, fuller forearm and elbow frame, painter's-order tag.\n",
        (int)(sizeof(sAdsGrips)/sizeof(sAdsGrips[0])));
}
