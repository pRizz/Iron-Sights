/* Projectile Arc (FORK-069): the rocket launcher's flight, pure and ROM-free.
 *
 * gunFireTankShell launches a rocket at three fields of thrust along the aim
 * plus the player's motion, with the thrust itself (Projectile.unk10) as an
 * acceleration and PROJECTILEFLAG_00000020 set. objectTick's airborne branch
 * (propobj.c) then flies it, one tick of dt fields at a time:
 *
 * - Boost (flag 0x20). An ideal height (unkB0) climbs with an ideal vertical
 *   speed (unkB4) under the thrust alone, while a lift (unk1C) ramps from 0
 *   to gravity at 1/90 per field. Until the lift is full the rocket falls
 *   under gravity less the lift, so it dips. Then, while below the ideal
 *   height, it closes 7% of the gap a field. Once it reaches the ideal it is
 *   placed on it, powered, with the ideal vertical speed.
 * - Powered: it accelerates along its thrust with no gravity.
 * - Thrust stops once its speed^2 passes ROCKET_SPEED_BREAK_THRESHOLD (about
 *   167 units a field), and after GRENADE_SMOKE_FRAMES fields it loses lift
 *   and power and falls like a shell.
 * - It explodes on the first wall, floor, object or character, or once it is
 *   within its clearance of the floor.
 *
 * None of that is a parabola, so this steps the game's integrator one field
 * at a time (the game's tick at 60 Hz) and records each position. */
#include <math.h>
#include "port_projectile_arc.h"

extern void *memset(void *, int, size_t);

typedef struct ArcRocket {
    coord3d pos;
    coord3d speed;
    coord3d thrust;
    f32 lift;      /* unk1C */
    f32 idealY;    /* unkB0 */
    f32 idealVy;   /* unkB4 */
    f32 age;       /* unkA8 */
    s32 boosting;  /* PROJECTILEFLAG_00000020 */
    s32 powered;   /* PROJECTILEFLAG_POWERED */
} ArcRocket;

static void rocketVertical(ArcRocket *r, coord3d *next, f32 dt)
{
    if (r->boosting)
    {
        if (r->lift < PORT_ARC_GRAVITY)
        {
            r->idealVy += r->thrust.y * dt;
            r->idealY += r->idealVy * dt;
            r->lift += (1.0f / 90.0f) * dt;
            if (r->lift > PORT_ARC_GRAVITY) r->lift = PORT_ARC_GRAVITY;
        }
        else if (next->y < r->idealY)
        {
            r->idealVy += r->thrust.y * dt;
            r->idealY += r->idealVy * dt;
            next->y += (0.07f * (r->idealY - next->y)) * dt;
        }
        else
        {
            next->y = r->idealY;
            r->boosting = FALSE;
            r->lift = 0.0f;
            r->powered = TRUE;
            r->speed.y = r->idealVy;
        }
    }

    if (!r->powered)
    {
        f32 vy = r->speed.y + (r->thrust.y + r->lift) * dt;
        f32 nextVy = vy - PORT_ARC_GRAVITY * dt;

        next->y += (dt * (vy + nextVy)) * 0.5f;
        r->speed.y = nextVy;
    }
    else
    {
        r->speed.y += (r->thrust.y + r->lift) * dt;
        next->y += r->speed.y * dt;
    }
}

/* After a clear tick: thrust stops past the speed cap, power after a while. */
static void rocketAge(ArcRocket *r)
{
    const f32 speed2 = r->speed.x * r->speed.x + r->speed.y * r->speed.y + r->speed.z * r->speed.z;

    if (speed2 > PORT_ARC_ROCKET_SPEED_CAP2)
    {
        r->thrust.x = r->thrust.y = r->thrust.z = 0.0f;
    }
    if (r->age >= PORT_ARC_ROCKET_POWER_FIELDS)
    {
        r->lift = 0.0f;
        r->powered = FALSE;
        r->boosting = FALSE;
    }
}

static s32 rocketPoint(PortArcPath *out, const coord3d *p)
{
    if (out->pointCount >= PORT_ARC_MAX_POINTS) return FALSE;
    out->points[out->pointCount++] = *p;
    return TRUE;
}

static void rocketEnd(PortArcPath *out, s32 impact, const coord3d *pos, const coord3d *normal)
{
    out->impact = impact;
    out->end = *pos;
    out->endNormal = *normal;
}

/* How far along from..to the hit lies, 0..1. */
static f32 rocketFraction(const coord3d *from, const coord3d *to, const coord3d *hit)
{
    const f32 cx = to->x - from->x, cy = to->y - from->y, cz = to->z - from->z;
    const f32 len2 = cx * cx + cy * cy + cz * cz;
    f32 f;

    if (!(len2 > 0.0f)) return 0.0f;
    f = ((hit->x - from->x) * cx + (hit->y - from->y) * cy + (hit->z - from->z) * cz) / len2;
    return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

/* The level, objects and characters are tested on a chord over every
 * launch->step fields (the path is nearly straight; each test is the costly
 * part), the floor every field. On a hit the positions past it are dropped. */
static s32 rocketChordHit(const PortArcWorld *world, PortArcPath *out, s32 from, PortArcHit *hit)
{
    const coord3d a = out->points[from];
    const coord3d b = out->points[out->pointCount - 1];
    f32 f;
    s32 keep = from + 1;
    s32 j;

    if (!world->segment(world->ctx, &a, &b, hit)) return FALSE;
    f = rocketFraction(&a, &b, &hit->pos);
    for (j = from + 1; j < out->pointCount - 1; j++)
    {
        if (rocketFraction(&a, &b, &out->points[j]) >= f) break;
        keep = j + 1;
    }
    out->pointCount = keep;
    rocketPoint(out, &hit->pos);
    return TRUE;
}

void portArcSimulateRocket(const PortArcLaunch *launch, const PortArcWorld *world, PortArcPath *out)
{
    static const coord3d up = { 0.0f, 1.0f, 0.0f };
    const f32 dt = 1.0f;
    const s32 chord = (launch->step >= 1.0f) ? (s32) launch->step : 1;
    ArcRocket r;
    s32 from = 0;
    s32 fields = 0;
    f32 t;

    memset(out, 0, sizeof(*out));
    memset(&r, 0, sizeof(r));
    r.pos = launch->pos;
    r.speed = launch->vel;
    r.thrust = launch->accel;
    r.idealY = launch->pos.y;
    r.idealVy = launch->vel.y;
    r.boosting = TRUE;
    rocketPoint(out, &r.pos);

    for (t = 0.0f; t < launch->maxFields; t += dt)
    {
        PortArcHit hit;
        coord3d next = r.pos;
        f32 floorY = 0.0f;
        s32 below;
        s32 full;

        r.age += dt;
        rocketVertical(&r, &next, dt);
        r.speed.x += r.thrust.x * dt;
        r.speed.z += r.thrust.z * dt;
        next.x += r.speed.x * dt;
        next.z += r.speed.z * dt;

        full = !rocketPoint(out, &next);
        below = world->floorBelow(world->ctx, &next, &floorY) && next.y < floorY + launch->floorClearance;
        r.pos = next;
        if (++fields >= chord || below || full || t + dt >= launch->maxFields)
        {
            if (rocketChordHit(world, out, from, &hit))
            {
                rocketEnd(out, TRUE, &hit.pos, &hit.normal);
                out->endChr = hit.chr;
                return;
            }
            if (below)
            {
                coord3d ground = { next.x, floorY, next.z };

                rocketEnd(out, TRUE, &ground, &up);
                return;
            }
            if (full) break;
            from = out->pointCount - 1;
            fields = 0;
        }
        rocketAge(&r);
    }
    rocketEnd(out, FALSE, &r.pos, &up);
}
