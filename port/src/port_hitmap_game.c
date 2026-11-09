/* Debrief hit map recorder (FORK-048). chrHandleBulletHit (chr.c) reports
 * each of the player's bullets that struck a body-part hit box; this finds
 * where the ray entered the box, in the box's own frame, so the point keeps
 * its place on the body whatever pose the character was in. The first body
 * hit in a mission supplies the reference figure the debrief draws. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "bondaicommands.h"
#include "game/model.h"
#include "port_hitmap.h"

extern s32 getPlayerCount(void);
/* chraction.c: a position's bearing from where the character faces, 0 = ahead. */
extern f32 get_distance_actor_to_position(ChrRecord *self, coord3d *pos);

static s32 hitmapEnabled = 1;
static PortHitMapHit hitmapHits[PORT_HITMAP_MAX_HITS];
static s32 hitmapCount;
static s32 hitmapLimit = PORT_HITMAP_MAX_HITS;
static PortHitMapFigure hitmapFigure;
/* The latest recorded hit. handles_shot_actors applies its damage straight
 * after, and portHitMapDamage then records that damage and marks the hit if
 * it was lethal. */
static ChrRecord *hitmapPendingChr;
static s32 hitmapPendingIndex;
static s32 hitmapLog = -1; /* GE_HITMAP_LOG=1: each figure box and hit on stderr */

s32 portHitMapOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "debrief_hit_map") != 0) return FALSE;
    *value = hitmapEnabled;
    return TRUE;
}

s32 portHitMapOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "debrief_hit_map") != 0) return FALSE;
    hitmapEnabled = value != 0;
    return TRUE;
}

s32 portHitMapShown(void)
{
    return hitmapEnabled;
}

void portHitMapReset(void)
{
    hitmapCount = 0;
    hitmapFigure.count = 0;
    hitmapPendingChr = NULL;
}

s32 portHitMapCount(void) { return hitmapCount; }

void portHitMapLimitSet(s32 limit)
{
    hitmapLimit = (limit > 0 && limit < PORT_HITMAP_MAX_HITS) ? limit : PORT_HITMAP_MAX_HITS;
}
const PortHitMapHit *portHitMapHits(void) { return hitmapHits; }
const PortHitMapFigure *portHitMapFigure(void) { return &hitmapFigure; }

static s32 hitmapOpcode(const ModelNode *node)
{
    return node->Opcode & 0xff;
}

static s32 hitmapBodyPart(s32 part)
{
    return part >= HIT_LEFT_FOOT && part <= HIT_CHEST;
}

/* Depth-first, as the model's own walkers visit it. */
static ModelNode *hitmapNextNode(ModelNode *node)
{
    if (node->Child != NULL) return node->Child;
    while (node != NULL) {
        if (node->Next != NULL) return node->Next;
        node = node->Parent;
    }
    return NULL;
}

/* Unanimated joint position: the joint origins summed up to the model's root,
 * then on through whatever the model is attached to. */
static void hitmapRestOrigin(Model *model, ModelNode *node, coord3d *out)
{
    out->x = out->y = out->z = 0.0f;
    while (model != NULL) {
        for (; node != NULL; node = node->Parent) {
            if (hitmapOpcode(node) == MODELNODE_OPCODE_GROUP) {
                out->x += node->Data->Group.Origin.x;
                out->y += node->Data->Group.Origin.y;
                out->z += node->Data->Group.Origin.z;
            }
        }
        node = model->attachedto_objinst;
        model = model->attachedto;
    }
}

static s32 hitmapOrdinal(Model *model, ModelNode *target, s32 part)
{
    ModelNode *node;
    s32 ordinal = 0;

    for (node = model->obj->RootNode; node != NULL && node != target; node = hitmapNextNode(node)) {
        if (hitmapOpcode(node) == MODELNODE_OPCODE_BBOX && node->Data->BoundingBox.ModelNumber == (u32)part) ordinal++;
    }
    return ordinal;
}

static void hitmapCapture(Model *model, s32 onlyPart)
{
    ModelNode *node;
    s32 ordinals[HIT_CHEST + 1];

    memset(ordinals, 0, sizeof(ordinals));
    if (model == NULL || model->obj == NULL) return;
    for (node = model->obj->RootNode; node != NULL; node = hitmapNextNode(node)) {
        PortHitMapBox *box;
        const ModelRoData_BoundingBoxRecord *record;
        coord3d origin;
        s32 part;

        if (hitmapOpcode(node) != MODELNODE_OPCODE_BBOX) continue;
        record = &node->Data->BoundingBox;
        part = (s32)record->ModelNumber;
        if (!hitmapBodyPart(part)) continue;
        if ((onlyPart && part != onlyPart) || hitmapFigure.count >= PORT_HITMAP_MAX_BOXES) {
            ordinals[part]++;
            continue;
        }
        hitmapRestOrigin(model, node, &origin);
        box = &hitmapFigure.boxes[hitmapFigure.count++];
        box->part = part;
        box->ordinal = ordinals[part]++;
        box->jointX = origin.x;
        box->jointY = origin.y;
        box->x0 = record->Bounds.xmin;
        box->x1 = record->Bounds.xmax;
        box->y0 = record->Bounds.ymin;
        box->y1 = record->Bounds.ymax;
        if (hitmapLog > 0) {
            fprintf(stderr, "ge: hitmap box part=%d ord=%d joint=%.1f,%.1f,%.1f x=%.1f..%.1f y=%.1f..%.1f z=%.1f..%.1f\n",
                    part, box->ordinal, origin.x, origin.y, origin.z, box->x0, box->x1, box->y0, box->y1,
                    record->Bounds.zmin, record->Bounds.zmax);
        }
    }
    portHitMapPose(&hitmapFigure);
}

/* The ray missed its own box (rounding at an edge): use the point where it
 * passes closest to the box centre, held inside the box. */
static void hitmapNearestEntry(const struct bbox *box, const coord3d *o, const coord3d *d, coord3d *entry)
{
    const f32 len = d->x * d->x + d->y * d->y + d->z * d->z;
    f32 t = 0.0f;
    s32 a;

    if (len > 0.0f) {
        t = ((box->f[0][0] + box->f[0][1]) * 0.5f - o->x) * d->x
          + ((box->f[1][0] + box->f[1][1]) * 0.5f - o->y) * d->y
          + ((box->f[2][0] + box->f[2][1]) * 0.5f - o->z) * d->z;
        t /= len;
    }
    for (a = 0; a < 3; a++) {
        f32 v = o->f[a] + d->f[a] * t;

        if (v < box->f[a][0]) v = box->f[a][0];
        if (v > box->f[a][1]) v = box->f[a][1];
        entry->f[a] = v;
    }
}

void portHitMapBullet(struct ShotData *shot, struct BulletHit *bhit)
{
    ChrRecord *chr;
    const ModelRoData_BoundingBoxRecord *record;
    Mtxf *mtx;
    coord3d origin, dir, entry;
    s32 part, box;

    if (hitmapLog < 0) hitmapLog = getenv("GE_HITMAP_LOG") != NULL;
    hitmapPendingChr = NULL;
    chr = bhit->prop != NULL ? bhit->prop->chr : NULL;
    if (getPlayerCount() != 1 || chr == NULL || bhit->model == NULL || bhit->node == NULL) return;
    if (hitmapOpcode(bhit->node) != MODELNODE_OPCODE_BBOX) return;
    record = &bhit->node->Data->BoundingBox;
    part = (s32)record->ModelNumber;
    if (!hitmapBodyPart(part) || hitmapCount >= hitmapLimit) return;
    if (hitmapFigure.count == 0) hitmapCapture(chr->model, 0);
    box = portHitMapFigureFind(&hitmapFigure, part, hitmapOrdinal(bhit->model, bhit->node, part));
    if (box < 0) {
        /* A part kept in an attached model, such as a separate head. */
        hitmapCapture(bhit->model, part);
        box = portHitMapFigureFind(&hitmapFigure, part, 0);
    }
    mtx = modelFindNodeMtx(bhit->model, bhit->node, 0);
    if (box < 0 || mtx == NULL || !portHitMapRayToLocal(mtx, &shot->viewOrigin, &shot->viewDir, &origin, &dir)) return;
    if (!portHitMapBoxEntry(&record->Bounds, &origin, &dir, &entry)) hitmapNearestEntry(&record->Bounds, &origin, &dir, &entry);
    hitmapHits[hitmapCount] = portHitMapPlace(box, &record->Bounds, &entry, &dir);
    if (hitmapLog > 0) {
        /* The game's own front test (melee damage): a point back along the
         * shot, from the character's facing; near 0 or 360 degrees is ahead. */
        coord3d toward;

        toward.x = bhit->prop->pos.x - shot->dir.x;
        toward.y = bhit->prop->pos.y - shot->dir.y;
        toward.z = bhit->prop->pos.z - shot->dir.z;
        fprintf(stderr, "ge: hitmap hit=%d part=%d box=%d local=%.1f,%.1f,%.1f dir=%.2f,%.2f,%.2f back=%d bearing=%.0f act=%d\n",
                hitmapCount, part, box, entry.x, entry.y, entry.z, dir.x, dir.y, dir.z,
                (hitmapHits[hitmapCount].flags & PORT_HITMAP_BACK) != 0,
                get_distance_actor_to_position(chr, &toward) * 57.29578f, chr->actiontype);
    }
    hitmapPendingChr = chr;
    hitmapPendingIndex = hitmapCount++;
}

void portHitMapDamage(ChrRecord *chr, f32 before)
{
    PortHitMapHit *hit;

    if (chr != hitmapPendingChr) return;
    hitmapPendingChr = NULL;
    hit = &hitmapHits[hitmapPendingIndex];
    portHitMapDamageUnits(before, chr->damage, chr->maxdamage, &hit->damage, &hit->overkill);
    if (hitmapLog > 0) {
        fprintf(stderr, "ge: hitmap damage=%d units=%u overkill=%u\n", hitmapPendingIndex, hit->damage, hit->overkill);
    }
    if (before < chr->maxdamage && chr->damage >= chr->maxdamage) {
        hit->flags |= PORT_HITMAP_KILL;
        if (hitmapLog > 0) fprintf(stderr, "ge: hitmap kill=%d\n", hitmapPendingIndex);
    }
}
