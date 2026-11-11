/* Projectile Arc (FORK-069): a thrown hand grenade's whole path, pure and
 * ROM-free.
 *
 * generate_player_thrown_grenade (gun.c) throws the grenade at 16.67 units a
 * field along the aim, plus 5 upwards and the player's motion, with
 * restitution 0.3 (Projectile.unk8C), friction 0.133 (unk94) and
 * PROJECTILEFLAG_00000002. Its fuse is THROWN_ITEM_TIMER_DEFAULT less the
 * fields the trigger was held, and it explodes the tick its timer drops
 * below zero, wherever it is. objectTick (propobj.c) moves it a tick at a
 * time:
 *
 * - In the air it falls like any round. What it hits reflects its speed
 *   with the restitution; it counts a bounce for each tick it hits
 *   something or is found under the floor's clearance.
 * - On landing (under the clearance, or a hit that leaves it no lower while
 *   it was falling) it is placed 4 units over the floor and its vertical
 *   speed is reversed times the restitution. A first bounce always hops at
 *   least 2.22 units a field; after that a hop that slow settles it.
 * - Settled, it slides on at its horizontal speed while it turns to lie
 *   flat (objSettle's slerp), then loses launch->slideFriction units a
 *   field of speed (0.133 at a steady 60 Hz or any higher tick rate) until
 *   it stops, resting on the floor. It now moves as objects do
 *   (objTryMovePropWithCollision, world->slide): a wall, object or character
 *   stops it its width short.
 *
 * Each position a field apart is recorded, the path's points. In the air,
 * walls and characters are tested on a chord over launch->step fields (each
 * test is the costly part); a chord that hits is flown again a field at a
 * time, as the game moves the grenade, so the bounce happens where the
 * game's does. The slide asks world->slide each field. */
#include <math.h>
#include "port_projectile_arc.h"

extern void *memset(void *, int, size_t);
extern float fabsf(float);

typedef struct ArcThrown {
    coord3d pos;
    coord3d speed;
    s32 bounces;     /* Projectile.unk90 */
    s32 settled;     /* PROJECTILEFLAG_AIRBORNE cleared by objSettle */
    s32 turning;     /* fields of frictionless slide left while it turns flat */
    s32 stopped;     /* at rest, against a wall, stuck, or in a character */
    s32 grounded;    /* times it was found under the floor's clearance */
    s32 inChr;       /* a knife stopped in a character */
    coord3d normal;  /* of what it stuck to or struck */
} ArcThrown;

static f32 thrownDot(const coord3d *a, const coord3d *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

/* handles_projectile_motion backs a level hit off by 0.1 units towards the
 * start (or half the step when the step is shorter than that). */
static void thrownBackOff(const coord3d *from, const coord3d *to, const coord3d *hit, coord3d *out)
{
    const coord3d d = { to->x - from->x, to->y - from->y, to->z - from->z };
    const f32 len = sqrtf(thrownDot(&d, &d));
    const f32 factor = (len > 0.1f) ? 0.1f / len : 0.5f;

    out->x = hit->x - factor * d.x;
    out->y = hit->y - factor * d.y;
    out->z = hit->z - factor * d.z;
}

static void settle(ArcThrown *g, const PortArcLaunch *launch)
{
    g->settled = TRUE;
    g->turning = launch->turnFields;
}

/* It ends its flight on what it hit: a mine sticks there, a knife strikes a
 * character. */
static void thrownStop(ArcThrown *g, const PortArcHit *hit)
{
    g->pos = hit->pos;
    g->normal = hit->normal;
    g->stopped = TRUE;
    g->inChr = hit->chr;
}

/* Whether a mine reaching the level's floor sticks to it. The game catches
 * a falling object over the floor once a tick ends with it inside its
 * clearance (its spinning bounding box's bottom), and it hops; a mine sticks
 * only when one tick's drop carries it from above that band to below the
 * floor. Which happens hangs on where the ticks fall and on the random spin,
 * so a prediction stepping through it flipped between the two as the aim
 * moved a hair. Decide by the likelier instead: it sticks once a tick's drop
 * is over twice the band it is caught in (PORT_ARC_MINE_CATCH), which takes
 * a fall from a ledge; an ordinary throw hops. Above 60 Hz a tick moves it
 * less, so it is caught more often still. */
static s32 mineSticksToFloor(const ArcThrown *g, const PortArcLaunch *launch)
{
    const f32 step = (launch->tickStep > 0.0f) ? launch->tickStep : 1.0f;

    return -g->speed.y * step > 2.0f * PORT_ARC_MINE_CATCH;
}

/* A level hit on the walkable floor, rather than a wall or ceiling. */
static s32 hitIsFloor(const PortArcWorld *world, const PortArcHit *hit)
{
    f32 floorY;

    return !hit->prop && hit->normal.y > 0.0f && world->floorBelow(world->ctx, &hit->pos, &floorY)
        && fabsf(floorY - hit->pos.y) < 2.0f;
}

/* One airborne tick. hits: test what lies along this field's move. */
static void airField(ArcThrown *g, const PortArcLaunch *launch, const PortArcWorld *world, s32 hits)
{
    const f32 vy = g->speed.y;
    const f32 vyNext = vy - PORT_ARC_GRAVITY;
    const f32 startY = g->pos.y;
    coord3d next = { g->pos.x + g->speed.x, g->pos.y + 0.5f * (vy + vyNext), g->pos.z + g->speed.z };
    PortArcHit hit;
    s32 struck = FALSE;
    s32 bounced = FALSE;
    s32 ground;
    f32 floorY = 0.0f;
    f32 hitY = 0.0f;

    g->speed.y = vyNext;
    if (hits && world->segment(world->ctx, &g->pos, &next, &hit)
        && !(launch->sticks && hitIsFloor(world, &hit) && !mineSticksToFloor(g, launch)))
    {   /* (a mine the floor would catch hops below, as if nothing were hit) */
        const f32 prevVy = g->speed.y;
        const f32 d = -(launch->bounce + 1.0f) * thrownDot(&g->speed, &hit.normal);
        coord3d moved;

        if ((launch->sticks && !hit.chr) || (launch->strikes && hit.chr && g->bounces == 0))
        {
            thrownStop(g, &hit);
            return;
        }
        if (launch->sticks)
        {   /* a mine on a character: no bounce; back where the tick began */
            g->pos.y = next.y;
            return;
        }

        g->speed.x += d * hit.normal.x;
        g->speed.y += d * hit.normal.y;
        g->speed.z += d * hit.normal.z;
        if (hit.prop)
        {   /* an object or character: back where the tick began, at its new height */
            moved.x = g->pos.x;
            moved.y = next.y;
            moved.z = g->pos.z;
        }
        else
        {
            thrownBackOff(&g->pos, &next, &hit.pos, &moved);
        }
        bounced = prevVy <= 0.0f && (g->speed.y >= 0.0f || startY <= moved.y);
        struck = TRUE;
        hitY = hit.pos.y;
        next = moved;
    }

    ground = world->floorBelow(world->ctx, &next, &floorY) && next.y < floorY + launch->floorClearance;
    if (ground) g->grounded++;
    if (ground && !struck && launch->sticks && mineSticksToFloor(g, launch))
    {   /* fast enough to have crossed the floor: stuck where it meets it */
        const PortArcHit floorHit = { { next.x, floorY, next.z }, { 0.0f, 1.0f, 0.0f }, FALSE, FALSE };

        thrownStop(g, &floorHit);
        return;
    }
    if (ground || struck) g->bounces++;
    if (ground || bounced)
    {
        next.y = (ground ? floorY : hitY) + launch->floorClearance + 4.0f;
        g->speed.y *= -launch->bounce;
        if (g->speed.y < PORT_ARC_HAND_HOP)
        {
            if (g->bounces == 1) g->speed.y = PORT_ARC_HAND_HOP;
            else settle(g, launch);
        }
    }
    g->pos = next;
}

/* One settled tick: slide, then friction once it lies flat. Something in
 * the way stops it where the move ends; the game keeps pushing it there. */
static void slideField(ArcThrown *g, const PortArcLaunch *launch, const PortArcWorld *world)
{
    coord3d next = { g->pos.x + g->speed.x, g->pos.y, g->pos.z + g->speed.z };
    s32 blocked;
    f32 floorY;

    if (g->stopped) return;
    if (g->turning > 0)
    {
        g->turning--;
    }
    else if (!(launch->slideFriction > 0.0f))
    {
        g->speed.x *= PORT_ARC_SLIDE_FACTOR;
        g->speed.z *= PORT_ARC_SLIDE_FACTOR;
    }
    else
    {
        const f32 speed = sqrtf(g->speed.x * g->speed.x + g->speed.z * g->speed.z);
        const f32 f = (speed > 0.0f) ? launch->slideFriction / speed : 1.0f;

        if (f >= 1.0f)
        {
            g->speed.x = g->speed.z = 0.0f;
        }
        else
        {
            g->speed.x -= g->speed.x * f;
            g->speed.z -= g->speed.z * f;
        }
    }
    blocked = world->slide != NULL && !world->slide(world->ctx, &g->pos, &next, PORT_ARC_HAND_WIDTH);
    if (world->floorBelow(world->ctx, &next, &floorY)) next.y = floorY + launch->restHeight;
    g->pos = next;
    if (blocked || (fabsf(g->speed.x) < 0.1f && fabsf(g->speed.z) < 0.1f && g->turning <= 0)) g->stopped = TRUE;
}

static void thrownField(ArcThrown *g, const PortArcLaunch *launch, const PortArcWorld *world, s32 hits)
{
    if (g->settled) slideField(g, launch, world);
    else airField(g, launch, world, hits);
}

static s32 thrownPoint(PortArcPath *out, const coord3d *p)
{
    if (out->pointCount >= PORT_ARC_MAX_POINTS) return FALSE;
    out->points[out->pointCount++] = *p;
    return TRUE;
}

/* Whether the chord over the fields just flown meets anything it would
 * bounce off. */
static s32 thrownChordHits(const PortArcWorld *world, const coord3d *from, const coord3d *to)
{
    PortArcHit hit;

    return world->segment(world->ctx, from, to, &hit);
}

/* launch->maxFields is a hand grenade's fuse: it explodes that many fields
 * after it is thrown, wherever it lies. A knife or mine ends where it
 * stops, or flies on past the prediction (no impact). */
void portArcSimulateThrown(const PortArcLaunch *launch, const PortArcWorld *world, PortArcPath *out)
{
    static const coord3d up = { 0.0f, 1.0f, 0.0f };
    const s32 fuse = (s32) launch->maxFields;
    const s32 chord = (launch->step >= 1.0f) ? (s32) launch->step : 1;
    ArcThrown g;
    s32 field = 0;

    memset(out, 0, sizeof(*out));
    memset(&g, 0, sizeof(g));
    g.pos = launch->pos;
    g.speed = launch->vel;
    thrownPoint(out, &g.pos);

    while (field < fuse && !g.stopped)
    {
        const ArcThrown before = g;
        const s32 first = out->pointCount;
        const s32 n = (fuse - field < chord) ? fuse - field : chord;
        s32 done = 0;

        /* Fly up to a chord's worth of fields, stopping where it settles, so
         * a chord never spans the change from flight to slide. */
        while (done < n && !g.stopped)
        {
            const s32 wasSettled = g.settled;

            thrownField(&g, launch, world, FALSE);
            done++;
            if (!thrownPoint(out, &g.pos)) break;
            if (g.settled != wasSettled) break;
        }
        /* A field that found it under the floor's clearance is flown again
         * with its hit test too: it may have crossed the floor itself, which
         * a chord between placed positions misses (a mine sticks there). */
        if (!before.settled && (g.grounded != before.grounded
                                || thrownChordHits(world, &out->points[first - 1], &out->points[out->pointCount - 1])))
        {
            s32 k;

            g = before;
            out->pointCount = first;
            for (k = 0; k < done && !g.stopped; k++)
            {
                thrownField(&g, launch, world, TRUE);
                if (!thrownPoint(out, &g.pos)) break;
            }
        }
        field += done;
        if (out->pointCount >= PORT_ARC_MAX_POINTS) break;
    }

    out->impact = launch->kind == PORT_ARC_THROWN || g.stopped;
    out->end = g.pos;
    out->endNormal = (g.normal.x != 0.0f || g.normal.y != 0.0f || g.normal.z != 0.0f) ? g.normal : up;
    if (g.inChr && launch->strikes)
    {   /* its speed is the field's, left as it was when it struck */
        const f32 len = sqrtf(thrownDot(&g.speed, &g.speed));

        out->endChr = len > 0.0f;
        if (out->endChr)
        {
            out->endDir.x = g.speed.x / len;
            out->endDir.y = g.speed.y / len;
            out->endDir.z = g.speed.z / len;
        }
    }
}
