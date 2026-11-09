#ifndef PORT_HITMAP_H
#define PORT_HITMAP_H
/* Debrief hit map (FORK-048): where each of the player's bullets struck a
 * character in the current solo mission, drawn on front and back body figures
 * on the mission debrief. port_hitmap.c is the pure placement and layout core,
 * port_hitmap_game.c records hits and port_hitmap_draw.c draws the debrief. */
#include <ultra64.h>
#include <bondtypes.h>

#define PORT_HITMAP_MAX_HITS  1024
#define PORT_HITMAP_MAX_BOXES 24

/* PortHitMapHit.flags */
#define PORT_HITMAP_BACK 1 /* entered through the box's back half */
#define PORT_HITMAP_KILL 2 /* the hit that took the character past its health */

/* One hit, relative to its body part's hit box: u runs along the box's local
 * x from its low side, v along its local y, both 0..65535. A box keeps that
 * frame in every pose, so the point lands on the same part of the figure. */
typedef struct PortHitMapHit {
    u8 box;   /* index into the figure's boxes */
    u8 flags;
    u16 u;
    u16 v;
    /* Damage in thousandths of the victim's full health (portHitMapDamageUnits):
     * all of it, and the part a kill shot spent past the health left. 0 when
     * the hit did no damage, as on a character already dying. */
    u16 damage;
    u16 overkill;
} PortHitMapHit;

/* The Parts view's rows: left and right sides counted together. */
enum {
    PORT_HITMAP_GROUP_HEAD,
    PORT_HITMAP_GROUP_CHEST,
    PORT_HITMAP_GROUP_PELVIS,
    PORT_HITMAP_GROUP_UPPER_ARM,
    PORT_HITMAP_GROUP_FOREARM,
    PORT_HITMAP_GROUP_HAND,
    PORT_HITMAP_GROUP_THIGH,
    PORT_HITMAP_GROUP_SHIN,
    PORT_HITMAP_GROUP_FOOT,
    PORT_HITMAP_GROUPS
};

#define PORT_HITMAP_HEALTH 1000 /* PortHitMapHit damage units in one full health */

typedef struct PortHitMapGroupStats {
    s32 hits, kills;
    u32 damage; /* PortHitMapHit damage units */
} PortHitMapGroupStats;

typedef struct PortHitMapStats {
    PortHitMapGroupStats groups[PORT_HITMAP_GROUPS];
    s32 hits, kills;
    u32 damage, overkill;
} PortHitMapStats;

/* A body part's hit box, drawn as the ellipse inscribed in it. GE's unanimated
 * skeleton spreads every limb out sideways along its bone's local x, so
 * portHitMapPose swings the leg chains down about the hips and the arms into
 * an A-pose about the shoulders. */
typedef struct PortHitMapBox {
    s32 part;    /* HIT_* body part */
    s32 ordinal; /* nth box of this part in node order */
    f32 jointX, jointY; /* unanimated joint position, model units, y up */
    f32 x0, x1, y0, y1; /* bounds in the joint's frame */
    /* Posed (portHitMapPose): centre, turn and direction of local +x, half extents. */
    f32 cx, cy, angle, ax, ay, hx, hy;
} PortHitMapBox;

typedef struct PortHitMapFigure {
    s32 count;
    PortHitMapBox boxes[PORT_HITMAP_MAX_BOXES];
} PortHitMapFigure;

/* Screen placement of one figure: screen = origin + scale * posed point. */
typedef struct PortHitMapView {
    f32 originX, originY;
    f32 scaleX, scaleY; /* scaleX < 0 mirrors (back view); scaleY < 0 (y up) */
    f32 width, height;  /* the fitted figure, centred in the rectangle */
} PortHitMapView;

/* Core (pure). */
s32 portHitMapRayToLocal(const Mtxf *mtx, const coord3d *origin, const coord3d *dir,
                         coord3d *localOrigin, coord3d *localDir);
s32 portHitMapBoxEntry(const struct bbox *box, const coord3d *origin, const coord3d *dir, coord3d *entry);
PortHitMapHit portHitMapPlace(s32 boxIndex, const struct bbox *box, const coord3d *entry,
                              const coord3d *localDir);
s32 portHitMapFigureFind(const PortHitMapFigure *figure, s32 part, s32 ordinal);
/* A HIT_* body part's Parts view row, or -1. */
s32 portHitMapGroup(s32 part);
/* A hit's damage from the victim's damage before and after it and its full
 * health, in PortHitMapHit units (each clamped to u16). */
void portHitMapDamageUnits(f32 before, f32 after, f32 maxdamage, u16 *damage, u16 *overkill);
/* Hits, kill shots and damage by Parts view row, and their totals. */
void portHitMapStatsCompute(const PortHitMapFigure *figure, const PortHitMapHit *hits, s32 count,
                            PortHitMapStats *stats);
void portHitMapPose(PortHitMapFigure *figure);
/* Fit a posed figure in a screen rectangle; back views mirror it. */
PortHitMapView portHitMapLayout(const PortHitMapFigure *figure, s32 back,
                                f32 x, f32 y, f32 w, f32 h);
/* A hit on its part's ellipse, in posed figure units. */
void portHitMapHitPoint(const PortHitMapFigure *figure, const PortHitMapHit *hit, f32 *x, f32 *y);

/* Recording (port_hitmap_game.c). */
s32 portHitMapOptionGet(const char *key, s32 *value);
s32 portHitMapOptionSet(const char *key, s32 value);
void portHitMapReset(void);
struct ShotData;
struct BulletHit;
void portHitMapBullet(struct ShotData *shot, struct BulletHit *bhit);
void portHitMapDamage(struct ChrRecord *chr, f32 before);
s32 portHitMapCount(void);
/* Test harness only (GE_HITMAP_PROBE cap=<n>): record at most limit hits. */
void portHitMapLimitSet(s32 limit);
const PortHitMapHit *portHitMapHits(void);
const PortHitMapFigure *portHitMapFigure(void);

/* Debrief (front.c constructor_menu0D_missioncomplete and its interface). */
s32 portHitMapShown(void);
/* TRUE while the debrief draws the map; it then prints the counts below. */
s32 portHitMapDebriefDrawn(void);
s32 portHitMapZoomed(void);
/* Closing the closer look from outside (the debrief's init) also returns it to FIGURES. */
void portHitMapZoomSet(s32 zoomed);
/* The closer look's view: FIGURES (0) or PARTS (1). */
s32 portHitMapPartsShown(void);
void portHitMapPartsSet(s32 parts);
/* onTab: the cursor is on NEXT or PREVIOUS. TRUE when the press was the map's. */
s32 portHitMapDebriefInput(s32 onTab, s32 *nextHighlight);
/* Rows the debrief reports, so the overview keeps clear of them. */
#define PORT_HITMAP_ROW_STATS  0 /* a statistics value above the weapon */
#define PORT_HITMAP_ROW_WEAPON 1 /* the weapon of choice */
/* A row printed at x. */
void portHitMapDebriefText(s32 row, s32 x, const char *text);
/* The shot and kill totals with the head, body, limb and other hits stacked
 * under them, leaving the lower-right corner to the map. */
Gfx *portHitMapDebriefCounts(Gfx *gdl, s32 shots, s32 kills, s32 head, s32 body, s32 limb, s32 other, s32 allHits);
Gfx *portHitMapDebriefDraw(Gfx *gdl, s32 bodyHits);
#endif
