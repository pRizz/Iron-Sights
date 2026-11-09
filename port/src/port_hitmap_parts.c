/* Debrief hit map Parts view (FORK-048): the closer look's PARTS tab. A table
 * gives each body part's hits, kill shots and shares of the hits and of the
 * damage, left and right sides together; a column beside it sums the mission
 * up. The counts are the map's own recorded hits, so they add up to Hits
 * shown; gun and hat hits, which the map never places, come from the
 * debrief's counters. Coordinates are the dossier's native 440x330 page. */
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "game/math_floor.h"
#include "game/textrelated.h"
#include "port_hitmap.h"
#include "port_hitmap_draw.h"

extern s32 get_curplayer_shot_register(SHOT_REGISTER shot_register);

#define PARTS_SCALE   0.55f
#define PARTS_NOTE    0.5f
#define PARTS_TOP     170
#define PARTS_ROW     12
#define PARTS_LABEL_X 0x37
#define PARTS_HITS_R  120 /* right edges of the hit and kill counts */
#define PARTS_KILLS_R 144
#define PARTS_SHARE_X 152 /* share of hits; share of damage follows */
#define PARTS_SHARE_STEP 66
#define PARTS_BAR_W   40
#define PARTS_BAR_H   5
#define PARTS_RULE_R  284 /* the table's header rule ends here */
#define SUMMARY_RULE_X 292
#define SUMMARY_X     298
#define SUMMARY_R     378 /* clear of the NEXT and PREVIOUS tabs */
#define TRACK_RGBA    0x22303c30u /* a bar's empty track, a light ink wash */

static const char *const partsNames[PORT_HITMAP_GROUPS] = {
    "Head", "Chest", "Pelvis", "Upper arms", "Forearms", "Hands", "Thighs", "Shins", "Feet",
};

static s32 partsRound(f32 v) { return (s32)floorFloat(v + 0.5f); }

static Gfx *partsRight(Gfx *g, const char *text, s32 right, s32 y, f32 scale)
{
    return portHitMapInk(g, text, right - portHitMapInkWidth(text, scale), y, scale, PAPER_TEXT);
}

static Gfx *partsRect(Gfx *g, s32 x0, s32 y0, s32 x1, s32 y1, u32 color)
{
    return microcode_constructor_related_to_menus(g, x0, y0, x1, y1, color);
}

/* A share as a bar on its track, full at the widest share on the page, then
 * its percentage; a dash when there is nothing to share. */
static Gfx *partsShare(Gfx *g, s32 x, s32 y, u32 part, u32 whole, f32 widest, u32 color)
{
    char text[16];
    const s32 top = y + 1;

    g = partsRect(g, x, top, x + PARTS_BAR_W, top + PARTS_BAR_H, TRACK_RGBA);
    if (whole == 0) return portHitMapInk(g, "-", x + PARTS_BAR_W + 3, y, PARTS_SCALE, PAPER_TEXT);
    if (part > 0 && widest > 0.0f) {
        s32 w = partsRound(PARTS_BAR_W * ((f32)part / whole) / widest);

        g = partsRect(g, x, top, x + (w < 1 ? 1 : w), top + PARTS_BAR_H, color);
    }
    snprintf(text, sizeof(text), "%d%%", partsRound(100.0f * part / whole));
    return portHitMapInk(g, text, x + PARTS_BAR_W + 3, y, PARTS_SCALE, PAPER_TEXT);
}

/* The largest share of hits or of damage, so both columns share one scale. */
static f32 partsWidest(const PortHitMapStats *s)
{
    f32 widest = 0.0f;
    s32 i;

    for (i = 0; i < PORT_HITMAP_GROUPS; i++) {
        const f32 hits = s->hits > 0 ? (f32)s->groups[i].hits / s->hits : 0.0f;
        const f32 damage = s->damage > 0 ? (f32)s->groups[i].damage / s->damage : 0.0f;

        if (hits > widest) widest = hits;
        if (damage > widest) widest = damage;
    }
    return widest;
}

static Gfx *partsTable(Gfx *g, const PortHitMapStats *s)
{
    const s32 damageX = PARTS_SHARE_X + PARTS_SHARE_STEP;
    const f32 widest = partsWidest(s);
    char text[16];
    s32 i, y = PARTS_TOP;

    g = portHitMapInk(g, "Part", PARTS_LABEL_X, y, PARTS_SCALE, PAPER_TEXT);
    g = partsRight(g, "Hits", PARTS_HITS_R, y, PARTS_SCALE);
    g = partsRight(g, "Kills", PARTS_KILLS_R, y, PARTS_SCALE);
    g = portHitMapInk(g, "Share of hits", PARTS_SHARE_X, y, PARTS_SCALE, PAPER_TEXT);
    g = portHitMapInk(g, "Share of damage", damageX, y, PARTS_SCALE, PAPER_TEXT);
    g = partsRect(g, PARTS_LABEL_X, y + 9, PARTS_RULE_R, y + 10, INK);
    for (i = 0; i < PORT_HITMAP_GROUPS; i++) {
        const PortHitMapGroupStats *p = &s->groups[i];

        y += PARTS_ROW;
        g = portHitMapInk(g, partsNames[i], PARTS_LABEL_X, y, PARTS_SCALE, PAPER_TEXT);
        snprintf(text, sizeof(text), "%d", p->hits);
        g = partsRight(g, text, PARTS_HITS_R, y, PARTS_SCALE);
        snprintf(text, sizeof(text), "%d", p->kills);
        g = partsRight(g, p->kills > 0 ? text : "-", PARTS_KILLS_R, y, PARTS_SCALE);
        g = partsShare(g, PARTS_SHARE_X, y, (u32)p->hits, (u32)s->hits, widest, INK);
        g = partsShare(g, damageX, y, p->damage, s->damage, widest, HEAT_RGB | 0xffu);
    }
    return g;
}

static Gfx *partsLine(Gfx *g, s32 *y, const char *label, const char *value)
{
    g = portHitMapInk(g, label, SUMMARY_X, *y, PARTS_SCALE, PAPER_TEXT);
    g = partsRight(g, value, SUMMARY_R, *y, PARTS_SCALE);
    *y += PARTS_ROW;
    return g;
}

/* The mission in sum. Damage reads in hundredths of a full health, so 100 is
 * one character's whole health bar; overkill is the share of it kill shots
 * spent past the health left. Hits the map could not record still count in
 * the debrief, so Not recorded appears as on the FIGURES view. */
static Gfx *partsSummary(Gfx *g, const PortHitMapStats *s, s32 bodyHits)
{
    const s32 unshown = bodyHits > s->hits ? bodyHits - s->hits : 0;
    char text[24];
    s32 y = PARTS_TOP;

    g = partsRect(g, SUMMARY_RULE_X, PARTS_TOP - 2, SUMMARY_RULE_X + 1, PARTS_TOP + PARTS_ROW * PORT_HITMAP_GROUPS + 9, INK);
    snprintf(text, sizeof(text), "%d", s->hits);
    g = partsLine(g, &y, "Hits shown", text);
    if (unshown > 0) {
        snprintf(text, sizeof(text), "%d", unshown);
        g = partsLine(g, &y, "Not recorded", text);
    }
    snprintf(text, sizeof(text), "%d", s->kills);
    g = partsLine(g, &y, "Kill shots", text);
    if (s->kills > 0) snprintf(text, sizeof(text), "%.1f", (f32)s->hits / s->kills);
    g = partsLine(g, &y, "Hits per kill", s->kills > 0 ? text : "-");
    snprintf(text, sizeof(text), "%d", partsRound(s->damage / (PORT_HITMAP_HEALTH / 100.0f)));
    g = partsLine(g, &y, "Damage dealt", text);
    if (s->damage > 0) snprintf(text, sizeof(text), "%d%%", partsRound(100.0f * s->overkill / s->damage));
    g = partsLine(g, &y, "Overkill", s->damage > 0 ? text : "-");
    y += PARTS_ROW / 2;
    snprintf(text, sizeof(text), "%d", get_curplayer_shot_register(SHOT_REGISTER_GUN));
    g = partsLine(g, &y, "Gun hits", text);
    snprintf(text, sizeof(text), "%d", get_curplayer_shot_register(SHOT_REGISTER_HAT));
    g = partsLine(g, &y, "Hat hits", text);
    y += 3;
    g = portHitMapInk(g, "100 damage is one", SUMMARY_X, y, PARTS_NOTE, PAPER_TEXT);
    g = portHitMapInk(g, "full health bar.", SUMMARY_X, y + 8, PARTS_NOTE, PAPER_TEXT);
    g = portHitMapInk(g, "Gun and hat hits", SUMMARY_X, y + 19, PARTS_NOTE, PAPER_TEXT);
    return portHitMapInk(g, "do no damage.", SUMMARY_X, y + 27, PARTS_NOTE, PAPER_TEXT);
}

Gfx *portHitMapPartsDraw(Gfx *g, s32 bodyHits)
{
    PortHitMapStats stats;

    portHitMapStatsCompute(portHitMapFigure(), portHitMapHits(), portHitMapCount(), &stats);
    g = partsTable(g, &stats);
    return partsSummary(g, &stats, bodyHits);
}
