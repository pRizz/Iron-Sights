/* Debrief hit map drawing and input (FORK-048). The mission debrief shows the
 * front and back figures in its lower-right corner; selecting them or the hit
 * counts opens a closer look in the statistics' place. Each body part is an
 * ink-outlined oval shaded red by its hits, and dots mark the hits.
 * Coordinates are the dossier's native 440x330 page. Hits on the back half of
 * a box go on the back figure, the rest on the front one. */
#include <math.h>
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <joy.h>
#include <music.h>
#include <snd.h>
#include "bondaicommands.h"
#include "game/front.h"
#include "game/language.h"
#include "game/math_floor.h"
#include "game/textrelated.h"
#include "assets/obseg/text/LtitleE.h"
#include "port_hitmap.h"
#include "port_hud.h"

extern s32 viGetX(void);
extern s32 viGetY(void);

/* With the map up, the debrief stacks its hit counts under the shot and kill
 * totals, a little smaller, and the overview fills the lower-right corner as
 * close to the text as its shape allows: each part's oval keeps clear of the
 * statistics, weapon and counts rows beside it (front.c reports their right
 * edges). The figures sit right beside the counts column, as tall as fits
 * there, or else as tall as fits anywhere, as far left as they can. The
 * whole bottom area opens the closer look. */
#define OVERVIEW_QUARTER 164 /* under STATISTICS */
#define OVERVIEW_WEAPON  218 /* the weapon row's band */
#define OVERVIEW_COUNTS  237 /* the counts' band, down to the foot */
#define OVERVIEW_FOOT    325
#define OVERVIEW_RIGHT   382
#define OVERVIEW_GAP     8
#define OVERVIEW_CAPTION 9
/* The counts column: stock's labels and first row, values past the widest label. */
#define COUNTS_X     0x37
#define COUNTS_SPACE 7
#define COUNTS_TOP   0xF4
#define COUNTS_SCALE 0.8f
/* The closer look, below the stage name. */
#define ZOOM_TOP 143
#define ZOOM_FIG_Y 170.0f
#define ZOOM_FIG_H 140.0f
#define ZOOM_FRONT_X 70.0f
#define ZOOM_BACK_X 190.0f
#define ZOOM_FIG_W 110.0f
#define ZOOM_LEGEND_X 320
#define ZOOM_CLOSE_X 340

/* Ink on the dossier: parts outlined in ink and shaded red by their hits. */
#define PAPER_TEXT  0x000000ffu
#define INK         0x22303cffu
#define PAPER_RIM   0xd2caa6ffu
#define PAPER_BACK  0xcac29cffu /* the dossier paper's average, under the parts */
#define HEAT_RGB    0xc81e2300u /* crimson, up to 85% over the paper */
#define HEAT_MIN    0.06f
#define HEAT_SPAN   0.79f
#define DOT_HEAD    0xffc23cffu
#define DOT_KILL    0xd42a1effu
#define HOVER_BOX   50
#define HITMAP_PI   3.14159265f

typedef struct HitMapFrame {
    f32 x, y, figureW, figureH; /* the front figure's box; the back one follows */
} HitMapFrame;

static s32 hitmapEdges[2];                  /* this frame's text right edges by PORT_HITMAP_ROW_* */
static s32 hitmapCountsEdge;                /* and the counts column's */
static HitMapFrame hitmapOverviewFrame;     /* the last overview drawn, for hovering */
static s32 hitmapZoomed;

s32 portHitMapZoomed(void) { return hitmapZoomed; }
void portHitMapZoomSet(s32 zoomed) { hitmapZoomed = zoomed != 0; }

static s32 hitmapHasFigure(void)
{
    return portHitMapShown() && portHitMapFigure()->count > 0 && portHitMapCount() > 0;
}

s32 portHitMapDebriefDrawn(void) { return hitmapHasFigure(); }

static s32 hitmapCursorIn(f32 x, f32 y, f32 w, f32 h)
{
    return cursor_h_pos >= x && cursor_h_pos <= x + w && cursor_v_pos >= y && cursor_v_pos <= y + h;
}

static s32 hitmapTextWidth(const char *text, f32 scale)
{
    s32 h = 0, w = 0;

    portTextScale(scale);
    textMeasure(&h, &w, (char *)text, ptrFontZurichBoldChars, ptrFontZurichBold, 0);
    portTextScale(1.0f);
    return w;
}

void portHitMapDebriefText(s32 row, s32 x, const char *text)
{
    const s32 right = x + hitmapTextWidth(text, 1.0f);

    if (row < 0 || row > PORT_HITMAP_ROW_WEAPON) return;
    if (right > hitmapEdges[row]) hitmapEdges[row] = right;
}

/* A text band: rows from the page's left to their right edge, top to bottom. */
typedef struct HitMapBand {
    f32 top, bottom, right;
} HitMapBand;

/* How far right the group must start so the part keeps clear of the bands:
 * its oval's screen bounds, relative to the group's top-left at groupY. */
static f32 hitmapClearOf(const HitMapBand *bands, s32 count, f32 groupY, f32 left, f32 top, f32 bottom, f32 need)
{
    s32 i;

    for (i = 0; i < count; i++) {
        const f32 x = bands[i].right + OVERVIEW_GAP - left;

        if (groupY + bottom > bands[i].top && groupY + top < bands[i].bottom && x > need) need = x;
    }
    return need;
}

static f32 hitmapRoot(f32 v)
{
    return v > 0.0f ? sqrtf(v) : 0.0f;
}

/* The leftmost x for figures of height h, or how far they would overrun the tabs. */
static f32 hitmapSnugX(const HitMapBand *bands, s32 count, f32 h, f32 *figureW)
{
    const PortHitMapFigure *figure = portHitMapFigure();
    const f32 groupY = OVERVIEW_FOOT - OVERVIEW_CAPTION - h, captionW = hitmapTextWidth("HIT MAP", 0.5f);
    PortHitMapView views[2];
    f32 w, need = 0.0f;
    s32 side, i;

    w = portHitMapLayout(figure, FALSE, 0.0f, 0.0f, 1000.0f, h).width;
    views[0] = portHitMapLayout(figure, FALSE, 0.0f, 0.0f, w, h);
    views[1] = portHitMapLayout(figure, TRUE, w + OVERVIEW_GAP, 0.0f, w, h);
    for (side = 0; side < 2; side++) {
        const PortHitMapView *v = &views[side];
        const f32 sx = v->scaleX < 0.0f ? -v->scaleX : v->scaleX, sy = -v->scaleY;

        for (i = 0; i < figure->count; i++) {
            const PortHitMapBox *b = &figure->boxes[i];
            const f32 cx = v->originX + v->scaleX * b->cx, cy = v->originY + v->scaleY * b->cy;
            const f32 ex = sx * hitmapRoot(b->hx * b->ax * b->hx * b->ax + b->hy * b->ay * b->hy * b->ay) + 1.0f;
            const f32 ey = sy * hitmapRoot(b->hx * b->ay * b->hx * b->ay + b->hy * b->ax * b->hy * b->ax) + 1.0f;

            need = hitmapClearOf(bands, count, groupY, cx - ex, cy - ey, cy + ey, need);
        }
    }
    need = hitmapClearOf(bands, count, groupY, w + OVERVIEW_GAP * 0.5f - captionW * 0.5f, h, h + OVERVIEW_CAPTION, need);
    *figureW = w;
    return need;
}

/* The tallest figures whose group fits between the text and the tabs; with
 * snug, only those that also sit right beside the counts column. */
static HitMapFrame hitmapFit(const HitMapBand *bands, s32 snug)
{
    const f32 beside = hitmapCountsEdge + OVERVIEW_GAP + 2.0f;
    HitMapFrame frame = { 0.0f, 0.0f, 0.0f, 0.0f };
    f32 h, w, x;

    for (h = OVERVIEW_FOOT - OVERVIEW_CAPTION - OVERVIEW_QUARTER; h >= 20.0f; h -= 1.0f) {
        x = hitmapSnugX(bands, 3, h, &w);
        if ((!snug || x <= beside) && x + w * 2.0f + OVERVIEW_GAP <= OVERVIEW_RIGHT) {
            frame.x = x;
            frame.y = OVERVIEW_FOOT - OVERVIEW_CAPTION - h;
            frame.figureW = w;
            frame.figureH = h;
            break;
        }
    }
    return frame;
}

/* Beside the counts, as tall as fits there; the tallest anywhere otherwise. */
static HitMapFrame hitmapOverviewPlace(void)
{
    const HitMapBand bands[3] = {
        { OVERVIEW_QUARTER, OVERVIEW_WEAPON, (f32)hitmapEdges[PORT_HITMAP_ROW_STATS] },
        { OVERVIEW_WEAPON, OVERVIEW_COUNTS, (f32)hitmapEdges[PORT_HITMAP_ROW_WEAPON] },
        { OVERVIEW_COUNTS, OVERVIEW_FOOT, (f32)hitmapCountsEdge },
    };
    const HitMapFrame snug = hitmapFit(bands, TRUE);

    return snug.figureH > 0.0f ? snug : hitmapFit(bands, FALSE);
}

static f32 hitmapGroupRight(const HitMapFrame *f)
{
    return f->x + f->figureW * 2.0f + OVERVIEW_GAP;
}

/* The bottom area: the counts' strip across the page, and the map's column
 * where it rises above that strip. */
static s32 hitmapStripTop(void) { return COUNTS_TOP - 3; }

static s32 hitmapColumnTop(const HitMapFrame *f)
{
    const s32 top = (s32)f->y - 3;

    return top < hitmapStripTop() ? top : hitmapStripTop();
}

static s32 hitmapOverviewHover(void)
{
    const HitMapFrame *f = &hitmapOverviewFrame;
    const f32 left = f->x - 3;

    return f->figureH > 0.0f
        && (hitmapCursorIn(COUNTS_X - 3, hitmapStripTop(), OVERVIEW_RIGHT + 3 - (COUNTS_X - 3), OVERVIEW_FOOT + 3 - hitmapStripTop())
            || hitmapCursorIn(left, hitmapColumnTop(f), hitmapGroupRight(f) + 3 - left, hitmapStripTop() - hitmapColumnTop(f)));
}

static s32 hitmapCloseHover(void)
{
    return hitmapCursorIn(ZOOM_CLOSE_X - 4, ZOOM_TOP - 2, 50, 14);
}

static void hitmapSound(void)
{
    sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
}

/* The debrief's buttons while the map is up, or the cursor rests on it.
 * Returns TRUE when it used this frame's press; *nextHighlight drops the NEXT
 * tab's default highlight so a press off the tabs is the map's. */
s32 portHitMapDebriefInput(s32 onTab, s32 *nextHighlight)
{
    const u16 confirm = START_BUTTON | Z_TRIG | A_BUTTON;

    if (!hitmapHasFigure()) {
        hitmapZoomed = FALSE;
        return FALSE;
    }
    if (!hitmapZoomed) {
        if (onTab || !hitmapOverviewHover()) return FALSE;
        *nextHighlight = FALSE;
        if (!joyGetButtonsPressedThisFrame(PLAYER_1, confirm)) return FALSE;
        hitmapZoomed = TRUE;
        hitmapSound();
        return TRUE;
    }
    if (onTab) return FALSE;
    *nextHighlight = FALSE;
    if (!joyGetButtonsPressedThisFrame(PLAYER_1, confirm | B_BUTTON)) return FALSE;
    hitmapZoomed = FALSE;
    hitmapSound();
    return TRUE;
}

static Gfx *hitmapText(Gfx *g, const char *text, s32 x, s32 y, f32 scale, u32 color)
{
    char line[64];

    snprintf(line, sizeof(line), "%s\n", text);
    portTextScale(scale);
    g = microcode_constructor(g);
    g = textRender(g, &x, &y, line, ptrFontZurichBoldChars, ptrFontZurichBold, color, viGetX(), viGetY(), 0, 0);
    portTextScale(1.0f);
    return g;
}

/* A part's oval grown by grow, filled (outline 0) or stroked. */
static Gfx *hitmapPart(Gfx *g, const PortHitMapView *view, const PortHitMapBox *b, f32 grow, f32 outline, u32 color)
{
    const f32 scale = -view->scaleY;
    /* Screen y runs down, so the turn reverses; a mirrored view turns from the other side.
     * (Not atan2f: the game's is built on a 16-bit acosf table.) */
    const f32 angle = view->scaleX >= 0.0f ? -b->angle : HITMAP_PI + b->angle;

    return portHudEllipse(g, view->originX + view->scaleX * b->cx, view->originY + view->scaleY * b->cy,
                          b->hx * scale + grow, b->hy * scale + grow, angle, outline, color);
}

static Gfx *hitmapDot(Gfx *g, f32 x, f32 y, f32 r, u32 color)
{
    return portHudEllipse(g, x, y, r, r, 0.0f, 0.0f, color);
}

/* A hit's dot; gold head dots get a one-pixel ink ring, as gold on paper is faint. */
static Gfx *hitmapHitDot(Gfx *g, f32 x, f32 y, f32 r, u32 color)
{
    g = hitmapDot(g, x, y, r, color);
    return color == DOT_HEAD ? portHudEllipse(g, x, y, r, r, 0.0f, 0.01f, INK) : g;
}

/* A kill shot's red ring, edged with one raster pixel of paper (the overlay
 * never strokes thinner) so it stays apart from the crimson parts. */
static Gfx *hitmapKillRing(Gfx *g, f32 x, f32 y, f32 r)
{
    g = hitmapDot(g, x, y, r, DOT_KILL);
    return portHudEllipse(g, x, y, r, r, 0.0f, 0.01f, PAPER_RIM);
}

/* Hits per part on one side, and the busiest part on either side, so the
 * front and back figures share one scale. */
static s32 hitmapHeat(s32 back, s32 counts[PORT_HITMAP_MAX_BOXES])
{
    const PortHitMapHit *hits = portHitMapHits();
    const s32 n = portHitMapCount();
    s32 sides[2][PORT_HITMAP_MAX_BOXES] = { { 0 } };
    s32 i, most = 1;

    for (i = 0; i < n; i++) {
        s32 *c = &sides[(hits[i].flags & PORT_HITMAP_BACK) != 0][hits[i].box];

        if (++*c > most) most = *c;
    }
    for (i = 0; i < PORT_HITMAP_MAX_BOXES; i++) counts[i] = sides[back != 0][i];
    return most;
}

static u32 hitmapHeatColor(s32 count, s32 most)
{
    const f32 alpha = HEAT_MIN + HEAT_SPAN * count / most;

    return HEAT_RGB | (u32)(alpha * 255.0f + 0.5f);
}

/* One figure and the hits on its side: opaque paper under the parts (so the
 * CLASSIFIED stamp does not show through), parts shaded by hits, inked over,
 * then dots on a paper rim, kill shots last so their red ring stays on top.
 * Dots and strokes grow with the figure. */
static Gfx *hitmapFigure(Gfx *g, s32 back, f32 x, f32 y, f32 w, f32 h)
{
    const PortHitMapFigure *figure = portHitMapFigure();
    const PortHitMapHit *hits = portHitMapHits();
    const PortHitMapView view = portHitMapLayout(figure, back, x, y, w, h);
    const s32 count = portHitMapCount();
    const f32 dot = 0.6f + 0.009f * view.height, stroke = 0.3f + 0.003f * view.height;
    s32 heat[PORT_HITMAP_MAX_BOXES];
    const s32 most = hitmapHeat(back, heat);
    s32 i, pass;

    for (i = 0; i < figure->count; i++) g = hitmapPart(g, &view, &figure->boxes[i], stroke, 0.0f, PAPER_BACK);
    for (i = 0; i < figure->count; i++) {
        g = hitmapPart(g, &view, &figure->boxes[i], 0.0f, 0.0f, hitmapHeatColor(heat[i], most));
    }
    for (i = 0; i < figure->count; i++) g = hitmapPart(g, &view, &figure->boxes[i], 0.0f, stroke, INK);
    for (pass = 0; pass < 2; pass++) for (i = 0; i < count; i++) {
        const PortHitMapHit *hit = &hits[i];
        const s32 head = figure->boxes[hit->box].part == HIT_HEAD;
        const s32 kill = (hit->flags & PORT_HITMAP_KILL) != 0;
        f32 fx, fy, sx, sy;

        if (((hit->flags & PORT_HITMAP_BACK) != 0) != (back != 0) || kill != pass) continue;
        portHitMapHitPoint(figure, hit, &fx, &fy);
        sx = view.originX + view.scaleX * fx;
        sy = view.originY + view.scaleY * fy;
        g = kill ? hitmapKillRing(g, sx, sy, dot + stroke * 2.4f) : hitmapDot(g, sx, sy, dot + stroke, PAPER_RIM);
        g = hitmapHitDot(g, sx, sy, dot, head ? DOT_HEAD : INK);
    }
    return g;
}

static Gfx *hitmapOverview(Gfx *g)
{
    const char *caption = "HIT MAP";
    const HitMapFrame f = hitmapOverviewPlace();
    const f32 groupW = f.figureW * 2.0f + OVERVIEW_GAP;

    hitmapOverviewFrame = f;
    if (f.figureH <= 0.0f) return g;
    if (hitmapOverviewHover()) {
        const s32 strip = hitmapStripTop(), column = hitmapColumnTop(&f);

        g = microcode_constructor_related_to_menus(g, COUNTS_X - 3, strip, OVERVIEW_RIGHT + 3, OVERVIEW_FOOT + 3, HOVER_BOX);
        if (column < strip) {
            g = microcode_constructor_related_to_menus(g, (s32)f.x - 3, column, (s32)hitmapGroupRight(&f) + 3, strip, HOVER_BOX);
        }
    }
    g = hitmapFigure(g, FALSE, f.x, f.y, f.figureW, f.figureH);
    g = hitmapFigure(g, TRUE, f.x + f.figureW + OVERVIEW_GAP, f.y, f.figureW, f.figureH);
    return hitmapText(g, caption, (s32)(f.x + groupW * 0.5f) - hitmapTextWidth(caption, 0.5f) / 2,
                      (s32)(f.y + f.figureH) + 3, 0.5f, PAPER_TEXT);
}

static const char *hitmapTitle(s32 slot)
{
    return (const char *)langGet(getStringID(LTITLE, slot));
}

static Gfx *hitmapCount(Gfx *g, s32 row, s32 rowH, s32 x, s32 label, const char *value)
{
    const s32 y = COUNTS_TOP + row * rowH;
    const s32 right = x + hitmapTextWidth(value, COUNTS_SCALE);

    if (right > hitmapCountsEdge) hitmapCountsEdge = right;
    g = hitmapText(g, hitmapTitle(label), COUNTS_X, y, COUNTS_SCALE, PAPER_TEXT);
    return hitmapText(g, value, x, y, COUNTS_SCALE, PAPER_TEXT);
}

static s32 hitmapPercent(s32 count, s32 allHits)
{
    return (s32)floorFloat(((count * 100.0f) / allHits) + 0.5f);
}

Gfx *portHitMapDebriefCounts(Gfx *g, s32 shots, s32 kills, s32 head, s32 body, s32 limb, s32 other, s32 allHits)
{
    const s32 values[6] = { shots, kills, head, body, limb, other };
    const s32 labels[6] = { TITLE_STR_108_SHOTTOTAL, TITLE_STR_113_KILLTOTAL, TITLE_STR_109_HEADHITS,
                            TITLE_STR_110_BODYHITS, TITLE_STR_111_LIMBHITS, TITLE_STR_112_OTHER };
    s32 rowH = 0, w = 0, x = 0, i;
    char value[32];

    portTextScale(COUNTS_SCALE);
    textMeasure(&rowH, &w, (char *)hitmapTitle(TITLE_STR_104_STATS), ptrFontZurichBoldChars, ptrFontZurichBold, 0);
    portTextScale(1.0f);
    for (i = 0; i < 6; i++) {
        w = hitmapTextWidth(hitmapTitle(labels[i]), COUNTS_SCALE);
        if (w > x) x = w;
    }
    x += COUNTS_X + COUNTS_SPACE;
    for (i = 0; i < 6; i++) {
        if (i < 2) snprintf(value, sizeof(value), "%d", values[i]);
        else snprintf(value, sizeof(value), "%d (%d%%)", values[i], hitmapPercent(values[i], allHits));
        g = hitmapCount(g, i, rowH, x, labels[i], value);
    }
    return g;
}

static Gfx *hitmapLegendDot(Gfx *g, s32 y, u32 color, s32 kill, const char *label, s32 count)
{
    const f32 cx = ZOOM_LEGEND_X + 3.0f, cy = y + 3.5f;
    char line[32];

    g = kill ? hitmapKillRing(g, cx, cy, 2.9f) : hitmapDot(g, cx, cy, 2.3f, PAPER_RIM);
    g = hitmapHitDot(g, cx, cy, 1.9f, color);
    snprintf(line, sizeof(line), "%s: %d", label, count);
    return hitmapText(g, line, ZOOM_LEGEND_X + 9, y, 0.55f, PAPER_TEXT);
}

/* The dots by legend entry: ink and gold (head) dots, which add up to the
 * hits shown, and kill shots, which are also one of those. */
static void hitmapLegendCounts(s32 *ink, s32 *head, s32 *kill)
{
    const PortHitMapFigure *figure = portHitMapFigure();
    const PortHitMapHit *hits = portHitMapHits();
    const s32 count = portHitMapCount();
    s32 i;

    *ink = *head = *kill = 0;
    for (i = 0; i < count; i++) {
        if (figure->boxes[hits[i].box].part == HIT_HEAD) (*head)++;
        else (*ink)++;
        if ((hits[i].flags & PORT_HITMAP_KILL) != 0) (*kill)++;
    }
}

/* Fewest to most hits, shaded as the parts are. */
static Gfx *hitmapLegendHeat(Gfx *g, s32 y)
{
    s32 i;

    for (i = 0; i < 4; i++) {
        const f32 cx = ZOOM_LEGEND_X + 3.0f + i * 7.0f, cy = y + 3.5f;

        g = portHudEllipse(g, cx, cy, 3.0f, 3.0f, 0.0f, 0.0f, hitmapHeatColor(i, 3));
        g = portHudEllipse(g, cx, cy, 3.0f, 3.0f, 0.0f, 0.6f, INK);
    }
    return hitmapText(g, "Hits per part", ZOOM_LEGEND_X, y + 9, 0.5f, PAPER_TEXT);
}

/* The hits with a dot; then, only when there are any, the debrief's head,
 * body and limb hits without one. Fists and knives never count as debrief
 * hits (no WEAPONSTATBITFLAG_PLAYER_STAT_HIT), so those are bullet hits the
 * map could not record: past its cap, or a box it could not find. */
static Gfx *hitmapTally(Gfx *g, s32 bodyHits)
{
    const s32 shown = portHitMapCount();
    const s32 unshown = bodyHits > shown ? bodyHits - shown : 0;
    char line[48];

    snprintf(line, sizeof(line), "Hits shown: %d", shown);
    g = hitmapText(g, line, ZOOM_LEGEND_X, 250, 0.55f, PAPER_TEXT);
    if (unshown <= 0) return g;
    snprintf(line, sizeof(line), "Not recorded: %d", unshown);
    return hitmapText(g, line, ZOOM_LEGEND_X, 262, 0.55f, PAPER_TEXT);
}

static Gfx *hitmapZoom(Gfx *g, s32 bodyHits)
{
    s32 ink, head, kill;

    g = hitmapText(g, "HIT MAP:", 0x37, ZOOM_TOP, 1.0f, PAPER_TEXT);
    if (hitmapCloseHover()) {
        g = microcode_constructor_related_to_menus(g, ZOOM_CLOSE_X - 4, ZOOM_TOP - 2, ZOOM_CLOSE_X + 46, ZOOM_TOP + 12, HOVER_BOX);
    }
    g = hitmapText(g, "CLOSE", ZOOM_CLOSE_X, ZOOM_TOP, 0.75f, PAPER_TEXT);
    g = hitmapText(g, "FRONT", (s32)(ZOOM_FRONT_X + ZOOM_FIG_W * 0.5f) - hitmapTextWidth("FRONT", 0.6f) / 2,
                   (s32)ZOOM_FIG_Y - 12, 0.6f, PAPER_TEXT);
    g = hitmapText(g, "BACK", (s32)(ZOOM_BACK_X + ZOOM_FIG_W * 0.5f) - hitmapTextWidth("BACK", 0.6f) / 2,
                   (s32)ZOOM_FIG_Y - 12, 0.6f, PAPER_TEXT);
    g = hitmapFigure(g, FALSE, ZOOM_FRONT_X, ZOOM_FIG_Y, ZOOM_FIG_W, ZOOM_FIG_H);
    g = hitmapFigure(g, TRUE, ZOOM_BACK_X, ZOOM_FIG_Y, ZOOM_FIG_W, ZOOM_FIG_H);
    hitmapLegendCounts(&ink, &head, &kill);
    g = hitmapLegendDot(g, 180, INK, FALSE, "Hit", ink);
    g = hitmapLegendDot(g, 194, DOT_HEAD, FALSE, "Head", head);
    g = hitmapLegendDot(g, 208, INK, TRUE, "Kill shot", kill);
    g = hitmapLegendHeat(g, 224);
    return hitmapTally(g, bodyHits);
}

Gfx *portHitMapDebriefDraw(Gfx *gdl, s32 bodyHits)
{
    if (hitmapHasFigure()) gdl = hitmapZoomed ? hitmapZoom(gdl, bodyHits) : hitmapOverview(gdl);
    hitmapEdges[0] = hitmapEdges[1] = hitmapCountsEdge = 0;
    return gdl;
}
