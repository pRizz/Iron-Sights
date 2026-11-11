/* Projectile Arc (FORK-069), the pure part: the option, the flight model and
 * the dash layout. See port_projectile_arc.h.
 *
 * The flight model follows the airborne-projectile tick in propobj.c
 * (objectTick's PROJECTILEFLAG_AIRBORNE branch). An unpowered round falls at
 * PROP_PROJECTILE_GRAVITY_MODIFIER per field^2 under a trapezoidal step,
 * which is exact for constant acceleration, so the path is the same
 * parabola at any tick rate and can be sampled at any time. Each collision
 * step tests the straight chord, as the game does each tick:
 *
 * - A tank shell (ITEM_ROCKETROUND) explodes on the first wall, floor,
 *   object or character it touches, and when it drops below the floor.
 * - A grenade round (ITEM_GRENADEROUND) reflects its speed off what it hits
 *   with restitution 0.3. It explodes when it was falling and the bounce
 *   sends it up or leaves it no lower (a floor), or when it drops below the
 *   floor; walls, ceilings and characters only turn it. Hitting an object
 *   or character leaves it where the tick began, at the tick's new height,
 *   as handles_projectile_motion does. */
#include <string.h>
#include <math.h>
#include "port_projectile_arc.h"

/* include/ puts the N64 C headers ahead of the C library's, and they
 * declare only a few of these. */
extern int strcmp(const char *, const char *);
extern void *memset(void *, int, size_t);
extern float fabsf(float);
extern float fmodf(float, float);
extern float ceilf(float);
/* include/math.h leaves expf inside #if 0: undeclared, the game build would
 * take it as returning int and leave its float on the x87 stack each call,
 * until the stack overflowed and later sums came out NaN (the gun's pose did,
 * and it vanished). */
extern float expf(float);

static s32 projectileArc = PORT_PROJECTILE_ARC_DEFAULT;
static s32 arcCrosshair = PORT_ARC_CROSSHAIR_DEFAULT;

static s32 *arcOptionFor(const char *key)
{
    if (strcmp(key, "projectile_arc") == 0) return &projectileArc;
    if (strcmp(key, "arc_crosshair") == 0) return &arcCrosshair;
    return NULL;
}

s32 portProjectileArcOptionGet(const char *key, s32 *value)
{
    s32 *option = arcOptionFor(key);

    if (option == NULL) return FALSE;
    *value = *option;
    return TRUE;
}

s32 portProjectileArcOptionSet(const char *key, s32 value)
{
    s32 *option = arcOptionFor(key);

    if (option == NULL) return FALSE;
    *option = value != 0;
    return TRUE;
}

s32 portProjectileArcEnabled(void) { return projectileArc; }
s32 portArcCrosshairEnabled(void) { return arcCrosshair; }

/* The Arc Impact Crosshair follows its target over about a twentieth of a
 * second, so an impact that jumps (a bounce off a corner, a grenade's roll
 * turning) glides there instead. Exponential, so any frame rate eases the
 * same. */
void portArcCursorEase(f32 *x, f32 *y, f32 toX, f32 toY, f32 seconds)
{
    const f32 keep = (seconds > 0.0f) ? expf(-seconds / 0.05f) : 1.0f;

    *x = toX + (*x - toX) * keep;
    *y = toY + (*y - toY) * keep;
}

/* A nearer impact draws a larger crosshair, as an object there would look:
 * its size falls as 1/distance past PORT_ARC_CURSOR_NEAR, held at
 * PORT_ARC_CURSOR_MIN_SCALE so a far one never vanishes. */
f32 portArcCursorScale(f32 distance)
{
    f32 scale;

    if (!(distance > PORT_ARC_CURSOR_NEAR)) return 1.0f;
    scale = PORT_ARC_CURSOR_NEAR / distance;
    return scale < PORT_ARC_CURSOR_MIN_SCALE ? PORT_ARC_CURSOR_MIN_SCALE : scale;
}

/* An impact outside the view keeps the crosshair at the view's edge, on the
 * line from the centre towards it, so it still points the way. */
void portArcCursorClamp(f32 *x, f32 *y, f32 left, f32 top, f32 width, f32 height, f32 margin)
{
    const f32 cx = left + width * 0.5f;
    const f32 cy = top + height * 0.5f;
    const f32 hx = width * 0.5f - margin;
    const f32 hy = height * 0.5f - margin;
    const f32 dx = *x - cx;
    const f32 dy = *y - cy;
    f32 scale = 1.0f;

    if (hx <= 0.0f || hy <= 0.0f) return;
    if (fabsf(dx) * scale > hx) scale = hx / fabsf(dx);
    if (fabsf(dy) * scale > hy) scale = hy / fabsf(dy);
    *x = cx + dx * scale;
    *y = cy + dy * scale;
}

void portArcPointAt(const PortArcLeg *leg, f32 t, coord3d *out)
{
    out->x = leg->start.x + leg->vel.x * t;
    out->y = leg->start.y + leg->vel.y * t - 0.5f * PORT_ARC_GRAVITY * t * t;
    out->z = leg->start.z + leg->vel.z * t;
}

static f32 dot3(const coord3d *a, const coord3d *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static f32 dist3(const coord3d *a, const coord3d *b)
{
    coord3d d = { a->x - b->x, a->y - b->y, a->z - b->z };
    return sqrtf(dot3(&d, &d));
}

/* How far along from..to the hit lies, 0..1. */
static f32 chordFraction(const coord3d *from, const coord3d *to, const coord3d *hit)
{
    coord3d chord = { to->x - from->x, to->y - from->y, to->z - from->z };
    coord3d rel = { hit->x - from->x, hit->y - from->y, hit->z - from->z };
    f32 len2 = dot3(&chord, &chord);
    f32 f;

    if (!(len2 > 0.0f)) return 0.0f;
    f = dot3(&rel, &chord) / len2;
    if (f < 0.0f) return 0.0f;
    return f > 1.0f ? 1.0f : f;
}

/* handles_projectile_motion backs a level hit off by 0.1 units towards the
 * start (or half the step when the step is shorter than that). */
static void backOff(const coord3d *from, const coord3d *to, const coord3d *hit, coord3d *out)
{
    f32 len = dist3(from, to);
    f32 factor = (len > 0.1f) ? 0.1f / len : 0.5f;

    out->x = hit->x - factor * (to->x - from->x);
    out->y = hit->y - factor * (to->y - from->y);
    out->z = hit->z - factor * (to->z - from->z);
}

static s32 closeLeg(PortArcPath *out, const coord3d *start, const coord3d *vel, f32 duration)
{
    PortArcLeg *leg;

    if (out->legCount >= PORT_ARC_MAX_LEGS) return FALSE;
    leg = &out->legs[out->legCount++];
    leg->start = *start;
    leg->vel = *vel;
    leg->duration = duration > 0.0f ? duration : 0.0f;
    return TRUE;
}

static void endAt(PortArcPath *out, s32 impact, const coord3d *pos, const coord3d *normal)
{
    out->impact = impact;
    out->end = *pos;
    out->endNormal = *normal;
}

/* The flight state between steps: the current leg and the time into it. */
typedef struct ArcFlight {
    coord3d legStart;
    coord3d legVel;
    f32 legT;
    coord3d pos;
} ArcFlight;

/* A grenade round meets something: bounce it, or end the path where it
 * explodes. Returns TRUE while it flies on. */
static s32 grenadeBounce(ArcFlight *fl, const coord3d *next, f32 dt, const PortArcHit *hit, PortArcPath *out)
{
    coord3d vel = fl->legVel;
    coord3d newPos;
    f32 prevVy;
    f32 d;
    s32 explode;

    vel.y -= PORT_ARC_GRAVITY * (fl->legT + dt);   /* the speed after this tick's fall */
    prevVy = vel.y;
    d = -(PORT_ARC_GL_BOUNCE + 1.0f) * dot3(&vel, &hit->normal);
    vel.x += d * hit->normal.x;
    vel.y += d * hit->normal.y;
    vel.z += d * hit->normal.z;

    if (hit->prop)
    {
        newPos.x = fl->pos.x;
        newPos.y = next->y;
        newPos.z = fl->pos.z;
    }
    else
    {
        backOff(&fl->pos, next, &hit->pos, &newPos);
    }

    explode = prevVy <= 0.0f && (vel.y >= 0.0f || fl->pos.y <= newPos.y);
    closeLeg(out, &fl->legStart, &fl->legVel, fl->legT + chordFraction(&fl->pos, next, &hit->pos) * dt);
    if (explode || out->legCount >= PORT_ARC_MAX_LEGS)
    {
        endAt(out, explode, &hit->pos, &hit->normal);
        out->endChr = explode && hit->chr;   /* it came down on a character */
        return FALSE;
    }

    fl->legStart = newPos;
    fl->legVel = vel;
    fl->legT = 0.0f;
    fl->pos = newPos;
    return TRUE;
}

/* After a clear step: whether the round is now low enough over the floor
 * to explode. */
static s32 belowFloor(const PortArcLaunch *launch, const PortArcWorld *world, const coord3d *pos, f32 *floorY)
{
    if (!world->floorBelow(world->ctx, pos, floorY)) return FALSE;
    return pos->y < *floorY + launch->floorClearance;
}

static void land(ArcFlight *fl, f32 floorY, PortArcPath *out)
{
    static const coord3d up = { 0.0f, 1.0f, 0.0f };
    coord3d ground;

    ground.x = fl->pos.x;
    ground.y = floorY;
    ground.z = fl->pos.z;
    closeLeg(out, &fl->legStart, &fl->legVel, fl->legT);
    endAt(out, TRUE, &ground, &up);
}

/* Steps of launch->step fields; level and object hits are found along each
 * step's chord at any length, but the floor test only samples the step's
 * end, so the step that first dips under the floor is flown again a field
 * at a time, as the game moves a round, to land where the game's does. */
void portArcSimulate(const PortArcLaunch *launch, const PortArcWorld *world, PortArcPath *out)
{
    static const coord3d up = { 0.0f, 1.0f, 0.0f };
    ArcFlight fl;
    f32 t = 0.0f;
    f32 step = launch->step > 0.0f ? launch->step : 1.0f;

    memset(out, 0, sizeof(*out));
    fl.legStart = launch->pos;
    fl.legVel = launch->vel;
    fl.legT = 0.0f;
    fl.pos = launch->pos;

    while (t < launch->maxFields)
    {
        const ArcFlight before = fl;
        const f32 tBefore = t;
        PortArcLeg leg;
        PortArcHit hit;
        coord3d next;
        f32 floorY;
        f32 dt = launch->maxFields - t;

        if (dt > step) dt = step;
        leg.start = fl.legStart;
        leg.vel = fl.legVel;
        portArcPointAt(&leg, fl.legT + dt, &next);
        t += dt;

        if (world->segment(world->ctx, &fl.pos, &next, &hit))
        {
            if (launch->kind == PORT_ARC_GRENADE)
            {
                if (grenadeBounce(&fl, &next, dt, &hit, out)) continue;
                return;
            }
            closeLeg(out, &fl.legStart, &fl.legVel, fl.legT + chordFraction(&fl.pos, &next, &hit.pos) * dt);
            endAt(out, TRUE, &hit.pos, &hit.normal);
            out->endChr = hit.chr;
            return;
        }

        fl.pos = next;
        fl.legT += dt;
        if (!belowFloor(launch, world, &fl.pos, &floorY)) continue;
        if (dt > 1.0f)
        {
            fl = before;
            t = tBefore;
            step = 1.0f;
            continue;
        }
        land(&fl, floorY, out);
        return;
    }

    closeLeg(out, &fl.legStart, &fl.legVel, fl.legT);
    endAt(out, FALSE, &fl.pos, &up);
}

/* The dash being built while walking the path. */
typedef struct DashWalk {
    PortArcDash *out;
    s32 max;
    s32 count;
    s32 open;
} DashWalk;

static void dashExtend(DashWalk *w, const coord3d *p, f32 s)
{
    if (w->count >= w->max) return;
    if (!w->open)
    {
        w->out[w->count].a = *p;
        w->out[w->count].distA = s;
        w->open = TRUE;
    }
    w->out[w->count].b = *p;
}

static void dashClose(DashWalk *w)
{
    if (!w->open) return;
    w->open = FALSE;
    w->count++;
}

static void lerp3(const coord3d *a, const coord3d *b, f32 f, coord3d *out)
{
    out->x = a->x + (b->x - a->x) * f;
    out->y = a->y + (b->y - a->y) * f;
    out->z = a->z + (b->z - a->z) * f;
}

/* Lay the dash pattern over one short chord p0..p1 covering s0..s1. */
static void dashChord(DashWalk *w, const coord3d *p0, const coord3d *p1, f32 s0, f32 s1,
                      f32 dashLen, f32 period, f32 phase)
{
    f32 s = s0;

    while (s < s1 && w->count < w->max)
    {
        f32 u = fmodf(s - phase, period);
        s32 on;
        f32 boundary;
        f32 segEnd;
        coord3d p;

        if (u < 0.0f) u += period;
        /* Rounding can leave s a hair short of a boundary it just reached;
         * treat that as the boundary rather than start a sliver of a dash. */
        if (period - u < 1e-3f) u = 0.0f;
        on = u < dashLen - 1e-3f;
        boundary = s + (on ? dashLen - u : period - u);
        if (boundary <= s) boundary = s + 0.001f;
        segEnd = boundary < s1 ? boundary : s1;
        if (on && !w->open && segEnd - s < 1e-3f)
        {   /* the path ends a hair into a dash: nothing to draw */
            s = segEnd;
            continue;
        }
        if (on)
        {
            lerp3(p0, p1, (s - s0) / (s1 - s0), &p);
            dashExtend(w, &p, s);
            lerp3(p0, p1, (segEnd - s0) / (s1 - s0), &p);
            dashExtend(w, &p, s);
            if (segEnd == boundary) dashClose(w);
        }
        else
        {
            dashClose(w);
        }
        s = segEnd;
    }
}

/* Dashes of dashLen with gaps of gapLen along the path, the pattern moved
 * forward by phase. Each dash is straight between its ends and none spans a
 * bounce. Returns how many were written. */
s32 portArcDashes(const PortArcPath *path, f32 dashLen, f32 gapLen, f32 phase, PortArcDash *out, s32 max)
{
    DashWalk w = { out, max, 0, FALSE };
    f32 period = dashLen + gapLen;
    f32 s = 0.0f;
    s32 i;

    if (!(dashLen > 0.0f) || !(gapLen >= 0.0f)) return 0;

    /* A rocket's polyline: its positions a field apart are the chords. */
    for (i = 1; i < path->pointCount && w.count < max; i++)
    {
        f32 len = dist3(&path->points[i - 1], &path->points[i]);

        if (len > 0.0f) dashChord(&w, &path->points[i - 1], &path->points[i], s, s + len, dashLen, period, phase);
        s += len;
    }
    dashClose(&w);

    for (i = 0; i < path->legCount && w.count < max; i++)
    {
        const PortArcLeg *leg = &path->legs[i];
        f32 speed = sqrtf(dot3(&leg->vel, &leg->vel)) + PORT_ARC_GRAVITY * leg->duration;
        s32 n = (s32) ceilf(leg->duration * speed / 8.0f);
        coord3d p0;
        s32 k;

        if (n < 1) n = 1;
        if (n > 512) n = 512;
        portArcPointAt(leg, 0.0f, &p0);
        for (k = 1; k <= n && w.count < max; k++)
        {
            coord3d p1;
            f32 len;

            portArcPointAt(leg, leg->duration * (f32) k / (f32) n, &p1);
            len = dist3(&p0, &p1);
            if (len > 0.0f) dashChord(&w, &p0, &p1, s, s + len, dashLen, period, phase);
            s += len;
            p0 = p1;
        }
        dashClose(&w);
    }
    return w.count;
}

/* The arc starts at the muzzle, right under the eye, so it fades in over
 * its first stretch rather than starting as a blob at the screen's edge. */
f32 portArcFadeIn(f32 dist)
{
    const f32 start = 40.0f;
    const f32 full = 240.0f;
    f32 f;

    if (dist <= start) return 0.0f;
    if (dist >= full) return 1.0f;
    f = (dist - start) / (full - start);
    return f * f * (3.0f - 2.0f * f);
}

/* At least minPixels wide on screen, so a distant stretch stays visible. */
f32 portArcScreenWidth(f32 worldWidth, f32 depth, f32 tanHalfFovY, f32 viewHeight, f32 minPixels)
{
    f32 pixel;
    f32 minWidth;

    if (!(depth > 0.0f) || !(viewHeight > 0.0f)) return worldWidth;
    pixel = 2.0f * depth * tanHalfFovY / viewHeight;
    minWidth = minPixels * pixel;
    return minWidth > worldWidth ? minWidth : worldWidth;
}

/* The axis, tip first, to draw a struck knife along: its flight, turned
 * until it lies at least 50 degrees off the line of sight. A knife thrown
 * from the eye flies nearly along that line and would show end-on as a dot;
 * the real one spins, so how it sticks is not known anyway. Turned towards
 * its own sideways and downward drift, else straight down (the handle tips
 * up towards the thrower). */
void portArcKnifeAxis(const coord3d *dir, const coord3d *sight, coord3d *out)
{
    const f32 cosMin = 0.64278764f;   /* cos 50 degrees */
    const f32 sinMin = 0.76604444f;
    const f32 along = dir->x * sight->x + dir->y * sight->y + dir->z * sight->z;
    coord3d p = { dir->x - sight->x * along, dir->y - sight->y * along, dir->z - sight->z * along };
    f32 len = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
    f32 sign = (along < 0.0f) ? -1.0f : 1.0f;

    if (along * sign <= cosMin)
    {
        *out = *dir;
        return;
    }
    if (len < 0.001f)
    {   /* straight along the sight: tip it down, off the sight */
        p.x = sight->x * sight->y;
        p.y = -1.0f + sight->y * sight->y;
        p.z = sight->z * sight->y;
        len = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
        if (len < 0.001f)
        {
            p = (coord3d) { 1.0f, 0.0f, 0.0f };
            len = 1.0f;
        }
    }
    out->x = sight->x * cosMin * sign + p.x / len * sinMin;
    out->y = sight->y * cosMin * sign + p.y / len * sinMin;
    out->z = sight->z * cosMin * sign + p.z / len * sinMin;
}
