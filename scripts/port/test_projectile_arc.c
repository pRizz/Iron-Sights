/* Included by test_ads.c: Projectile Arc (FORK-069). The option is off by
 * default and a plain toggle; the flight model matches the game's per-tick
 * trapezoidal fall at any tick length; a shell stops at the first surface
 * and a grenade round bounces off walls and ceilings but explodes on a
 * floor; a rocket dips, levels off under thrust, holds its speed cap and
 * falls once its power runs out; and the dash layout stays on the path,
 * never spanning a bounce. */

/* A fake level: a floor plane, an optional wall at x = wallX facing -x,
 * an optional ceiling, and an optional stan floor with no triangles. */
typedef struct ArcFixture {
    f32 floorY;
    s32 levelFloor;
    s32 stanFloor;
    f32 wallX;
    s32 wall;
    f32 ceilingY;
    s32 ceiling;
    s32 calls;
} ArcFixture;

static s32 arcPlaneHit(f32 a, f32 b, f32 plane, f32 *frac)
{
    if ((a - plane) * (b - plane) > 0.0f || a == b) return FALSE;
    *frac = (plane - a) / (b - a);
    return *frac > 0.0f && *frac <= 1.0f;
}

static s32 arcFixtureSegment(void *vctx, const coord3d *from, const coord3d *to, PortArcHit *hit)
{
    ArcFixture *fx = (ArcFixture *) vctx;
    f32 best = 2.0f;
    f32 f;
    coord3d n = { 0.0f, 0.0f, 0.0f };

    fx->calls++;
    if (fx->levelFloor && arcPlaneHit(from->y, to->y, fx->floorY, &f) && f < best) { best = f; n = (coord3d) { 0, 1, 0 }; }
    if (fx->wall && arcPlaneHit(from->x, to->x, fx->wallX, &f) && f < best) { best = f; n = (coord3d) { -1, 0, 0 }; }
    if (fx->ceiling && arcPlaneHit(from->y, to->y, fx->ceilingY, &f) && f < best) { best = f; n = (coord3d) { 0, -1, 0 }; }
    if (best > 1.0f) return FALSE;
    hit->pos.x = from->x + (to->x - from->x) * best;
    hit->pos.y = from->y + (to->y - from->y) * best;
    hit->pos.z = from->z + (to->z - from->z) * best;
    hit->normal = n;
    hit->prop = FALSE;
    hit->chr = FALSE;
    return TRUE;
}

static s32 arcFixtureFloor(void *vctx, const coord3d *pos, f32 *y)
{
    ArcFixture *fx = (ArcFixture *) vctx;

    (void) pos;
    if (!fx->stanFloor) return FALSE;
    *y = fx->floorY;
    return TRUE;
}

static PortArcWorld arcWorld(ArcFixture *fx)
{
    PortArcWorld w = { fx, arcFixtureSegment, arcFixtureFloor };
    return w;
}

static PortArcLaunch arcLaunch(PortArcKind kind, f32 vx, f32 vy, f32 step)
{
    PortArcLaunch l;

    memset(&l, 0, sizeof(l));
    l.kind = kind;
    l.pos = (coord3d) { 0.0f, 100.0f, 0.0f };
    l.vel = (coord3d) { vx, vy, 0.0f };
    l.maxFields = 300.0f;
    l.step = step;
    return l;
}

static void testArcOption(void)
{
    s32 value;

    assert(portOptFind("projectile_arc") >= 0 && portOptCategoryOf(portOptFind("projectile_arc")) == PORTOPT_CAT_GAMEPLAY);
    assert(PORT_PROJECTILE_ARC_DEFAULT == 0);
    assert(portJoyPortOptionGet("projectile_arc", &value) && value == 0 && !portProjectileArcEnabled());
    assert(portJoyPortSetOption("projectile_arc", "1") && portProjectileArcEnabled());
    assert(portJoyPortOptionSet("projectile_arc", 7) && portJoyPortOptionGet("projectile_arc", &value) && value == 1);
    assert(portJoyPortOptionSet("projectile_arc", PORT_PROJECTILE_ARC_DEFAULT) && !portProjectileArcEnabled());
    assert(!portProjectileArcOptionGet("tracer_strength", &value));

    /* Arc Impact Crosshair: its own toggle, on by default. */
    assert(portOptFind("arc_crosshair") >= 0 && portOptCategoryOf(portOptFind("arc_crosshair")) == PORTOPT_CAT_GAMEPLAY);
    assert(PORT_ARC_CROSSHAIR_DEFAULT == 1);
    assert(portJoyPortOptionGet("arc_crosshair", &value) && value == 1 && portArcCrosshairEnabled());
    assert(portJoyPortSetOption("arc_crosshair", "0") && !portArcCrosshairEnabled() && !portProjectileArcEnabled());
    assert(portJoyPortOptionSet("arc_crosshair", 5) && portJoyPortOptionGet("arc_crosshair", &value) && value == 1);
}

/* The crosshair glides to a jumped impact over about a twentieth of a
 * second, the same in one step or many; an impact off the view keeps it at
 * the edge on the line towards it. */
static void testArcCursor(void)
{
    f32 x = 0.0f, y = 0.0f, x2 = 0.0f, y2 = 0.0f;
    s32 i;

    portArcCursorEase(&x, &y, 100.0f, 50.0f, 0.0f);
    assert(x == 0.0f && y == 0.0f);
    portArcCursorEase(&x, &y, 100.0f, 50.0f, 0.05f);
    assert(fabsf(x - 100.0f * (1.0f - expf(-1.0f))) < 0.01f && fabsf(y - x * 0.5f) < 0.01f);
    for (i = 0; i < 6; i++) portArcCursorEase(&x2, &y2, 100.0f, 50.0f, 0.05f / 6.0f);
    assert(fabsf(x2 - x) < 0.01f && fabsf(y2 - y) < 0.01f);
    portArcCursorEase(&x, &y, 100.0f, 50.0f, 1.0f);
    assert(fabsf(x - 100.0f) < 0.01f);

    x = 160.0f; y = 120.0f;                               /* inside: untouched */
    portArcCursorClamp(&x, &y, 0.0f, 0.0f, 320.0f, 240.0f, 6.0f);
    assert(x == 160.0f && y == 120.0f);
    x = 160.0f; y = 1000.0f;                              /* far below: at the bottom edge */
    portArcCursorClamp(&x, &y, 0.0f, 0.0f, 320.0f, 240.0f, 6.0f);
    assert(fabsf(x - 160.0f) < 0.01f && fabsf(y - 234.0f) < 0.01f);
    x = 160.0f + 308.0f; y = 120.0f + 228.0f;             /* off the corner: along the diagonal */
    portArcCursorClamp(&x, &y, 0.0f, 0.0f, 320.0f, 240.0f, 6.0f);
    assert(fabsf(y - 234.0f) < 0.01f && fabsf((x - 160.0f) / (y - 120.0f) - 308.0f / 228.0f) < 0.001f);

    /* Its size falls with distance as perspective's would, from full near
     * to a floor far off. */
    assert(portArcCursorScale(0.0f) == 1.0f && portArcCursorScale(PORT_ARC_CURSOR_NEAR) == 1.0f);
    assert(fabsf(portArcCursorScale(PORT_ARC_CURSOR_NEAR * 2.0f) - 0.5f) < 0.001f);
    assert(portArcCursorScale(700.0f) > portArcCursorScale(800.0f));
    assert(portArcCursorScale(100000.0f) == PORT_ARC_CURSOR_MIN_SCALE);
}

/* propobj.c steps an unpowered round per tick of dt fields:
 * v' = v - g*dt; y += dt*(v + v')/2. That lands on the parabola exactly. */
static void testArcMatchesTicks(void)
{
    const f32 dts[] = { 1.0f, 2.0f, 3.0f, 0.5f };
    PortArcLeg leg = { { 0.0f, 100.0f, 0.0f }, { 20.0f, 9.0f, -4.0f }, 0.0f };
    s32 i;

    for (i = 0; i < 4; i++)
    {
        f32 x = leg.start.x, y = leg.start.y, z = leg.start.z, vy = leg.vel.y, t = 0.0f;
        coord3d p;

        while (t < 120.0f - 1e-4f)
        {
            f32 next = vy - PORT_ARC_GRAVITY * dts[i];
            y += dts[i] * (vy + next) * 0.5f;
            vy = next;
            x += leg.vel.x * dts[i];
            z += leg.vel.z * dts[i];
            t += dts[i];
        }
        portArcPointAt(&leg, t, &p);
        assert(fabsf(p.x - x) < 0.01f && fabsf(p.y - y) < 0.05f && fabsf(p.z - z) < 0.01f);
    }
}

static void testArcShell(void)
{
    ArcFixture fx = { 0.0f, TRUE, FALSE, 0.0f, FALSE, 0.0f, FALSE, 0 };
    PortArcWorld world = arcWorld(&fx);
    PortArcLaunch launch = arcLaunch(PORT_ARC_SHELL, PORT_ARC_TANK_SPEED, 0.0f, 3.0f);
    PortArcPath path;
    f32 tLand = sqrtf(2.0f * 100.0f / PORT_ARC_GRAVITY);

    /* Level flight off a 100-unit ledge lands where the parabola says. */
    portArcSimulate(&launch, &world, &path);
    assert(path.impact && path.legCount == 1);
    assert(fabsf(path.end.y) < 0.01f && path.endNormal.y == 1.0f);
    assert(fabsf(path.end.x - PORT_ARC_TANK_SPEED * tLand) < 6.0f);   /* within the chord's error */
    assert(fabsf(path.legs[0].duration - tLand) < 0.1f);

    /* A wall in the way stops it there, whatever comes after. */
    fx.wall = TRUE;
    fx.wallX = 500.0f;
    portArcSimulate(&launch, &world, &path);
    assert(path.impact && path.legCount == 1 && fabsf(path.end.x - 500.0f) < 0.01f && path.endNormal.x == -1.0f);

    /* Nothing to hit: it flies on past the prediction with no marker. */
    fx.wall = FALSE;
    fx.levelFloor = FALSE;
    portArcSimulate(&launch, &world, &path);
    assert(!path.impact && path.legCount == 1 && fabsf(path.legs[0].duration - launch.maxFields) < 0.001f);

    /* The stan floor alone (no triangles under it) still ends the flight. */
    fx.stanFloor = TRUE;
    portArcSimulate(&launch, &world, &path);
    assert(path.impact && fabsf(path.end.y) < 0.001f && path.end.x > 0.0f);

    /* The floor test lands a coarse prediction exactly where field-by-field
     * steps (the game's) do, with the round's clearance over the floor. */
    {
        PortArcPath fine;
        PortArcLaunch one = launch;

        launch.floorClearance = one.floorClearance = 8.2f;
        one.step = 1.0f;
        portArcSimulate(&launch, &world, &path);
        portArcSimulate(&one, &world, &fine);
        assert(path.impact && fine.impact && fabsf(path.end.x - fine.end.x) < 0.01f);
        assert(fabsf(path.legs[0].duration - fine.legs[0].duration) < 0.001f);
        /* the clearance lands it earlier than the bare floor would */
        assert(path.legs[0].duration < sqrtf(2.0f * 100.0f / PORT_ARC_GRAVITY));
        launch.floorClearance = 0.0f;
    }
}

static void testArcGrenade(void)
{
    ArcFixture fx = { 0.0f, TRUE, FALSE, 300.0f, TRUE, 0.0f, FALSE, 0 };
    PortArcWorld world = arcWorld(&fx);
    PortArcLaunch launch = arcLaunch(PORT_ARC_GRENADE, PORT_ARC_GL_SPEED, 4.0f, 2.0f);
    PortArcPath path;

    /* Off the wall at x = 300 and back, then it explodes on the floor. */
    portArcSimulate(&launch, &world, &path);
    assert(path.legCount == 2 && path.impact);
    assert(fabsf(path.legs[1].vel.x + PORT_ARC_GL_BOUNCE * PORT_ARC_GL_SPEED) < 0.01f);
    assert(path.legs[1].start.x < 300.0f && path.legs[1].start.x > 299.0f);
    assert(fabsf(path.end.y) < 0.01f && path.end.x < 300.0f && path.endNormal.y == 1.0f);

    /* A ceiling hit while rising turns it down without exploding. */
    fx.wall = FALSE;
    fx.ceiling = TRUE;
    fx.ceilingY = 140.0f;
    launch.vel.y = 15.0f;
    portArcSimulate(&launch, &world, &path);
    assert(path.legCount == 2 && path.impact && path.legs[1].vel.y < 0.0f && fabsf(path.end.y) < 0.01f);

    /* A shell from the same launch stops at the ceiling instead. */
    launch.kind = PORT_ARC_SHELL;
    portArcSimulate(&launch, &world, &path);
    assert(path.legCount == 1 && path.impact && fabsf(path.end.y - 140.0f) < 0.01f);
}

static void testArcRocket(void)
{
    ArcFixture fx = { 0.0f, FALSE, FALSE, 0.0f, FALSE, 0.0f, FALSE, 0 };
    PortArcWorld world = arcWorld(&fx);
    PortArcLaunch launch = arcLaunch(PORT_ARC_ROCKET, PORT_ARC_ROCKET_THRUST * PORT_ARC_ROCKET_CADENCE, 0.0f, 1.0f);
    PortArcPath path;
    PortArcDash dashes[16];
    f32 low = 1e9f;
    f32 dx;
    s32 i, n;

    launch.accel = (coord3d) { PORT_ARC_ROCKET_THRUST, 0.0f, 0.0f };
    launch.maxFields = 200.0f;
    portArcSimulateRocket(&launch, &world, &path);
    assert(!path.impact && path.legCount == 0 && path.pointCount == 201);
    for (i = 0; i < path.pointCount; i++) if (path.points[i].y < low) low = path.points[i].y;
    /* A level launch dips while its lift ramps up, then levels off. */
    assert(low < 99.0f && path.points[0].y == 100.0f);
    assert(fabsf(path.points[200].y - path.points[190].y) < 0.01f);
    /* Thrust stops at the speed cap and the speed holds there. */
    dx = path.points[200].x - path.points[199].x;
    assert(dx > 160.0f && dx < 170.0f);   /* the cap counts the slight sink too */
    assert(fabsf((path.points[199].x - path.points[198].x) - dx) < 0.001f);
    /* It draws as dashes along its polyline. */
    n = portArcDashes(&path, 36.0f, 24.0f, 0.0f, dashes, 16);
    assert(n == 16 && dashes[0].a.x == 0.0f && dashes[1].distA > dashes[0].distA);

    /* A wall stops it where it stands. */
    fx.wall = TRUE;
    fx.wallX = 2000.0f;
    portArcSimulateRocket(&launch, &world, &path);
    assert(path.impact && fabsf(path.end.x - 2000.0f) < 0.01f && path.endNormal.x == -1.0f);
    assert(fabsf(path.points[path.pointCount - 1].x - 2000.0f) < 0.01f);
    /* Testing collisions on three-field chords finds the same hit and keeps
     * no positions past it. */
    {
        PortArcPath chorded;

        launch.step = 3.0f;
        portArcSimulateRocket(&launch, &world, &chorded);
        launch.step = 1.0f;
        assert(chorded.impact && fabsf(chorded.end.x - path.end.x) < 0.01f && fabsf(chorded.end.y - path.end.y) < 0.01f);
        assert(chorded.pointCount == path.pointCount);
        for (i = 0; i < chorded.pointCount; i++) assert(chorded.points[i].x <= 2000.0f + 0.01f);
    }

    /* With its power gone after GRENADE_SMOKE_FRAMES fields it falls to a
     * floor just under the lowest it flew. */
    fx.wall = FALSE;
    fx.levelFloor = TRUE;
    fx.floorY = low - 5.0f;
    launch.maxFields = 315.0f;
    portArcSimulateRocket(&launch, &world, &path);
    assert(path.impact && fabsf(path.end.y - fx.floorY) < 0.01f);
    assert(path.pointCount > (s32) PORT_ARC_ROCKET_POWER_FIELDS && path.pointCount <= PORT_ARC_MAX_POINTS);
}

static void testArcDashes(void)
{
    PortArcPath path;
    PortArcDash dashes[64];
    s32 n, i;
    f32 total = 0.0f;

    memset(&path, 0, sizeof(path));
    /* Two straight 600-unit legs (no fall is negligible over 0.1 fields). */
    path.legCount = 2;
    path.legs[0] = (PortArcLeg) { { 0, 0, 0 }, { 6000.0f, 0, 0 }, 0.1f };
    path.legs[1] = (PortArcLeg) { { 600.0f, 0, 0 }, { 0, 0, 6000.0f }, 0.1f };
    n = portArcDashes(&path, 36.0f, 24.0f, 0.0f, dashes, 64);
    assert(n >= 19 && n <= 22);
    for (i = 0; i < n; i++)
    {
        f32 dx = dashes[i].b.x - dashes[i].a.x, dz = dashes[i].b.z - dashes[i].a.z;
        f32 len = sqrtf(dx * dx + dz * dz);

        assert(len <= 36.0f + 0.05f);
        /* None cuts the corner: each lies on one leg or the other. */
        assert(fabsf(dz) < 0.01f || fabsf(dx) < 0.01f);
        total += len;
        if (i > 0) assert(dashes[i].distA > dashes[i - 1].distA);
    }
    assert(total > 0.55f * 1200.0f && total < 0.65f * 1200.0f);

    /* A later phase moves the pattern forward along the path. */
    n = portArcDashes(&path, 36.0f, 24.0f, 40.0f, dashes, 64);
    assert(n > 0 && fabsf(dashes[0].a.x) < 0.01f && fabsf(dashes[0].b.x - 16.0f) < 0.3f);
    assert(fabsf(dashes[1].distA - 40.0f) < 0.3f);

    /* The output cap holds. */
    assert(portArcDashes(&path, 36.0f, 24.0f, 0.0f, dashes, 5) == 5);
}

static void testArcLook(void)
{
    assert(portArcFadeIn(0.0f) == 0.0f && portArcFadeIn(1000.0f) == 1.0f);
    assert(portArcFadeIn(100.0f) < portArcFadeIn(150.0f));
    /* Near, the world width rules; far, the on-screen minimum does. */
    assert(portArcScreenWidth(2.5f, 10.0f, 0.577f, 480.0f, 2.0f) == 2.5f);
    assert(portArcScreenWidth(2.5f, 10000.0f, 0.577f, 480.0f, 2.0f) > 40.0f);
    {   /* A struck knife lies at least 50 degrees off the line of sight. */
        const coord3d sight = { 1.0f, 0.0f, 0.0f };
        const coord3d across = { 0.0f, 0.0f, 1.0f };
        const coord3d falling = { 0.8f, -0.6f, 0.0f };
        coord3d axis;

        portArcKnifeAxis(&across, &sight, &axis);
        assert(axis.z == 1.0f && axis.x == 0.0f);
        portArcKnifeAxis(&sight, &sight, &axis);           /* end-on: tipped down */
        assert(fabsf(axis.x - 0.64278764f) < 0.001f && axis.y < -0.76f);
        portArcKnifeAxis(&falling, &sight, &axis);         /* 37 degrees: turned to 50 */
        assert(fabsf(axis.x - 0.64278764f) < 0.001f && fabsf(axis.y + 0.76604444f) < 0.001f);
        assert(fabsf(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z - 1.0f) < 0.001f);
    }
}

static void testProjectileArc(void)
{
    testArcOption();
    testArcCursor();
    testArcMatchesTicks();
    testArcShell();
    testArcGrenade();
    testArcRocket();
    testArcDashes();
    testArcLook();
}
