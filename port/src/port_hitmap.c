/* Debrief hit map core (FORK-048): pure hit placement and figure layout.
 * Nothing here reads game state, so scripts/port/test_hitmap.c runs it
 * without a ROM. */
#include <math.h>
#include "bondaicommands.h"
#include "port_hitmap.h"

#define HITMAP_LEFT_FIRST  HIT_LEFT_FOOT
#define HITMAP_LEFT_LAST   HIT_LEFT_THIGH
#define HITMAP_LEFT_ARM0   HIT_LEFT_HAND
#define HITMAP_LEFT_ARM1   HIT_LEFT_SHOULDER
#define HITMAP_UNIT        65535.0f
#define HITMAP_FAR         1e30f
#define HITMAP_LEG_DROP    1.5707963f /* radians below horizontal */
#define HITMAP_ARM_DROP    1.1344640f /* 65 degrees */

/* Characters face +z in model space, so a front hit enters the high-z half. */
#define HITMAP_FORWARD_Z   1.0f

/* A node matrix's rows are its axes in view space, scaled by the model's
 * scale; dividing each projection by the row's squared length undoes both, as
 * modelTestRayIntersectsTransformedBBox does. */
s32 portHitMapRayToLocal(const Mtxf *mtx, const coord3d *origin, const coord3d *dir,
                         coord3d *localOrigin, coord3d *localDir)
{
    coord3d rel;
    s32 a;

    rel.x = origin->x - mtx->m[3][0];
    rel.y = origin->y - mtx->m[3][1];
    rel.z = origin->z - mtx->m[3][2];
    for (a = 0; a < 3; a++) {
        const f32 sq = mtx->m[a][0] * mtx->m[a][0] + mtx->m[a][1] * mtx->m[a][1] + mtx->m[a][2] * mtx->m[a][2];

        if (!(sq > 0.0f)) return FALSE;
        localOrigin->f[a] = (mtx->m[a][0] * rel.x + mtx->m[a][1] * rel.y + mtx->m[a][2] * rel.z) / sq;
        localDir->f[a] = (mtx->m[a][0] * dir->x + mtx->m[a][1] * dir->y + mtx->m[a][2] * dir->z) / sq;
    }
    return TRUE;
}

/* Slab test: where the ray's line first crosses into the box. A ray starting
 * inside still reports the entry behind its start, the surface it came through. */
s32 portHitMapBoxEntry(const struct bbox *box, const coord3d *origin, const coord3d *dir, coord3d *entry)
{
    f32 near = -HITMAP_FAR, far = HITMAP_FAR;
    s32 a;

    for (a = 0; a < 3; a++) {
        const f32 lo = box->f[a][0], hi = box->f[a][1];
        f32 t0, t1;

        if (dir->f[a] < 1e-9f && dir->f[a] > -1e-9f) {
            if (origin->f[a] < lo || origin->f[a] > hi) return FALSE;
            continue;
        }
        t0 = (lo - origin->f[a]) / dir->f[a];
        t1 = (hi - origin->f[a]) / dir->f[a];
        if (t0 > t1) { const f32 t = t0; t0 = t1; t1 = t; }
        if (t0 > near) near = t0;
        if (t1 < far) far = t1;
        if (near > far) return FALSE;
    }
    if (near <= -HITMAP_FAR) return FALSE;
    entry->x = origin->x + dir->x * near;
    entry->y = origin->y + dir->y * near;
    entry->z = origin->z + dir->z * near;
    return TRUE;
}

static u16 hitmapUnit(f32 value, f32 lo, f32 hi)
{
    f32 t = hi > lo ? (value - lo) / (hi - lo) : 0.5f;

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (u16)(t * HITMAP_UNIT + 0.5f);
}

PortHitMapHit portHitMapPlace(s32 boxIndex, const struct bbox *box, const coord3d *entry,
                              const coord3d *localDir)
{
    PortHitMapHit hit;
    const f32 middle = (box->zmin + box->zmax) * 0.5f;
    const f32 depth = (entry->z - middle) * HITMAP_FORWARD_Z;

    hit.box = (u8)boxIndex;
    hit.damage = hit.overkill = 0;
    hit.u = hitmapUnit(entry->x, box->xmin, box->xmax);
    hit.v = hitmapUnit(entry->y, box->ymin, box->ymax);
    /* A side entry halfway along takes the half the bullet travelled toward. */
    hit.flags = (depth < 0.0f || (depth == 0.0f && localDir->z * HITMAP_FORWARD_Z > 0.0f)) ? PORT_HITMAP_BACK : 0;
    return hit;
}

s32 portHitMapFigureFind(const PortHitMapFigure *figure, s32 part, s32 ordinal)
{
    s32 i, first = -1;

    for (i = 0; i < figure->count; i++) {
        if (figure->boxes[i].part != part) continue;
        if (figure->boxes[i].ordinal == ordinal) return i;
        if (first < 0) first = i;
    }
    return first;
}

s32 portHitMapGroup(s32 part)
{
    switch (part) {
    case HIT_HEAD: return PORT_HITMAP_GROUP_HEAD;
    case HIT_CHEST: return PORT_HITMAP_GROUP_CHEST;
    case HIT_PELVIS: return PORT_HITMAP_GROUP_PELVIS;
    case HIT_LEFT_SHOULDER: case HIT_RIGHT_SHOULDER: return PORT_HITMAP_GROUP_UPPER_ARM;
    case HIT_LEFT_ARM: case HIT_RIGHT_ARM: return PORT_HITMAP_GROUP_FOREARM;
    case HIT_LEFT_HAND: case HIT_RIGHT_HAND: return PORT_HITMAP_GROUP_HAND;
    case HIT_LEFT_THIGH: case HIT_RIGHT_THIGH: return PORT_HITMAP_GROUP_THIGH;
    case HIT_LEFT_LEG: case HIT_RIGHT_LEG: return PORT_HITMAP_GROUP_SHIN;
    case HIT_LEFT_FOOT: case HIT_RIGHT_FOOT: return PORT_HITMAP_GROUP_FOOT;
    default: return -1;
    }
}

static u16 hitmapHealthUnits(f32 amount, f32 maxdamage)
{
    const f32 units = amount * PORT_HITMAP_HEALTH / maxdamage + 0.5f;

    if (!(units > 0.0f)) return 0;
    return units >= 65535.0f ? 65535 : (u16)units;
}

/* Health is how far damage has to climb to maxdamage, so the damage dealt is
 * the climb and a kill shot's overkill is how far it went past the top. */
void portHitMapDamageUnits(f32 before, f32 after, f32 maxdamage, u16 *damage, u16 *overkill)
{
    *damage = *overkill = 0;
    if (!(maxdamage > 0.0f) || !(after > before)) return;
    *damage = hitmapHealthUnits(after - before, maxdamage);
    if (after > maxdamage) *overkill = hitmapHealthUnits(after - (before > maxdamage ? before : maxdamage), maxdamage);
}

void portHitMapStatsCompute(const PortHitMapFigure *figure, const PortHitMapHit *hits, s32 count,
                            PortHitMapStats *stats)
{
    static const PortHitMapStats empty;
    s32 i;

    *stats = empty;
    for (i = 0; i < count; i++) {
        const s32 group = hits[i].box < figure->count ? portHitMapGroup(figure->boxes[hits[i].box].part) : -1;
        const s32 kill = (hits[i].flags & PORT_HITMAP_KILL) != 0;
        PortHitMapGroupStats *g;

        if (group < 0) continue;
        g = &stats->groups[group];
        g->hits++;
        g->kills += kill;
        g->damage += hits[i].damage;
        stats->hits++;
        stats->kills += kill;
        stats->damage += hits[i].damage;
        stats->overkill += hits[i].overkill;
    }
}

static s32 hitmapLeftPart(s32 part)
{
    return (part >= HITMAP_LEFT_FIRST && part <= HITMAP_LEFT_LAST)
        || (part >= HITMAP_LEFT_ARM0 && part <= HITMAP_LEFT_ARM1);
}

static s32 hitmapRightPart(s32 part)
{
    return (part >= HIT_RIGHT_FOOT && part <= HIT_RIGHT_THIGH)
        || (part >= HIT_RIGHT_HAND && part <= HIT_RIGHT_SHOULDER);
}

/* The part a limb swings about: the thigh or shoulder that starts its chain. */
static s32 hitmapChainRoot(s32 part)
{
    if (part >= HIT_LEFT_FOOT && part <= HIT_LEFT_THIGH) return HIT_LEFT_THIGH;
    if (part >= HIT_RIGHT_FOOT && part <= HIT_RIGHT_THIGH) return HIT_RIGHT_THIGH;
    if (part >= HIT_LEFT_HAND && part <= HIT_LEFT_SHOULDER) return HIT_LEFT_SHOULDER;
    if (part >= HIT_RIGHT_HAND && part <= HIT_RIGHT_SHOULDER) return HIT_RIGHT_SHOULDER;
    return 0;
}

/* Legs hang straight down; arms leave the body this far below horizontal. */
static f32 hitmapChainDrop(s32 root)
{
    return (root == HIT_LEFT_THIGH || root == HIT_RIGHT_THIGH) ? HITMAP_LEG_DROP : HITMAP_ARM_DROP;
}

void portHitMapPose(PortHitMapFigure *figure)
{
    s32 i;

    for (i = 0; i < figure->count; i++) {
        PortHitMapBox *b = &figure->boxes[i];
        const s32 root = hitmapChainRoot(b->part);
        const s32 pivotBox = root ? portHitMapFigureFind(figure, root, 0) : -1;
        const f32 restX = b->jointX + (b->x0 + b->x1) * 0.5f;
        const f32 restY = b->jointY + (b->y0 + b->y1) * 0.5f;
        f32 px = b->jointX, py = b->jointY, angle = 0.0f, c, sn;

        if (root) {
            /* A bone runs along local x, out to the side it sits on. */
            const f32 out = (b->x0 + b->x1) >= 0.0f ? 1.0f : -1.0f;

            angle = -out * hitmapChainDrop(root);
            if (pivotBox >= 0) {
                px = figure->boxes[pivotBox].jointX;
                py = figure->boxes[pivotBox].jointY;
            }
        }
        c = cosf(angle);
        sn = sinf(angle);
        b->cx = px + (restX - px) * c - (restY - py) * sn;
        b->cy = py + (restX - px) * sn + (restY - py) * c;
        b->angle = angle;
        b->ax = c;
        b->ay = sn;
        b->hx = (b->x1 - b->x0) * 0.5f;
        b->hy = (b->y1 - b->y0) * 0.5f;
    }
}

/* Facing the viewer, the character's left side is on the screen's right. */
static f32 hitmapLeftSign(const PortHitMapFigure *figure)
{
    f32 balance = 0.0f;
    s32 i;

    for (i = 0; i < figure->count; i++) {
        const PortHitMapBox *b = &figure->boxes[i];

        if (hitmapLeftPart(b->part)) balance += b->cx;
        if (hitmapRightPart(b->part)) balance -= b->cx;
    }
    return balance < 0.0f ? -1.0f : 1.0f;
}

static void hitmapLocalPoint(const PortHitMapBox *b, f32 lx, f32 ly, f32 *x, f32 *y)
{
    *x = b->cx + lx * b->ax - ly * b->ay;
    *y = b->cy + lx * b->ay + ly * b->ax;
}

PortHitMapView portHitMapLayout(const PortHitMapFigure *figure, s32 back,
                                f32 x, f32 y, f32 w, f32 h)
{
    PortHitMapView view = { x + w * 0.5f, y + h * 0.5f, 0.0f, 0.0f, 0.0f, 0.0f };
    f32 minX = HITMAP_FAR, maxX = -HITMAP_FAR, minY = HITMAP_FAR, maxY = -HITMAP_FAR;
    f32 scale, sign;
    s32 i;

    /* Each part is drawn as the ellipse inscribed in its box. */
    for (i = 0; i < figure->count; i++) {
        const PortHitMapBox *b = &figure->boxes[i];
        const f32 ex = sqrtf(b->hx * b->ax * b->hx * b->ax + b->hy * b->ay * b->hy * b->ay);
        const f32 ey = sqrtf(b->hx * b->ay * b->hx * b->ay + b->hy * b->ax * b->hy * b->ax);

        if (b->cx - ex < minX) minX = b->cx - ex;
        if (b->cx + ex > maxX) maxX = b->cx + ex;
        if (b->cy - ey < minY) minY = b->cy - ey;
        if (b->cy + ey > maxY) maxY = b->cy + ey;
    }
    if (figure->count <= 0 || !(maxX > minX) || !(maxY > minY) || w <= 0.0f || h <= 0.0f) return view;
    scale = w / (maxX - minX);
    if (h / (maxY - minY) < scale) scale = h / (maxY - minY);
    sign = hitmapLeftSign(figure) * (back ? -1.0f : 1.0f);
    view.scaleX = scale * sign;
    view.scaleY = -scale;
    view.width = scale * (maxX - minX);
    view.height = scale * (maxY - minY);
    view.originX = x + w * 0.5f - view.scaleX * (minX + maxX) * 0.5f;
    view.originY = y + h * 0.5f + scale * (minY + maxY) * 0.5f;
    return view;
}

void portHitMapHitPoint(const PortHitMapFigure *figure, const PortHitMapHit *hit, f32 *x, f32 *y)
{
    const PortHitMapBox *b = &figure->boxes[hit->box];
    const f32 sx = hit->u / HITMAP_UNIT * 2.0f - 1.0f, sy = hit->v / HITMAP_UNIT * 2.0f - 1.0f;

    /* The elliptical grid mapping takes the box's square onto its inscribed
     * ellipse: centres stay put, sides land on the rim, corners just inside. */
    hitmapLocalPoint(b, sx * sqrtf(1.0f - sy * sy * 0.5f) * b->hx,
                     sy * sqrtf(1.0f - sx * sx * 0.5f) * b->hy, x, y);
}
