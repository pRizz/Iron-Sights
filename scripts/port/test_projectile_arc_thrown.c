/* Included by test_ads.c after test_projectile_arc.c: Projectile Arc
 * (FORK-069), a thrown hand grenade's whole path, and a knife's or mine's. It hops once on landing,
 * settles, slides and stops on the floor; it explodes where it is when its
 * fuse runs out, even in the air; walls turn it in the air and stop it on
 * the ground; and testing collisions on chords lands it where testing every
 * field does. */

/* The fixture's wall, for a settled grenade: it stops its width short. */
static s32 arcFixtureSlide(void *vctx, const coord3d *from, coord3d *to, f32 width)
{
    ArcFixture *fx = (ArcFixture *) vctx;

    if (!fx->wall || to->x <= fx->wallX - width) return TRUE;
    to->x = (from->x > fx->wallX - width) ? from->x : fx->wallX - width;
    to->z = from->z;
    return FALSE;
}

static PortArcWorld arcThrownWorld(ArcFixture *fx)
{
    PortArcWorld w = arcWorld(fx);

    w.slide = arcFixtureSlide;
    return w;
}

static PortArcLaunch arcThrow(f32 vx, f32 fuse, f32 step)
{
    PortArcLaunch l = arcLaunch(PORT_ARC_THROWN, vx, PORT_ARC_HAND_LIFT, step);

    l.maxFields = fuse;
    l.floorClearance = PORT_ARC_HAND_CLEARANCE;
    l.slideFriction = PORT_ARC_HAND_FRICTION;
    l.bounce = PORT_ARC_HAND_BOUNCE;
    l.restHeight = PORT_ARC_HAND_REST;
    l.turnFields = PORT_ARC_HAND_TURN_FIELDS;
    return l;
}

/* At 60 Hz the game scales the slide's friction by the tick's step; while
 * frames take two fields each, the roll after the turn is half as long. */
static void testArcThrownTickRate(void)
{
    ArcFixture fx = { 0.0f, TRUE, TRUE, 0.0f, FALSE, 0.0f, FALSE, 0 };
    PortArcWorld w = arcThrownWorld(&fx);
    PortArcLaunch l = arcThrow(4.0f, 2000.0f, 3.0f);
    PortArcPath steady;
    PortArcPath twoFields;
    f32 friction;
    f32 rollShort;
    f32 rollLong;

    /* Start at rest height with no fall left: it settles on landing and
     * slides at 4 units a field. */
    l.pos.y = PORT_ARC_HAND_REST + 0.1f;
    l.vel.y = -3.0f;
    portArcSimulateThrown(&l, &w, &steady);
    l.slideFriction = PORT_ARC_HAND_FRICTION * 2.0f;
    portArcSimulateThrown(&l, &w, &twoFields);
    assert(steady.impact && twoFields.impact);
    friction = PORT_ARC_HAND_FRICTION;
    rollShort = twoFields.end.x;
    rollLong = steady.end.x;
    /* The same flight and turn; past them, v^2 / 2f apart. */
    assert(rollLong > rollShort + 20.0f);
    assert(fabsf((rollLong - rollShort) - (16.0f / (2.0f * friction)) * (1.0f - 0.5f)) < 6.0f);
}

static f32 arcRestY(const ArcFixture *fx)
{
    return fx->floorY + PORT_ARC_HAND_REST;
}

/* Where a bounce puts it: 4 units over where it bounced. */
static f32 arcBounceY(const ArcFixture *fx)
{
    return fx->floorY + PORT_ARC_HAND_CLEARANCE + 4.0f;
}

static void testArcThrownRolls(void)
{
    ArcFixture fx = { 0.0f, TRUE, TRUE, 0.0f, FALSE, 0.0f, FALSE, 0 };
    PortArcWorld w = arcThrownWorld(&fx);
    PortArcLaunch l = arcThrow(PORT_ARC_HAND_SPEED, (f32) PORT_ARC_HAND_FUSE + 1.0f, 3.0f);
    PortArcPath path;
    s32 landed = -1;
    s32 hopped = FALSE;
    s32 i;

    portArcSimulateThrown(&l, &w, &path);
    assert(path.impact && path.legCount == 0);
    assert(path.pointCount > 2 && path.pointCount <= PORT_ARC_HAND_FUSE + 2);
    assert(path.points[0].x == l.pos.x && path.points[0].y == l.pos.y);
    for (i = 1; i < path.pointCount; i++)
    {
        assert(path.points[i].y >= fx.floorY);              /* never under the floor */
        assert(path.points[i].x >= path.points[i - 1].x);   /* nothing turns it back */
        if (landed < 0 && fabsf(path.points[i].y - arcBounceY(&fx)) < 0.01f) landed = i;
        if (landed >= 0 && i > landed && path.points[i].y > arcBounceY(&fx) + 0.5f) hopped = TRUE;
    }
    assert(landed > 0 && hopped);
    /* It comes to rest on the floor well before the fuse, past the landing. */
    assert(fabsf(path.end.y - arcRestY(&fx)) < 0.01f);
    assert(path.end.x > path.points[landed].x + 20.0f);
    assert(path.pointCount < PORT_ARC_HAND_FUSE);
    assert(fabsf(path.end.x - path.points[path.pointCount - 1].x) < 0.01f);
}

static void testArcThrownFuse(void)
{
    ArcFixture fx = { 0.0f, TRUE, TRUE, 0.0f, FALSE, 0.0f, FALSE, 0 };
    PortArcWorld w = arcThrownWorld(&fx);
    PortArcLaunch l = arcThrow(PORT_ARC_HAND_SPEED, 6.0f, 3.0f);
    PortArcPath path;

    /* Cooked down to six fields: it bursts in the air, six fields out. */
    portArcSimulateThrown(&l, &w, &path);
    assert(path.impact && path.pointCount == 7);
    assert(fabsf(path.end.x - 6.0f * PORT_ARC_HAND_SPEED) < 0.01f);
    assert(path.end.y > arcRestY(&fx) + 10.0f);
    assert(fabsf(path.end.y - (100.0f + 6.0f * PORT_ARC_HAND_LIFT - 18.0f * PORT_ARC_GRAVITY)) < 0.01f);
}

static void testArcThrownWalls(void)
{
    ArcFixture fx = { 0.0f, TRUE, TRUE, 150.0f, TRUE, 0.0f, FALSE, 0 };
    PortArcWorld w = arcThrownWorld(&fx);
    PortArcLaunch l = arcThrow(PORT_ARC_HAND_SPEED, (f32) PORT_ARC_HAND_FUSE + 1.0f, 3.0f);
    PortArcPath fine;
    PortArcPath coarse;
    s32 coarseCalls;
    s32 i;

    /* In the air the wall turns it back at 0.3 of its speed: it lands
     * short of the wall, coming back. */
    l.pos.y = 20.0f;
    portArcSimulateThrown(&l, &w, &coarse);
    coarseCalls = fx.calls;
    for (i = 0; i < coarse.pointCount; i++) assert(coarse.points[i].x < 150.0f);
    assert(coarse.end.x < 140.0f && coarse.end.x > -400.0f);
    assert(fabsf(coarse.end.y - arcRestY(&fx)) < 0.01f);

    /* Chords land it where a test every field does, for fewer tests. */
    fx.calls = 0;
    l.step = 1.0f;
    portArcSimulateThrown(&l, &w, &fine);
    assert(fx.calls > coarseCalls);
    assert(fabsf(fine.end.x - coarse.end.x) < 0.5f && fabsf(fine.end.z - coarse.end.z) < 0.5f);

    /* Sliding along the floor into the wall, it stops its width short. */
    fx.wallX = 400.0f;
    l = arcThrow(PORT_ARC_HAND_SPEED, (f32) PORT_ARC_HAND_FUSE + 1.0f, 3.0f);
    l.pos.y = arcRestY(&fx) + 0.5f;
    l.vel.y = 0.0f;
    l.vel.x = 12.0f;
    portArcSimulateThrown(&l, &w, &coarse);
    assert(fabsf(coarse.end.x - (400.0f - PORT_ARC_HAND_WIDTH)) < 0.01f);
}

/* A knife or mine, as FlightFor sets one up. */
static PortArcLaunch arcLightThrow(PortArcKind kind, f32 speed)
{
    PortArcLaunch l = arcLaunch(kind, speed, PORT_ARC_HAND_LIFT, 3.0f);

    l.maxFields = 240.0f;
    l.bounce = PORT_ARC_LIGHT_BOUNCE;
    l.floorClearance = (kind == PORT_ARC_KNIFE) ? PORT_ARC_KNIFE_CLEARANCE : PORT_ARC_MINE_CLEARANCE;
    l.restHeight = (kind == PORT_ARC_KNIFE) ? PORT_ARC_KNIFE_REST : PORT_ARC_MINE_REST;
    l.turnFields = (kind == PORT_ARC_KNIFE) ? PORT_ARC_KNIFE_TURN : PORT_ARC_MINE_TURN;
    l.sticks = kind == PORT_ARC_MINE;
    l.strikes = kind == PORT_ARC_KNIFE;
    l.slideFriction = 0.0f;
    return l;
}

/* A mine sticks where it meets a wall, facing out of it; on open floor it
 * is caught over the floor, hops, settles and slides to rest. */
static void testArcThrownMine(void)
{
    ArcFixture fx = { 0.0f, TRUE, TRUE, 300.0f, TRUE, 0.0f, FALSE, 0 };
    PortArcWorld w = arcThrownWorld(&fx);
    PortArcLaunch l = arcLightThrow(PORT_ARC_MINE, PORT_ARC_HAND_SPEED);
    PortArcPath path;

    portArcSimulateThrown(&l, &w, &path);
    assert(path.impact && path.pointCount > 2);
    assert(fabsf(path.end.x - 300.0f) < 0.01f && path.end.y > 50.0f);
    assert(path.endNormal.x == -1.0f && path.endNormal.y == 0.0f);

    /* Dropped from low, it falls slowly enough to be caught in the band. */
    fx.wall = FALSE;
    l.pos.y = 15.0f;
    l.vel.y = 0.0f;
    portArcSimulateThrown(&l, &w, &path);
    assert(path.impact && fabsf(path.end.y - PORT_ARC_MINE_REST) < 0.01f && path.endNormal.y == 1.0f);
    assert(path.end.x > 100.0f);
}

/* A mine reaching the floor sticks only when one tick drops it over twice
 * the band it is caught in (a fall from a ledge); slower, the floor catches
 * it and it hops. The outcome
 * changes once as the drop grows, never back and forth (the arc flickered
 * between the two as the aim moved a hair), and above 60 Hz, where a tick
 * moves it less, the same throw hops. */
static void testArcThrownMineFloor(void)
{
    ArcFixture fx = { 0.0f, TRUE, TRUE, 0.0f, FALSE, 0.0f, FALSE, 0 };
    PortArcWorld w = arcThrownWorld(&fx);
    PortArcLaunch l = arcLightThrow(PORT_ARC_MINE, PORT_ARC_HAND_SPEED);
    PortArcPath path;
    s32 switches = 0;
    s32 wasStuck = FALSE;
    f32 h;

    for (h = 40.0f; h <= 1400.0f; h += 2.0f)
    {
        s32 stuck;

        l.pos.y = h;
        portArcSimulateThrown(&l, &w, &path);
        assert(path.impact && path.endNormal.y == 1.0f);
        stuck = fabsf(path.end.y) < 0.01f;
        assert(stuck || fabsf(path.end.y - PORT_ARC_MINE_REST) < 0.01f);
        if (h > 40.0f && stuck != wasStuck) switches++;
        wasStuck = stuck;
    }
    assert(switches == 1 && wasStuck);   /* hops from low, sticks from high */

    l.pos.y = 1400.0f;
    l.tickStep = 60.0f / 144.0f;
    portArcSimulateThrown(&l, &w, &path);
    assert(path.impact && fabsf(path.end.y - PORT_ARC_MINE_REST) < 0.01f);
}

/* A knife bounces off a wall at a tenth of its speed and, settled, keeps
 * 0.9 of its speed a field: it comes to rest on the floor. */
static void testArcThrownKnife(void)
{
    ArcFixture fx = { 0.0f, TRUE, TRUE, 0.0f, FALSE, 0.0f, FALSE, 0 };
    PortArcWorld w = arcThrownWorld(&fx);
    PortArcLaunch l = arcLightThrow(PORT_ARC_KNIFE, PORT_ARC_KNIFE_SPEED);
    PortArcPath open;
    PortArcPath walled;
    s32 i;

    portArcSimulateThrown(&l, &w, &open);
    assert(open.impact && fabsf(open.end.y - PORT_ARC_KNIFE_REST) < 0.01f);
    for (i = 1; i < open.pointCount; i++) assert(open.points[i].x >= open.points[i - 1].x);
    /* After its turn a slide at v keeps 0.9 a field: about 9v more. */
    assert(open.end.x > open.points[open.pointCount - 1].x - 0.01f);

    fx.wall = TRUE;
    fx.wallX = 200.0f;
    portArcSimulateThrown(&l, &w, &walled);
    assert(walled.impact);
    for (i = 0; i < walled.pointCount; i++) assert(walled.points[i].x < 200.0f);
    assert(walled.end.x < 200.0f - PORT_ARC_HAND_WIDTH + 0.01f);
    assert(fabsf(walled.end.y - PORT_ARC_KNIFE_REST) < 0.01f);
}

/* The fixture's wall at x, as a character standing there. */
static s32 arcChrSegment(void *vctx, const coord3d *from, const coord3d *to, PortArcHit *hit)
{
    const s32 found = arcFixtureSegment(vctx, from, to, hit);

    hit->prop = hit->chr = found && hit->normal.x != 0.0f;
    return found;
}

/* A knife flying into a character ends in them, and the path says which way
 * it flew, for the knife drawn stuck there; one that ends on the floor or a
 * wall does not. */
static void testArcThrownKnifeStrikes(void)
{
    ArcFixture fx = { 0.0f, TRUE, TRUE, 150.0f, TRUE, 0.0f, FALSE, 0 };
    PortArcWorld w = arcThrownWorld(&fx);
    PortArcLaunch l = arcLightThrow(PORT_ARC_KNIFE, PORT_ARC_KNIFE_SPEED);
    PortArcPath path;
    f32 len;

    portArcSimulateThrown(&l, &w, &path);
    assert(path.impact && !path.endChr);
    w.segment = arcChrSegment;
    portArcSimulateThrown(&l, &w, &path);
    assert(path.impact && path.endChr);
    assert(fabsf(path.end.x - 150.0f) < 0.01f && path.end.y > 50.0f);
    len = sqrtf(path.endDir.x * path.endDir.x + path.endDir.y * path.endDir.y + path.endDir.z * path.endDir.z);
    /* Six fields out it still climbs: 5 - 6g up, 25 along. */
    assert(fabsf(len - 1.0f) < 0.001f && fabsf(path.endDir.y - 0.1322f) < 0.005f);
}

/* A shell explodes on a character it meets, and the path says so (for the
 * burst drawn there); a grenade round only turns off one and explodes on
 * the floor beyond, which is not a character hit. */
static void testArcRoundsOnChr(void)
{
    ArcFixture fx = { 0.0f, TRUE, TRUE, 300.0f, TRUE, 0.0f, FALSE, 0 };
    PortArcWorld w = arcWorld(&fx);
    PortArcLaunch shell = arcLaunch(PORT_ARC_SHELL, PORT_ARC_TANK_SPEED, 0.0f, 3.0f);
    PortArcLaunch round = arcLaunch(PORT_ARC_GRENADE, PORT_ARC_GL_SPEED, 0.0f, 2.0f);
    PortArcPath path;

    shell.floorClearance = round.floorClearance = 1.0f;
    w.segment = arcChrSegment;
    portArcSimulate(&shell, &w, &path);
    assert(path.impact && path.endChr && fabsf(path.end.x - 300.0f) < 0.01f);
    portArcSimulate(&round, &w, &path);
    assert(path.impact && !path.endChr && path.end.x < 300.0f);
    w.segment = arcFixtureSegment;
    portArcSimulate(&shell, &w, &path);
    assert(path.impact && !path.endChr);
}

static void testProjectileArcThrown(void)
{
    testArcRoundsOnChr();
    testArcThrownKnifeStrikes();
    testArcThrownRolls();
    testArcThrownFuse();
    testArcThrownWalls();
    testArcThrownTickRate();
    testArcThrownMine();
    testArcThrownMineFloor();
    testArcThrownKnife();
}
