/* ROM-free checks that the HUD and watch ammo counters read an ammo icon's
 * width by name at each pointer width (LP64-005 strict batch 1, entry 14,
 * test_lp64_typed_reads.sh). gunfire.c is included whole, with stubs for what
 * generate_ammo_total_microcode and gunDrawWatchAmmoDisplay call;
 * --gc-sections drops the rest. The counters' numbers are placed either side
 * of the icon by its width, so the stubbed text renderer records where each is
 * drawn. The icon's index holds an invented number whose high half is zero, so
 * a read of byte 4 at 64 bits sees zero instead of the width. */
#include <stdio.h>
#include <stdlib.h>
#include "../../src/game/gunfire.c"

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_ammo: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

#define ICON_WIDTH 20
#define AMMO_TYPE 1
#define ICON_9MM 0x02000C84u
#define VIEW_WIDTH 320
#define VIEW_HEIGHT 240

static sImageTableEntry icon = {0x1234, ICON_WIDTH, 12, 0, 0, 0, 0, 0, 0};
struct sImageTableEntry *ammo9mmimage = &icon, *rifleammoimage, *shotgunammoimage, *knifeammoimage,
    *glaunchammoimage, *rocketammoimage, *genericmineammoimage, *grenadeammoimage, *magnumammoimage,
    *goldengunammoimage, *remotemineammoimage, *timedmineammoimage, *proxmineammoimage, *tankammoimage;
AmmoStats ammo_related[AMMO_RELATED_MAX];
struct player *g_CurrentPlayer;
struct fontchar *ptrFontBankGothicChars;
struct font *ptrFontBankGothic;

/* Where each number is drawn, in drawing order. */
static s32 drawnX[8];
static s32 drawnCount;

static ITEM_IDS weapons[2];

ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand) { return weapons[hand]; }
u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 mask) { return 0; }
WeaponStats *get_ptr_item_statistics(ITEM_IDS item)
{
    static WeaponStats stats;
    stats.AmmoType = AMMO_TYPE;
    return &stats;
}
s32 getPlayerCount(void) { return 1; }
s32 get_cur_playernum(void) { return 0; }
s16 viGetViewLeft(void) { return 0; }
s16 viGetViewTop(void) { return 0; }
s16 viGetViewWidth(void) { return VIEW_WIDTH; }
s16 viGetViewHeight(void) { return VIEW_HEIGHT; }
s16 viGetX(void) { return VIEW_WIDTH; }
s16 viGetY(void) { return VIEW_HEIGHT; }
Gfx *microcode_constructor(Gfx *gdl) { return gdl; }
Gfx *combiner_bayer_lod_perspective(Gfx *gdl) { return gdl; }
Gfx *draw_blackbox_to_screen(Gfx *gdl, s32 *x1, s32 *y1, s32 *x2, s32 *y2) { return gdl; }
void texSelect(Gfx **gdlptr, struct sImageTableEntry *tconfig, u32 arg2, s32 arg3, u32 ulst) {}
void display_image_at_position(Gfx **gdlptr, f32 *position, f32 *size, s32 twidth, u32 theight, u32 flipXY, u32 flipX,
                               u32 flipY, u32 r, u32 g, u32 b, u32 alpha, u32 mipmapped_texture, u32 highlight_mode)
{
}
Gfx *portHudAmmoIconTag(Gfx *gdl, const struct sImageTableEntry *image) { return gdl; }
Gfx *portHudScaleBegin(Gfx *gdl, f32 x, f32 y) { return gdl; }
Gfx *portHudScaleEnd(Gfx *gdl) { return gdl; }
Gfx *portHudTouchClearTag(Gfx *gdl, s32 tag) { return gdl; }
s32 hudLayoutWide(void) { return 0; }
s32 hudAnchorsToEdges(void) { return 0; }
s32 portMissionTimerShown(void) { return 0; }
s32 portMpShowNames(void) { return 0; }
const char *portJoyPortGadgetHudName(void) { return NULL; }
const char *portProfilePlayerLabel(s32 player) { return NULL; }
u32 portHudAnchorFrac(void) { return 0; }
Gfx *portHudDrawMissionTimer(Gfx *gdl) { return gdl; }
f32 getPlayer_c_screenleft(void) { return 0.0f; }
f32 getPlayer_c_screenwidth(void) { return VIEW_WIDTH; }
const char g_GunHudIntegerFormat[] = "%d\n";

void textMeasure(s32 *textheight, s32 *textwidth, char *text, struct fontchar *font1, struct font *font2, s32 lineheight)
{
    *textheight = 0;
    *textwidth = 0;
}

static void recordDrawnX(s32 x)
{
    if (drawnCount < (s32) (sizeof(drawnX) / sizeof(drawnX[0])))
    {
        drawnX[drawnCount] = x;
    }
    drawnCount++;
}

Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text, struct fontchar *chars, struct font *font, u32 colour, s32 width,
                s32 height, u32 yOffset, s32 lineheight)
{
    recordDrawnX(*x);
    return gdl;
}

Gfx *textRenderOutlined(Gfx *gdl, s32 *x, s32 *y, char *text, struct fontchar *chars, struct font *font, u32 colour,
                        u32 colour2, s32 width, s32 height, s32 yOffset, s32 lineheight)
{
    recordDrawnX(*x);
    return gdl;
}

/* A player holding a 9mm gun in the hands asked, with ammo in the magazine
 * and in reserve, so both numbers are drawn. */
static void arrangePlayer(ITEM_IDS right, ITEM_IDS left)
{
    g_CurrentPlayer = calloc(1, sizeof(*g_CurrentPlayer));
    g_CurrentPlayer->hands[0].weapon_ammo_in_magazine = 7;
    g_CurrentPlayer->hands[1].weapon_ammo_in_magazine = 7;
    g_CurrentPlayer->ammoheldarr[AMMO_TYPE] = 30;
    ammo_related[AMMO_TYPE].IconImage = ICON_9MM;
    weapons[GUNRIGHT] = right;
    weapons[GUNLEFT] = left;
    drawnCount = 0;
}

/* The watch's magazine count ends half an icon left of 196, its reserve count
 * starts half an icon right of 203. */
static void testWatchCountsSitEitherSideOfTheIcon(void)
{
    // Arrange
    static Gfx dl[512];
    arrangePlayer(ITEM_WPPK, ITEM_UNARMED);

    // Act
    gunDrawWatchAmmoDisplay(dl);

    // Assert
    expect(drawnCount == 2, "the watch draws two numbers");
    expect(drawnX[0] == 196 - ICON_WIDTH / 2, "the watch's magazine count is placed by the icon's width");
    expect(drawnX[1] == 203 + (ICON_WIDTH + 1) / 2, "the watch's reserve count is placed by the icon's width");
    free(g_CurrentPlayer);
}

/* The right hand's counts are placed about the icon at the view's right. */
static void testRightHandCountsSitEitherSideOfTheIcon(void)
{
    // Arrange
    static Gfx dl[512];
    arrangePlayer(ITEM_WPPK, ITEM_UNARMED);

    // Act
    generate_ammo_total_microcode(dl);

    // Assert
    expect(drawnCount == 2, "the HUD draws the right hand's two numbers");
    expect(drawnX[0] == VIEW_WIDTH - 59 - ICON_WIDTH / 2 - 4, "the right magazine count is placed by the icon's width");
    expect(drawnX[1] == VIEW_WIDTH - 59 + (ICON_WIDTH + 1) / 2 + 3, "the right reserve count is placed by the icon's width");
    free(g_CurrentPlayer);
}

/* The left hand's counts are placed about the icon at the view's left. */
static void testLeftHandCountsSitEitherSideOfTheIcon(void)
{
    // Arrange: the left hand only, so its numbers are the first drawn.
    static Gfx dl[512];
    arrangePlayer(ITEM_UNARMED, ITEM_WPPK);

    // Act
    generate_ammo_total_microcode(dl);

    // Assert
    expect(drawnCount == 2, "the HUD draws the left hand's two numbers");
    expect(drawnX[0] == 59 + ICON_WIDTH / 2 + 3, "the left magazine count is placed by the icon's width");
    expect(drawnX[1] == 59 - (ICON_WIDTH + 1) / 2 - 4, "the left reserve count is placed by the icon's width");
    free(g_CurrentPlayer);
}

int main(void)
{
    testWatchCountsSitEitherSideOfTheIcon();
    testRightHandCountsSitEitherSideOfTheIcon();
    testLeftHandCountsSitEitherSideOfTheIcon();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_ammo: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
