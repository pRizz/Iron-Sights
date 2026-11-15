/* Debrief hit map gun filter (FORK-048): the closer look's GUN drop-down,
 * under its title row. It lists All guns and each gun the player fired or hit
 * with, most hits first, each with its hits in brackets and its accuracy, and
 * filters the figures and the PARTS table to the gun picked. Beside it, the
 * shown guns' shots fired and accuracy. The list is drawn like the Options
 * sheet's drop-down: opaque paper with a dark edge, the value in use on a
 * light band and the entry under the cursor in white on a dark one. Every
 * input points at it with the menu cursor and confirms; B closes it.
 * Coordinates are the dossier's native 440x330 page. */
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "game/textrelated.h"
#include "port_hitmap.h"
#include "port_hitmap_draw.h"

extern u16 *get_ptr_long_watch_text_for_item(ITEM_IDS item);

#define GUNS_LABEL_X  0x37
#define GUNS_Y        159   /* the row's text */
#define GUNS_SCALE    0.6f
#define GUNS_GAP      6
#define GUNS_ARROW_W  12    /* the box's down arrow */
#define GUNS_BOX_MIN  70
#define GUNS_BOX_MAX  170
#define GUNS_LINE     14    /* a list line, and the box's height */
#define GUNS_TEXT_Y   3     /* text below a line's top: room for brackets and descenders */
#define GUNS_PAD      6     /* text in from a line's sides */
#define GUNS_ACC_W    34    /* the list's accuracy column */
#define GUNS_ROWS     10    /* list lines that fit above the page's foot */
#define GUNS_STATS_R  378   /* clear of the NEXT and PREVIOUS tabs */
#define GUNS_MAX      ITEM_IDS_MAX
#define COL_EDGE      0x000000c0u
#define COL_PANEL     0xede4c4ffu /* opaque paper, so the figures do not show through */
#define COL_LIVE      0x00000032u
#define COL_HOT       0x000000a0u
#define COL_HOT_TEXT  0xffffffffu

static s32 gunsShown = PORT_HITMAP_ALL_GUNS;
static s32 gunsOpen;
static s32 gunsPage;

typedef struct GunsList {
    s32 count;            /* entries: All guns, then the guns */
    s32 guns[GUNS_MAX + 1];
} GunsList;

static GunsList gunsList(void)
{
    GunsList list;

    list.guns[0] = PORT_HITMAP_ALL_GUNS;
    list.count = 1 + portHitMapGunList(portHitMapGuns(), &list.guns[1], GUNS_MAX);
    return list;
}

/* Entries a page shows: all of them, or all but the last line, which turns the page. */
static s32 gunsPerPage(const GunsList *list) { return list->count <= GUNS_ROWS ? list->count : GUNS_ROWS - 1; }
static s32 gunsPages(const GunsList *list) { return (list->count + gunsPerPage(list) - 1) / gunsPerPage(list); }

static s32 gunsLines(const GunsList *list)
{
    const s32 per = gunsPerPage(list), left = list->count - gunsPage * per;

    if (list->count <= GUNS_ROWS) return list->count;
    return (left < per ? left : per) + 1;
}

static void gunsName(s32 gun, char *out, s32 size)
{
    const char *name = gun == PORT_HITMAP_ALL_GUNS ? "All guns" : (const char *)get_ptr_long_watch_text_for_item(gun);
    s32 n;

    snprintf(out, size, "%s", name != NULL ? name : "?");
    n = (s32)strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == ' ')) out[--n] = '\0';
}

/* A gun's entry: its name and its hits on characters in brackets. */
static void gunsLabel(s32 gun, char *out, s32 size)
{
    const PortHitMapGunCounts c = portHitMapGunTotals(portHitMapGuns(), gun);
    char name[48];

    gunsName(gun, name, sizeof(name));
    snprintf(out, size, "%s (%d)", name, portHitMapGunHits(&c));
}

static void gunsAccuracy(s32 gun, char *out, s32 size)
{
    const PortHitMapGunCounts c = portHitMapGunTotals(portHitMapGuns(), gun);
    const s32 accuracy = portHitMapGunAccuracy(&c);

    if (accuracy < 0) snprintf(out, size, "-");
    else snprintf(out, size, "%d%%", accuracy);
}

static s32 gunsBoxX(void) { return GUNS_LABEL_X + portHitMapInkWidth("GUN:", GUNS_SCALE) + GUNS_GAP; }

/* As wide as the widest entry, within limits. */
static s32 gunsBoxW(const GunsList *list)
{
    s32 i, w = GUNS_BOX_MIN;

    for (i = 0; i < list->count; i++) {
        char label[64];
        s32 need;

        gunsLabel(list->guns[i], label, sizeof(label));
        need = portHitMapInkWidth(label, GUNS_SCALE) + GUNS_PAD * 2 + GUNS_ARROW_W;
        if (need > w) w = need;
    }
    return w < GUNS_BOX_MAX ? w : GUNS_BOX_MAX;
}

static s32 gunsBoxTop(void) { return GUNS_Y - GUNS_TEXT_Y; }
static s32 gunsListTop(void) { return gunsBoxTop() + GUNS_LINE + 2; }

static s32 gunsBoxHover(const GunsList *list)
{
    return portHitMapCursorIn(gunsBoxX(), gunsBoxTop(), gunsBoxW(list), GUNS_LINE);
}

/* The open list's line under the cursor, or -1. */
static s32 gunsLineHover(const GunsList *list)
{
    const s32 x = gunsBoxX(), w = gunsBoxW(list) + GUNS_ACC_W, top = gunsListTop() + 2;
    s32 line;

    for (line = 0; line < gunsLines(list); line++) {
        if (portHitMapCursorIn(x, top + line * GUNS_LINE, w, GUNS_LINE - 1)) return line;
    }
    return -1;
}

void portHitMapGunsReset(void)
{
    gunsShown = PORT_HITMAP_ALL_GUNS;
    gunsOpen = FALSE;
    gunsPage = 0;
}

s32 portHitMapGunShown(void) { return gunsShown; }
s32 portHitMapGunsOpen(void) { return gunsOpen; }

s32 portHitMapGunBodyHits(s32 bodyHits)
{
    PortHitMapGunCounts c;

    if (gunsShown == PORT_HITMAP_ALL_GUNS) return bodyHits;
    c = portHitMapGunTotals(portHitMapGuns(), gunsShown);
    return c.head + c.body + c.limb;
}

/* Test harness only (GE_HITMAP_PROBE gun=<n>, list): pick the nth entry, open the list. */
void portHitMapGunsPick(s32 entry, s32 open)
{
    const GunsList list = gunsList();

    if (entry >= 0 && entry < list.count) gunsShown = list.guns[entry];
    gunsOpen = open != 0;
    gunsPage = 0;
}

static void gunsOpenAtShown(const GunsList *list)
{
    s32 i;

    gunsOpen = TRUE;
    gunsPage = 0;
    for (i = 0; i < list->count; i++) {
        if (list->guns[i] == gunsShown) gunsPage = i / gunsPerPage(list);
    }
}

s32 portHitMapGunsInput(s32 confirm, s32 back)
{
    const GunsList list = gunsList();
    s32 line;

    if (!gunsOpen) {
        if (!confirm || !gunsBoxHover(&list)) return FALSE;
        gunsOpenAtShown(&list);
        portHitMapSound();
        return TRUE;
    }
    if (!confirm && !back) return TRUE;
    line = confirm ? gunsLineHover(&list) : -1;
    if (line >= 0 && list.count > GUNS_ROWS && line == gunsLines(&list) - 1) {
        gunsPage = (gunsPage + 1) % gunsPages(&list);
    } else {
        if (line >= 0) gunsShown = list.guns[gunsPage * gunsPerPage(&list) + line];
        gunsOpen = FALSE;
    }
    portHitMapSound();
    return TRUE;
}

static Gfx *gunsRect(Gfx *g, s32 x0, s32 y0, s32 x1, s32 y1, u32 color)
{
    return microcode_constructor_related_to_menus(g, x0, y0, x1, y1, color);
}

/* An ink outline one pixel wide. */
static Gfx *gunsFrame(Gfx *g, s32 x0, s32 y0, s32 x1, s32 y1)
{
    g = gunsRect(g, x0, y0, x1, y0 + 1, INK);
    g = gunsRect(g, x0, y1 - 1, x1, y1, INK);
    g = gunsRect(g, x0, y0, x0 + 1, y1, INK);
    return gunsRect(g, x1 - 1, y0, x1, y1, INK);
}

/* A small down arrow, narrowing rows of ink. */
static Gfx *gunsArrow(Gfx *g, s32 cx, s32 top)
{
    s32 i;

    for (i = 0; i < 4; i++) g = gunsRect(g, cx - 3 + i, top + i, cx + 4 - i, top + i + 1, INK);
    return g;
}

/* GUN:, the box with the shown gun, and the shown guns' shots and accuracy. */
Gfx *portHitMapGunsRow(Gfx *g)
{
    const GunsList list = gunsList();
    const s32 x = gunsBoxX(), w = gunsBoxW(&list), top = gunsBoxTop();
    const PortHitMapGunCounts c = portHitMapGunTotals(portHitMapGuns(), gunsShown);
    char label[64], accuracy[16], stats[64];

    g = portHitMapInk(g, "GUN:", GUNS_LABEL_X, GUNS_Y, GUNS_SCALE, PAPER_TEXT);
    if (gunsOpen || gunsBoxHover(&list)) g = gunsRect(g, x, top, x + w, top + GUNS_LINE, COL_LIVE);
    g = gunsFrame(g, x, top, x + w, top + GUNS_LINE);
    gunsLabel(gunsShown, label, sizeof(label));
    g = portHitMapInk(g, label, x + GUNS_PAD, GUNS_Y, GUNS_SCALE, PAPER_TEXT);
    g = gunsArrow(g, x + w - GUNS_ARROW_W / 2 - 3, top + 5);
    gunsAccuracy(gunsShown, accuracy, sizeof(accuracy));
    snprintf(stats, sizeof(stats), "Shots fired: %d    Accuracy: %s", c.shots, accuracy);
    return portHitMapInk(g, stats, GUNS_STATS_R - portHitMapInkWidth(stats, GUNS_SCALE), GUNS_Y, GUNS_SCALE, PAPER_TEXT);
}

static Gfx *gunsLine(Gfx *g, const char *left, const char *right, s32 x, s32 w, s32 y, u32 color)
{
    g = portHitMapInk(g, left, x + GUNS_PAD, y + GUNS_TEXT_Y, GUNS_SCALE, color);
    if (right[0] == '\0') return g;
    return portHitMapInk(g, right, x + w - GUNS_PAD - portHitMapInkWidth(right, GUNS_SCALE), y + GUNS_TEXT_Y, GUNS_SCALE, color);
}

/* The open list, over the figures or the table; drawn last. */
Gfx *portHitMapGunsList(Gfx *g)
{
    const GunsList list = gunsList();
    const s32 x = gunsBoxX(), w = gunsBoxW(&list) + GUNS_ACC_W, top = gunsListTop();
    const s32 lines = gunsLines(&list), hover = gunsLineHover(&list), per = gunsPerPage(&list);
    s32 line;

    if (!gunsOpen) return g;
    g = gunsRect(g, x - 1, top - 1, x + w + 1, top + lines * GUNS_LINE + 4, COL_EDGE);
    g = gunsRect(g, x, top, x + w, top + lines * GUNS_LINE + 3, COL_PANEL);
    for (line = 0; line < lines; line++) {
        const s32 y = top + 2 + line * GUNS_LINE;
        const s32 more = list.count > GUNS_ROWS && line == lines - 1;
        const s32 gun = more ? 0 : list.guns[gunsPage * per + line];
        const u32 text = line == hover ? COL_HOT_TEXT : PAPER_TEXT;
        char left[64], right[24];

        if (line == hover) g = gunsRect(g, x + 1, y, x + w - 1, y + GUNS_LINE, COL_HOT);
        else if (!more && gun == gunsShown) g = gunsRect(g, x + 1, y, x + w - 1, y + GUNS_LINE, COL_LIVE);
        if (more) {
            snprintf(left, sizeof(left), "More guns (page %d of %d)", gunsPage + 1, gunsPages(&list));
            right[0] = '\0';
        } else {
            gunsLabel(gun, left, sizeof(left));
            gunsAccuracy(gun, right, sizeof(right));
        }
        g = gunsLine(g, left, right, x, w, y, text);
    }
    return microcode_constructor(g);
}
