/* Debrief hit map core (FORK-048): placement, pose, layout and per-part stats without a ROM. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../port/src/port_hitmap.c"

void osSyncPrintf(const char *fmt, ...)
{
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args); abort();
}

static int near(f32 a, f32 b, f32 tolerance)
{
    const f32 d = a - b;
    return d < tolerance && d > -tolerance;
}

static struct bbox makeBox(f32 x0, f32 x1, f32 y0, f32 y1, f32 z0, f32 z1)
{
    struct bbox b;

    b.xmin = x0; b.xmax = x1; b.ymin = y0; b.ymax = y1; b.zmin = z0; b.zmax = z1;
    return b;
}

static void testRayToLocalUndoesScaledTurn(void)
{
    /* Arrange: a joint turned 90 degrees about y, scaled 0.1, at (5, 6, 7). */
    Mtxf m = { 0 };
    coord3d origin = { { { 5.0f + 0.1f * 3.0f, 6.0f + 0.1f * 4.0f, 7.0f - 0.1f * 2.0f } } };
    coord3d dir = { { { 0.0f, 0.0f, -1.0f } } };
    coord3d lo, ld;

    m.m[0][2] = -0.1f; /* local +x -> view -z */
    m.m[1][1] = 0.1f;
    m.m[2][0] = 0.1f;  /* local +z -> view +x */
    m.m[3][0] = 5.0f; m.m[3][1] = 6.0f; m.m[3][2] = 7.0f; m.m[3][3] = 1.0f;
    /* Act */
    assert(portHitMapRayToLocal(&m, &origin, &dir, &lo, &ld));
    /* Assert: the view point (local 2, 4, 3) and the view -z direction (local +x, scaled). */
    assert(near(lo.x, 2.0f, 1e-3f) && near(lo.y, 4.0f, 1e-3f) && near(lo.z, 3.0f, 1e-3f));
    assert(near(ld.x, 10.0f, 1e-3f) && near(ld.y, 0.0f, 1e-3f) && near(ld.z, 0.0f, 1e-3f));
}

static void testBoxEntryFindsTheSurfaceCrossed(void)
{
    /* Arrange */
    const struct bbox b = makeBox(-10, 10, 0, 100, -20, 30);
    coord3d from = { { { 2.0f, 50.0f, 500.0f } } }, ahead = { { { 0.0f, 0.0f, -1.0f } } };
    coord3d inside = { { { 0.0f, 50.0f, 0.0f } } }, aside = { { { 50.0f, 50.0f, 500.0f } } };
    coord3d entry;

    /* Act / Assert: from the front, the ray enters through z max. */
    assert(portHitMapBoxEntry(&b, &from, &ahead, &entry));
    assert(near(entry.x, 2.0f, 1e-4f) && near(entry.y, 50.0f, 1e-4f) && near(entry.z, 30.0f, 1e-4f));
    /* A ray starting inside reports the surface behind it. */
    assert(portHitMapBoxEntry(&b, &inside, &ahead, &entry) && near(entry.z, 30.0f, 1e-4f));
    /* A ray beside the box misses it. */
    assert(!portHitMapBoxEntry(&b, &aside, &ahead, &entry));
}

static void testPlaceRecordsBoxFractionsAndSide(void)
{
    /* Arrange */
    const struct bbox b = makeBox(-10, 10, 0, 100, -20, 30);
    const coord3d front = { { { -10.0f, 25.0f, 30.0f } } }, back = { { { 10.0f, 100.0f, -20.0f } } };
    const coord3d sideRear = { { { -10.0f, 50.0f, -5.0f } } };
    const coord3d toBack = { { { 0.0f, 0.0f, -1.0f } } }, toFront = { { { 0.0f, 0.0f, 1.0f } } };
    const coord3d across = { { { 1.0f, 0.0f, 0.0f } } };
    /* Act */
    const PortHitMapHit a = portHitMapPlace(3, &b, &front, &toBack);
    const PortHitMapHit c = portHitMapPlace(4, &b, &back, &toFront);
    const PortHitMapHit s = portHitMapPlace(5, &b, &sideRear, &across);

    /* Assert: fractions across x and up y; the half of the box entered picks the side. */
    assert(a.box == 3 && a.u == 0 && a.v == 16384 && !(a.flags & PORT_HITMAP_BACK));
    assert(c.u == 65535 && c.v == 65535 && (c.flags & PORT_HITMAP_BACK));
    assert(s.flags & PORT_HITMAP_BACK);
}

static void testFigureFindPrefersTheOrdinal(void)
{
    /* Arrange */
    PortHitMapFigure f = { 0 };

    f.count = 3;
    f.boxes[0].part = HIT_CHEST; f.boxes[0].ordinal = 0;
    f.boxes[1].part = HIT_HEAD;  f.boxes[1].ordinal = 0;
    f.boxes[2].part = HIT_CHEST; f.boxes[2].ordinal = 1;
    /* Act / Assert: the matching ordinal, else the part's first box, else none. */
    assert(portHitMapFigureFind(&f, HIT_CHEST, 1) == 2);
    assert(portHitMapFigureFind(&f, HIT_CHEST, 7) == 0);
    assert(portHitMapFigureFind(&f, HIT_PELVIS, 0) == -1);
}

static void addBox(PortHitMapFigure *f, s32 part, f32 jx, f32 jy, f32 x0, f32 x1, f32 y0, f32 y1)
{
    PortHitMapBox *b = &f->boxes[f->count++];
    const PortHitMapBox empty = { 0 };

    *b = empty;
    b->part = part; b->jointX = jx; b->jointY = jy;
    b->x0 = x0; b->x1 = x1; b->y0 = y0; b->y1 = y1;
}

/* GE's unanimated skeleton: limbs straight out to the sides along local x. */
static PortHitMapFigure starfish(void)
{
    PortHitMapFigure f = { 0 };

    addBox(&f, HIT_PELVIS, 0, 0, -190, 190, -200, 80);
    addBox(&f, HIT_CHEST, 0, 40, -210, 210, 20, 430);
    addBox(&f, HIT_HEAD, 0, 520, -80, 80, -100, 200);
    addBox(&f, HIT_LEFT_THIGH, 100, -100, -100, 440, -100, 120);
    addBox(&f, HIT_LEFT_LEG, 515, -110, 0, 500, -80, 90);
    addBox(&f, HIT_RIGHT_THIGH, -100, -100, -440, 100, -100, 120);
    addBox(&f, HIT_LEFT_SHOULDER, 200, 320, -140, 210, -70, 120);
    addBox(&f, HIT_RIGHT_SHOULDER, -200, 320, -210, 140, -70, 120);
    return f;
}

static void testPoseHangsLegsAndLowersArms(void)
{
    /* Arrange */
    PortHitMapFigure f = starfish();

    /* Act */
    portHitMapPose(&f);
    /* Assert: the torso keeps its place; each leg chain hangs straight down
     * from its hip, the knee below the thigh; arms come down but stay out. */
    assert(near(f.boxes[0].cx, 0.0f, 1e-3f) && near(f.boxes[0].cy, -60.0f, 1e-3f));
    assert(near(f.boxes[3].cx, 110.0f, 1e-2f) && near(f.boxes[3].cy, -270.0f, 1e-2f));
    assert(near(f.boxes[3].ay, -1.0f, 1e-4f));
    /* knee centre (765, -105) about the hip (100, -100): (100 - 5, -100 - 665) */
    assert(near(f.boxes[4].cx, 95.0f, 1e-2f) && near(f.boxes[4].cy, -765.0f, 1e-2f));
    assert(near(f.boxes[5].ay, 1.0f, 1e-4f) && f.boxes[5].cx < 0.0f && f.boxes[5].cy < -100.0f);
    assert(f.boxes[6].cy < 320.0f && f.boxes[6].cx > 200.0f && f.boxes[6].ay < -0.8f);
    assert(f.boxes[7].cx < -200.0f && f.boxes[7].ay > 0.8f);
}

static void testLayoutFitsAndFacesTheViewer(void)
{
    /* Arrange */
    PortHitMapFigure f = starfish();
    PortHitMapView front, back;
    f32 leftX, rightX;

    portHitMapPose(&f);
    /* Act */
    front = portHitMapLayout(&f, FALSE, 100.0f, 50.0f, 60.0f, 120.0f);
    back = portHitMapLayout(&f, TRUE, 100.0f, 50.0f, 60.0f, 120.0f);
    leftX = front.originX + front.scaleX * f.boxes[6].cx;
    rightX = front.originX + front.scaleX * f.boxes[7].cx;
    /* Assert: it fits the rectangle; facing us, the left shoulder is on the
     * screen's right; the back view mirrors that; y runs down the screen. */
    assert(front.width <= 60.0f + 1e-3f && front.height <= 120.0f + 1e-3f);
    assert(near(front.width, 60.0f, 1e-3f) || near(front.height, 120.0f, 1e-3f));
    assert(leftX > rightX);
    assert(near(back.scaleX, -front.scaleX, 1e-6f) && front.scaleY < 0.0f);
    assert(front.originY + front.scaleY * f.boxes[2].cy < front.originY + front.scaleY * f.boxes[0].cy);
}

static void testHitPointFollowsThePosedBox(void)
{
    /* Arrange */
    PortHitMapFigure f = starfish();
    const PortHitMapHit middle = { 3, 0, 32768, 32768 }, end = { 3, 0, 65535, 32768 };
    f32 x, y, ex, ey;

    portHitMapPose(&f);
    /* Act */
    portHitMapHitPoint(&f, &middle, &x, &y);
    portHitMapHitPoint(&f, &end, &ex, &ey);
    /* Assert: the box centre, and its far end along the bone is down the leg. */
    assert(near(x, f.boxes[3].cx, 0.1f) && near(y, f.boxes[3].cy, 0.1f));
    assert(near(ex, x, 0.1f) && near(ey, y - f.boxes[3].hx, 0.1f));
}

static void testCornerHitsStayOnTheOval(void)
{
    /* Arrange: hits in each corner of the chest box and one halfway up a side. */
    PortHitMapFigure f = starfish();
    const PortHitMapBox *b = &f.boxes[1];
    const u16 ends[2] = { 0, 65535 };
    s32 i;

    portHitMapPose(&f);
    for (i = 0; i < 4; i++) {
        const PortHitMapHit corner = { 1, 0, ends[i & 1], ends[i >> 1] };
        f32 x, y, ex, ey;

        /* Act */
        portHitMapHitPoint(&f, &corner, &x, &y);
        ex = (x - b->cx) / b->hx;
        ey = (y - b->cy) / b->hy;
        /* Assert: on the inscribed ellipse's rim, never outside it. */
        assert(ex * ex + ey * ey <= 1.0f + 1e-3f && ex * ex + ey * ey > 0.95f);
    }
    {
        const PortHitMapHit side = { 1, 0, 65535, 32768 };
        f32 x, y;

        portHitMapHitPoint(&f, &side, &x, &y);
        assert(near(x, b->cx + b->hx, 0.1f) && near(y, b->cy, 0.1f));
    }
}

static void testGroupsJoinLeftAndRight(void)
{
    /* Arrange, act and assert: each side's part shares one row; others have none. */
    assert(portHitMapGroup(HIT_HEAD) == PORT_HITMAP_GROUP_HEAD);
    assert(portHitMapGroup(HIT_LEFT_SHOULDER) == PORT_HITMAP_GROUP_UPPER_ARM);
    assert(portHitMapGroup(HIT_RIGHT_SHOULDER) == PORT_HITMAP_GROUP_UPPER_ARM);
    assert(portHitMapGroup(HIT_LEFT_ARM) == PORT_HITMAP_GROUP_FOREARM);
    assert(portHitMapGroup(HIT_RIGHT_HAND) == PORT_HITMAP_GROUP_HAND);
    assert(portHitMapGroup(HIT_LEFT_LEG) == PORT_HITMAP_GROUP_SHIN);
    assert(portHitMapGroup(HIT_RIGHT_FOOT) == PORT_HITMAP_GROUP_FOOT);
    assert(portHitMapGroup(HIT_PELVIS) == PORT_HITMAP_GROUP_PELVIS);
    assert(portHitMapGroup(HIT_GUN) < 0 && portHitMapGroup(HIT_HAT) < 0 && portHitMapGroup(HIT_NULL_PART) < 0);
}

static void testDamageUnitsMeasureHealthAndOverkill(void)
{
    u16 damage, overkill;

    /* A quarter of a 4.0 health bar, no kill. */
    portHitMapDamageUnits(1.0f, 2.0f, 4.0f, &damage, &overkill);
    assert(damage == 250 && overkill == 0);
    /* A kill shot with 0.5 health left that dealt 2.0: 1.5 went past the top. */
    portHitMapDamageUnits(3.5f, 5.5f, 4.0f, &damage, &overkill);
    assert(damage == 500 && overkill == 375);
    /* Damage on a character already past its health is all overkill. */
    portHitMapDamageUnits(5.0f, 6.0f, 4.0f, &damage, &overkill);
    assert(damage == 250 && overkill == 250);
    /* No damage, or no health to measure against, records nothing; a huge hit clamps. */
    portHitMapDamageUnits(2.0f, 2.0f, 4.0f, &damage, &overkill);
    assert(damage == 0 && overkill == 0);
    portHitMapDamageUnits(0.0f, 1.0f, 0.0f, &damage, &overkill);
    assert(damage == 0 && overkill == 0);
    portHitMapDamageUnits(0.0f, 1000.0f, 1.0f, &damage, &overkill);
    assert(damage == 65535 && overkill == 65535);
}

static void testStatsCountByRow(void)
{
    /* Arrange: a head, both shoulders and a gun box; hits on each, one kill. */
    PortHitMapFigure f = { 0 };
    PortHitMapStats s;
    PortHitMapHit hits[5] = { { 0 } };
    const s32 parts[4] = { HIT_HEAD, HIT_LEFT_SHOULDER, HIT_RIGHT_SHOULDER, HIT_GUN };
    s32 i;

    f.count = 4;
    for (i = 0; i < 4; i++) f.boxes[i].part = parts[i];
    hits[0].box = 0; hits[0].damage = 1000; hits[0].overkill = 400; hits[0].flags = PORT_HITMAP_KILL;
    hits[1].box = 1; hits[1].damage = 250;
    hits[2].box = 2; hits[2].damage = 250; hits[2].flags = PORT_HITMAP_BACK;
    hits[3].box = 3; hits[3].damage = 999; /* not a body part */
    hits[4].box = 9;                       /* past the figure */
    /* Act */
    portHitMapStatsCompute(&f, hits, 5, &s);
    /* Assert: both shoulders count as upper arms; only body parts count. */
    assert(s.hits == 3 && s.kills == 1 && s.damage == 1500 && s.overkill == 400);
    assert(s.groups[PORT_HITMAP_GROUP_HEAD].hits == 1 && s.groups[PORT_HITMAP_GROUP_HEAD].kills == 1);
    assert(s.groups[PORT_HITMAP_GROUP_HEAD].damage == 1000);
    assert(s.groups[PORT_HITMAP_GROUP_UPPER_ARM].hits == 2 && s.groups[PORT_HITMAP_GROUP_UPPER_ARM].damage == 500);
    assert(s.groups[PORT_HITMAP_GROUP_FOOT].hits == 0);
}

static void testPlaceStartsWithoutDamage(void)
{
    /* Arrange */
    const struct bbox b = makeBox(0.0f, 10.0f, 0.0f, 10.0f, 0.0f, 10.0f);
    const coord3d entry = { { { 5.0f, 5.0f, 10.0f } } }, dir = { { { 0.0f, 0.0f, -1.0f } } };
    /* Act */
    const PortHitMapHit hit = portHitMapPlace(2, &b, &entry, &dir);
    /* Assert: portHitMapDamage fills these in once the game applies the damage. */
    assert(hit.damage == 0 && hit.overkill == 0);
}

int main(void)
{
    testRayToLocalUndoesScaledTurn();
    testBoxEntryFindsTheSurfaceCrossed();
    testPlaceRecordsBoxFractionsAndSide();
    testFigureFindPrefersTheOrdinal();
    testPoseHangsLegsAndLowersArms();
    testLayoutFitsAndFacesTheViewer();
    testHitPointFollowsThePosedBox();
    testCornerHitsStayOnTheOval();
    testGroupsJoinLeftAndRight();
    testDamageUnitsMeasureHealthAndOverkill();
    testStatsCountByRow();
    testPlaceStartsWithoutDamage();
    puts("hit map: ok");
    return 0;
}
