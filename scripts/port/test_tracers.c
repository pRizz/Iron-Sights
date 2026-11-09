/* Included by test_ads.c: Tracer Strength (FORK-062) scales the tracer look
 * from one percentage, keeps 100% original, holds a minimum on-screen width,
 * and leaves the lasers alone. */
static void testTracerOption(void)
{
    s32 value;

    assert(portOptFind("tracer_strength") >= 0 && portOptCategoryOf(portOptFind("tracer_strength")) == PORTOPT_CAT_GAMEPLAY);
    assert(portJoyPortOptionGet("tracer_strength", &value) && value == PORT_TRACER_STRENGTH_DEFAULT);
    assert(portJoyPortSetOption("tracer_strength", "50") && portJoyPortOptionGet("tracer_strength", &value) && value == 100);
    assert(portJoyPortOptionSet("tracer_strength", 999) && portJoyPortOptionGet("tracer_strength", &value) && value == 400);
    assert(portJoyPortOptionSet("tracer_strength", PORT_TRACER_STRENGTH_DEFAULT));
    assert(!portTracerOptionGet("weapon_kick", &value));
}

static void testTracerLooks(void)
{
    PortTracerLook look;

    /* 100% is the native tracer: no widening, glow or minimum. */
    portTracerLookFor(100, &look);
    assert(look.width == 1.0f && look.length == 1.0f && look.minWidth == 0.0f && look.glowWidth == 0.0f);

    /* 200%: twice as wide, a 1.5x streak, a 1.5 px (of 1080) minimum, a 2.5x glow. */
    portTracerLookFor(200, &look);
    assert(nearlyEqual(look.width, 2.0f) && nearlyEqual(look.length, 1.5f));
    assert(nearlyEqual(look.minWidth * 1080.0f, 1.5f) && look.glowWidth == 2.5f && look.glowAlpha == 90);

    /* 400%: the streak and glow alpha stop at their caps. */
    portTracerLookFor(400, &look);
    assert(nearlyEqual(look.width, 4.0f) && look.length == 2.0f && look.glowAlpha == 160);
}

static void testTracerMinimumWidth(void)
{
    PortTracerLook look;
    f32 view[4][4] = { {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1} };
    const f32 ahead[3] = { 0.0f, 0.0f, -250.0f };
    const f32 behind[3] = { 0.0f, 0.0f, 40.0f };
    const f32 tanhalf = portTracerTanHalfFov(60.0f);

    /* Depth is distance in front of the camera, never negative. */
    assert(nearlyEqual(portTracerViewDepth(view, ahead), 250.0f));
    assert(portTracerViewDepth(view, behind) == 0.0f);
    assert(kickAbs(tanhalf - 0.57735f) < 0.0005f);

    /* Close tracers keep their width; far ones widen to 1.5/1080 of the screen. */
    portTracerLookFor(200, &look);
    assert(portTracerMinWidthScale(60.0f, 100.0f, tanhalf, &look) == 1.0f);
    assert(kickAbs(portTracerMinWidthScale(60.0f, 10000.0f, tanhalf, &look) - 1.3365f) < 0.002f);
    portTracerLookFor(100, &look);
    assert(portTracerMinWidthScale(60.0f, 10000.0f, tanhalf, &look) == 1.0f);
}

static void testTracerStrips(void)
{
    const f32 right[3] = { 30.0f, 0.0f, -10.0f };
    const f32 flare[3] = { 0.0f, 500.0f, 2000.0f };
    Vtx core[4], glow[4];
    s32 i;

    /* At scale 1 the strip is the native layout, truncated to s16 as natively
     * (0.9f * 30 lands just under 27). */
    memset(core, 0, sizeof(core));
    for (i = 0; i < 4; i++) core[i].v.cn[3] = 0xff;
    portTracerShapeStrip(core, right, flare, 1.0f, 1.0f);
    assert(core[0].v.ob[0] == 30 && core[1].v.ob[0] == -30 && core[0].v.ob[2] == -10);
    assert(core[2].v.ob[0] == 26 && core[3].v.ob[0] == -26 && core[2].v.ob[1] == 500 && core[3].v.ob[2] == 2008);

    /* Per-end scales widen each end on its own. */
    portTracerShapeStrip(core, right, flare, 2.0f, 3.0f);
    assert(core[0].v.ob[0] == 60 && core[2].v.ob[0] >= 80 && core[2].v.ob[0] <= 81);

    /* The glow is wider and fainter, with the core's texture coordinates. */
    core[2].v.tc[0] = 77;
    portTracerGlowStrip(glow, core, right, flare, 1.0f, 1.0f, 2.5f, 90);
    assert(glow[0].v.ob[0] == 75 && glow[3].v.ob[0] >= -67 && glow[3].v.ob[0] <= -66);
    assert(glow[1].v.cn[3] == 90 && glow[2].v.tc[0] == 77);
}

static void testTracerLaserExemption(void)
{
    PortTracerLook look;

    assert(portTracerLook(ITEM_AK47, &look) && look.width == 2.0f);
    assert(!portTracerLook(ITEM_LASER, &look) && look.width == 1.0f);
    assert(!portTracerLook(ITEM_WATCHLASER, &look));
    portTracerOptionSet("tracer_strength", 100);
    assert(!portTracerLook(ITEM_AK47, &look) && look.glowWidth == 0.0f);
    portTracerOptionSet("tracer_strength", PORT_TRACER_STRENGTH_DEFAULT);
}

static void testTracers(void)
{
    testTracerOption();
    testTracerLooks();
    testTracerMinimumWidth();
    testTracerStrips();
    testTracerLaserExemption();
}
