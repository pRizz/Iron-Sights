/* Included by test_ads.c: Tracer Strength (FORK-062) scales the tracer look
 * from one percentage, keeps 100% original, holds a minimum on-screen width,
 * and leaves the lasers alone; Shotgun Tracers (FORK-063) times thinner pellet
 * beams like the native tracer without the game RNG (no randomGetNext is
 * linked into this test, so any call would fail to build). */
static void testTracerOption(void)
{
    s32 value;

    assert(portOptFind("tracer_strength") >= 0 && portOptCategoryOf(portOptFind("tracer_strength")) == PORTOPT_CAT_GAMEPLAY);
    assert(portJoyPortOptionGet("tracer_strength", &value) && value == PORT_TRACER_STRENGTH_DEFAULT);
    assert(portJoyPortSetOption("tracer_strength", "50") && portJoyPortOptionGet("tracer_strength", &value) && value == 100);
    assert(portJoyPortOptionSet("tracer_strength", 999) && portJoyPortOptionGet("tracer_strength", &value) && value == 400);
    assert(portJoyPortOptionSet("tracer_strength", PORT_TRACER_STRENGTH_DEFAULT));
    assert(!portTracerOptionGet("weapon_kick", &value));

    /* Shotgun Tracers is on by default and a plain toggle. */
    assert(portOptFind("shotgun_tracers") >= 0 && portOptCategoryOf(portOptFind("shotgun_tracers")) == PORTOPT_CAT_GAMEPLAY);
    assert(portJoyPortOptionGet("shotgun_tracers", &value) && value == 1 && portShotgunTracersEnabled());
    assert(portJoyPortSetOption("shotgun_tracers", "0") && !portShotgunTracersEnabled());
    assert(portJoyPortOptionSet("shotgun_tracers", 7) && portJoyPortOptionGet("shotgun_tracers", &value) && value == 1);
    assert(portJoyPortOptionSet("shotgun_tracers", PORT_SHOTGUN_TRACERS_DEFAULT) && portShotgunTracersEnabled());
}

static void testPelletLook(void)
{
    PortTracerLook look;

    /* Pellets are 60% as wide and glow 60% as bright as a bullet tracer... */
    assert(portTracerLook(ITEM_SHOTGUN, &look) && nearlyEqual(look.width, 1.2f) && look.glowAlpha == 54);
    assert(portTracerLook(ITEM_AUTOSHOT, &look) && nearlyEqual(look.width, 1.2f));
    /* ...and still thinner than the native tracer at 100%. */
    portTracerOptionSet("tracer_strength", 100);
    assert(portTracerLook(ITEM_SHOTGUN, &look) && nearlyEqual(look.width, 0.6f) && look.glowWidth == 0.0f);
    portTracerOptionSet("tracer_strength", PORT_TRACER_STRENGTH_DEFAULT);
}

static void testPelletBeams(void)
{
    const coord3d muzzle = { 0.0f, 0.0f, 0.0f };
    const coord3d wall = { 0.0f, 0.0f, -1000.0f };
    const coord3d far = { 0.0f, 0.0f, -20000.0f };
    const coord3d close = { 0.0f, 0.0f, -80.0f };
    BeamRecord beam;
    s32 live;

    /* The native numbers: a fifth of the path per field, starting up to one step either side. */
    portPelletBeamInit(&beam, ITEM_SHOTGUN, &muzzle, &wall, 0.5f);
    assert(beam.unk00 == 0 && beam.item_id == ITEM_SHOTGUN && beam.unk1c == 1000.0f);
    assert(nearlyEqual(beam.unk20, 200.0f) && beam.unk24 == beam.unk20 && beam.unk28 == 0.0f);
    assert(beam.delta.z == -1.0f && beam.delta.x == 0.0f);
    portPelletBeamInit(&beam, ITEM_SHOTGUN, &muzzle, &wall, 0.0f);
    assert(nearlyEqual(beam.unk28, -200.0f));

    /* Paths cap at 10000; a pellet whose start is past its hit draws nothing. */
    portPelletBeamInit(&beam, ITEM_AUTOSHOT, &muzzle, &far, 0.5f);
    assert(beam.unk1c == 10000.0f && nearlyEqual(beam.unk24, 2000.0f));
    portPelletBeamInit(&beam, ITEM_SHOTGUN, &muzzle, &close, 1.0f);
    assert(beam.unk00 == -1);
    portPelletBeamInit(&beam, ITEM_SHOTGUN, &muzzle, &muzzle, 0.5f);
    assert(beam.unk00 == -1);

    /* It crosses the path in five fields, at any tick rate. */
    portPelletBeamInit(&beam, ITEM_SHOTGUN, &muzzle, &wall, 0.5f);
    for (live = 0; beam.unk00 >= 0 && live < 50; live++) portPelletBeamAdvance(&beam, 0.25f);
    assert(live == 20);
    portPelletBeamAdvance(&beam, 1.0f);
    assert(beam.unk00 == -1);
}

static void testPelletSameDirection(void)
{
    const coord3d hit = { 0.0f, 0.0f, -1000.0f };
    const coord3d eye = { 0.0f, 0.0f, 0.0f };
    const coord3d near = { 50.0f, 0.0f, 0.0f };  /* about 2.9 degrees off at 1000 */
    const coord3d wide = { 120.0f, 0.0f, 0.0f }; /* about 6.8 degrees off */

    /* The multiplayer beam only shows when both guns point at the hit within 5 degrees. */
    assert(portPelletSameDirection(&eye, &near, &hit));
    assert(!portPelletSameDirection(&eye, &wide, &hit));
    assert(!portPelletSameDirection(&hit, &eye, &hit));
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

static void testDecorativePellets(void)
{
    const coord3d from = { 10.0f, 20.0f, 30.0f };
    const coord3d level = { 10.0f, 20.0f, -970.0f };
    const coord3d up = { 10.0f, 1020.0f, 30.0f };
    coord3d end;
    s32 value, k;

    /* On by default, and only in effect together with Shotgun Tracers. */
    assert(portOptFind("decorative_pellets") >= 0);
    assert(portJoyPortOptionGet("decorative_pellets", &value) && value == 1 && portDecorativePelletsEnabled());
    portTracerOptionSet("shotgun_tracers", 0);
    assert(!portDecorativePelletsEnabled());
    portTracerOptionSet("shotgun_tracers", 1);
    assert(portJoyPortOptionSet("decorative_pellets", 0) && !portDecorativePelletsEnabled());
    portTracerOptionSet("shotgun_tracers", PORT_SHOTGUN_TRACERS_DEFAULT);
    portTracerOptionSet("decorative_pellets", PORT_DECORATIVE_PELLETS_DEFAULT);

    /* Radius 0 is the real shot; any draw stays within 3 degrees and keeps the length. */
    portPelletDecorativeEnd(&from, &level, 0.0f, 0.3f, &end);
    assert(kickAbs(end.x - level.x) < 0.01f && kickAbs(end.y - level.y) < 0.01f && kickAbs(end.z - level.z) < 0.01f);
    for (k = 0; k < 16; k++)
    {
        const coord3d *to = (k & 1) ? &up : &level; /* a level and a straight-up shot */
        f32 dx, dy, dz, len, cosine;

        portPelletDecorativeEnd(&from, to, 1.0f, (f32) k / 16.0f, &end);
        dx = end.x - from.x; dy = end.y - from.y; dz = end.z - from.z;
        len = sqrtf(dx * dx + dy * dy + dz * dz);
        cosine = (dx * (to->x - from.x) + dy * (to->y - from.y) + dz * (to->z - from.z)) / (len * 1000.0f);
        assert(kickAbs(len - 1000.0f) < 0.05f);
        assert(cosine >= 0.99862f && cosine < 0.99870f); /* the cone's rim, cos 3 degrees */
    }
}

static void testTracers(void)
{
    testTracerOption();
    testTracerLooks();
    testTracerMinimumWidth();
    testTracerStrips();
    testTracerLaserExemption();
    testPelletLook();
    testPelletBeams();
    testPelletSameDirection();
    testDecorativePellets();
}
