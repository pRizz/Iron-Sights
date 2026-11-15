/* Fork clock presentation, separate from upstream ammo and gadget rendering. */
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <fr.h>
#include "boss.h"
#include "game/bondview.h"
#include "game/gun.h"
#include "game/front.h"
#include "game/file.h"
#include "game/file2.h"
#include "game/lv.h"
#include "game/options.h"
#include "game/player.h"
#include "game/textrelated.h"
#include "gbi_extension.h"
#include "port_hud.h"
#include "port_mission_clock.h"
#include "port_noclip.h"

s32 portHudMissionBestSeconds(s32 stage, s32 difficulty)
{
    s32 i;
    save_data *save;
    /* 007's saved 0x3ff is an unlock flag, not a completion time. */
    if (difficulty < DIFFICULTY_AGENT || difficulty > DIFFICULTY_00) return 0;
    save = fileGetSaveForFoldernum(selected_folder_num);
    if (!save) return 0;
    for (i = 0; mission_folder_setup_entries[i].folder_text_preset; i++) {
        const struct mission_folder_setup *entry = &mission_folder_setup_entries[i];
        if (entry->stage_id == stage && entry->mission_num >= 0 && entry->mission_num < 20)
            return fileGetSaveStageDifficultyTime(save, entry->mission_num, difficulty);
    }
    return 0;
}

s32 portHudMissionTargetSeconds(s32 stage, s32 difficulty)
{
    s32 i;
    if (difficulty < DIFFICULTY_AGENT || difficulty > DIFFICULTY_00) return 0;
    /* Stage IDs and mission indices differ. Resolve the live stage, not the
     * currently highlighted folder/briefing row (also valid after a restore). */
    for (i = 0; mission_folder_setup_entries[i].folder_text_preset; i++) {
        const struct mission_folder_setup *entry = &mission_folder_setup_entries[i];
        if (entry->stage_id == stage && entry->mission_num >= 0 && entry->mission_num < 20)
            return solo_target_time_array[entry->mission_num][difficulty];
    }
    return 0;
}

void portHudMissionTimerText(s32 ticks, s32 target, char *text, s32 size)
{
    if (size <= 0) return;
    if (ticks < 0) ticks = 0;
    if (target > 0)
        snprintf(text, size, "%02d:%02d:%02d / CHEAT %02d:%02d", ticks/3600,
            (ticks/60)%60, ((ticks%60)*100)/60, target/60, target%60);
    else
        snprintf(text, size, "%02d:%02d:%02d", ticks/3600,
            (ticks/60)%60, ((ticks%60)*100)/60);
}

u32 portHudMissionTargetColor(s32 ticks, s32 target)
{
    const s32 seconds = ticks > 0 ? ticks/60 : 0;
    /* Match file.c's whole-second <= comparison, including the full final
     * second. Color describes time only; objectives/cheat rules still apply. */
    if (target <= 0) return 0xffffffff;
    if (seconds > target) return 0xff9d91ff;
    if (target-seconds <= 10) return 0xf0d994ff;
    return 0xb9ed9fff;
}

PortMissionClockLayout portMissionClockLayout(s32 left, s32 top, s32 lineHeight, s32 hasBest)
{
    PortMissionClockLayout layout;
    const s32 height = lineHeight > 0 ? lineHeight : 8;
    layout.x = left + 30;
    layout.elapsedY = top + 4 + (height + 1) / 2;
    layout.bestY = layout.elapsedY + height + 2;
    layout.bottom = top + 4 + height + (hasBest ? height + 2 : 0);
    layout.toastY = layout.bottom + 4 > top + 28 ? layout.bottom + 4 : top + 28;
    return layout;
}

/* ---- the top-left text corner (pure layout) ------------------------------ */

PortHudBox portHudFpsLayout(s32 left, s32 top, s32 safeX, s32 safeY, s32 textWidth, s32 textHeight)
{
    PortHudBox box;
    /* Two units in from the corner, plus the one-unit outline. */
    box.x = left + 3 + safeX;
    box.y = top + 3 + safeY;
    box.right = box.x + textWidth + 1;
    box.bottom = box.y + textHeight + 1;
    return box;
}

s32 portHudClockOffsetX(s32 left, s32 fpsRight, s32 safeX, s32 scale)
{
    s32 offset = safeX * scale;
    if (fpsRight > 0) {
        /* The clock's text starts at left + 30 layout units, which HUD Scale
         * halves about the view's left edge; leave four screen units. */
        const s32 clear = (fpsRight + 4 - left) * scale - 30;
        if (clear > offset) offset = clear;
    }
    return offset > 0 ? offset : 0;
}

s32 portHudSubtitleLayoutTop(s32 viewTop, s32 textY, s32 clockBottom, s32 smallHud, s32 screenBottom)
{
    const s32 scale = smallHud ? 2 : 1;
    s32 obstacle = viewTop, minimumY;
    /* The clock is laid out in the subtitle's scaled units; round its scaled
     * bottom outward. */
    if (clockBottom > viewTop) obstacle = viewTop + (clockBottom - viewTop + scale - 1) / scale;
    if (screenBottom > obstacle) obstacle = screenBottom;
    if (obstacle <= viewTop) return textY;
    /* The subtitle bar starts two units above its text; leave four visible
     * units between the obstacle and that bar. */
    minimumY = viewTop + (obstacle - viewTop + 4) * scale + 2;
    return textY > minimumY ? textY : minimumY;
}

s32 portHudObjectiveLayoutTop(s32 viewTop, s32 clockToastY, s32 subtitleBottom, s32 screenBottom)
{
    s32 top = viewTop + 28;
    if (clockToastY > top) top = clockToastY;
    if (subtitleBottom > 0 && subtitleBottom + 4 > top) top = subtitleBottom + 4;
    if (screenBottom > 0 && screenBottom + 4 > top) top = screenBottom + 4;
    return top;
}

s32 portHudDebugLayoutTop(s32 base, s32 fpsBottom, s32 clockBottom, s32 labelBottom)
{
    s32 top = base;
    if (fpsBottom > 0 && fpsBottom + 4 > top) top = fpsBottom + 4;
    if (clockBottom > 0 && clockBottom + 4 > top) top = clockBottom + 4;
    if (labelBottom > 0 && labelBottom + 4 > top) top = labelBottom + 4;
    return top;
}

/* ---- the top-left text corner (this frame) ------------------------------- */

s32 portMissionTimerShown(void)
{
    extern s32 portMissionTimer(void);
    return portMissionTimer()
        && (g_CurrentPlayer->gunammooff & ~GUNAMMOREASON_OPTION) == 0
        && g_CurrentPlayer->mpmenuon == 0
        && getPlayerCount() == 1;
}

void portHudSafeArea(s32 *dx, s32 *dy)
{
    *dx = 0;
    *dy = 0;
#if defined(__EMSCRIPTEN__) && defined(GE_VIDEO)
    {   /* a browser canvas under a notch or rounded corner (safe-area.ts) */
        extern void portWebSafeAreaTopLeft(s32 viX, s32 viY, s32 *dx, s32 *dy);
        portWebSafeAreaTopLeft(viGetX(), viGetY(), dx, dy);
    }
#else
    {   /* GE_HUD_SAFE=x,y: a simulated inset in HUD units, so native audits
         * can capture the browser's notch layout */
        static s32 envX = -1, envY;
        if (envX < 0) {
            const char *e = getenv("GE_HUD_SAFE");
            envX = envY = 0;
            if (e == NULL || sscanf(e, "%d,%d", &envX, &envY) != 2 || envX < 0 || envY < 0) envX = envY = 0;
        }
        *dx = envX;
        *dy = envY;
    }
#endif
}

static s32 hudScale(void)
{
    return portHudScaleOn() ? 2 : 1;
}

static s32 gothicLineHeight(void)
{
    s32 height = 0, width = 0;
    /* textMeasure accumulates height only at a newline, like ammo digits. */
    textMeasure(&height, &width, "0\n", ptrFontBankGothicChars, ptrFontBankGothic, 0);
    return height;
}

static s32 fpsShown(void)
{
    extern s32 portVideoShowFps(void); /* video.c; weak 0 headless */
    return ptrFontBankGothic && ptrFontBankGothicChars && portVideoShowFps()
        && (getPlayerCount() == 1 || get_cur_playernum() == 0);
}

static char *fpsText(void)
{
    extern s32 g_PortStatFps; /* frametiming.c, from gfx_sdl2's 500 ms stat window */
    static char text[16];
    snprintf(text, sizeof(text), "%d", (int) g_PortStatFps);
    return text;
}

/* The FPS counter's box this frame, all zero while it is hidden. Its width
 * is reserved for at least three digits, so the clock beside it does not
 * shift as the rate crosses 99/100. */
static PortHudBox fpsBox(void)
{
    extern s32 portFullHeightActive(void); /* bondview2.c */
    PortHudBox box = {0, 0, 0, 0};
    s32 height = 0, width = 0, reserve = 0, safeX, safeY, top;
    if (!fpsShown()) return box;
    textMeasure(&height, &width, fpsText(), ptrFontBankGothicChars, ptrFontBankGothic, 0);
    textMeasure(&height, &reserve, "000", ptrFontBankGothicChars, ptrFontBankGothic, 0);
    portHudSafeArea(&safeX, &safeY);
    /* Solo full height keeps the top row while paused, since the view snaps
     * 10 lines down at the end of the pause-box ease. */
    top = (getPlayerCount() == 1 && portFullHeightActive()) ? 0 : viGetViewTop();
    return portHudFpsLayout(viGetViewLeft(), top, safeX, safeY,
                            width > reserve ? width : reserve, gothicLineHeight());
}

static PortMissionClockLayout clockLayout(s32 hasBest)
{
    const s32 scale = hudScale(), left = viGetViewLeft();
    s32 safeX, safeY;
    portHudSafeArea(&safeX, &safeY);
    /* The safe area only reaches the clock where it sits at the window edge
     * (Wide HUD); the 4:3 band is clear of it. */
    if (!hudAnchorsToEdges()) safeX = 0;
    return portMissionClockLayout(left + portHudClockOffsetX(left, fpsBox().right, safeX, scale),
                                  viGetViewTop() + safeY * scale, gothicLineHeight(), hasBest);
}

static s32 clockHasBest(void)
{
    return portHudMissionBestSeconds(bossGetStageNum(), lvlGetSelectedDifficulty()) > 0;
}

static s32 clockReady(void)
{
    return ptrFontBankGothic && ptrFontBankGothicChars && portMissionTimerShown();
}

/* The clock's bottom edge on screen (after HUD Scale), 0 while hidden. */
static s32 clockScreenBottom(void)
{
    const s32 scale = hudScale(), top = viGetViewTop();
    if (!clockReady()) return 0;
    return top + (clockLayout(clockHasBest()).bottom - top + scale - 1) / scale;
}

s32 portHudSubtitleTop(s32 textX, s32 textY)
{
    const s32 scale = hudScale(), left = viGetViewLeft();
    const PortHudBox fps = fpsBox();
    const s32 clockBottom = clockReady() ? clockLayout(clockHasBest()).bottom : 0;
    s32 fpsBottom = 0;
    /* The bar spans the width and the counter draws over it, so the FPS
     * counter only moves the text when it reaches the text's columns. */
    if (fps.bottom > 0 && fps.right + 2 > left + (textX - left) / scale) fpsBottom = fps.bottom;
    return portHudSubtitleLayoutTop(viGetViewTop(), textY, clockBottom, portHudScaleOn(), fpsBottom);
}

static s32 objectiveSubtitleBottom;
static s32 objectiveCardBottom;

void portHudSubtitleBottom(s32 bottom)
{
    objectiveSubtitleBottom = bottom;
}

void portHudObjectiveCardBottom(s32 bottom)
{
    objectiveCardBottom = bottom;
}

s32 portHudObjectiveTop(void)
{
    const s32 clockToastY = clockReady() ? clockLayout(clockHasBest()).toastY : 0;
    return portHudObjectiveLayoutTop(viGetViewTop(), clockToastY, objectiveSubtitleBottom, fpsBox().bottom);
}

/* The NOCLIP label (port_noclip.c) sits at the view's top right, 0.7 scale. */
static s32 noclipLabelBottom(void)
{
    if (!portNoclipActive() || !ptrFontBankGothic || !ptrFontBankGothicChars) return 0;
    return viGetViewTop() + 12 + (gothicLineHeight() * 7 + 9) / 10;
}

s32 portHudDebugRowsTop(void)
{
    /* FORK-084's floor: the rows start no nearer the top than the safe area */
    s32 safeX, safeY, top = viGetViewTop();
    portHudSafeArea(&safeX, &safeY);
    if (safeY > top) top = safeY;
    return portHudDebugLayoutTop(top + 0x11, fpsBox().bottom, clockScreenBottom(), noclipLabelBottom());
}

/* How many debug readout rows draw from portHudDebugRowsTop (bondview2.c). */
static s32 debugRowCount(void)
{
    extern s32 get_debug_testingmanpos_flag(void); /* debugmenu_handler.c */
    extern s32 portInputShowInput(void);           /* input_pc.c; weak 0 headless */
    return (get_debug_testingmanpos_flag() ? 1 : 0) + (portInputShowInput() ? 1 : 0);
}

static s32 topReserved;

static void reserveTop(s32 bottom)
{
    if (bottom > topReserved) topReserved = bottom;
}

Gfx *portHudDrawFpsCounter(Gfx *gdl)
{
    const PortHudBox fps = fpsBox();
    if (getPlayerCount() == 1 || get_cur_playernum() == 0) {
        /* Everything at the top left this frame. This draws last, after the
         * subtitle and objective card recorded their edges. */
        const s32 rows = debugRowCount();
        topReserved = 0;
        reserveTop(fps.bottom);
        reserveTop(clockScreenBottom());
        reserveTop(noclipLabelBottom());
        if (rows > 0) reserveTop(portHudDebugRowsTop() + (rows - 1) * 0xf + 0xb);
        reserveTop(objectiveSubtitleBottom);
        reserveTop(objectiveCardBottom);
    }
    if (fps.bottom == 0) return gdl;
    {
        /* Just the number, in the small Gothic font, OUTLINED white on
         * 0x646464FF like the HUD's own counters so the digits stay readable
         * over a bright sky; always anchored to the window's left edge. */
        s32 x = fps.x, y = fps.y;
        gdl = microcode_constructor(gdl);
        PORT_HUD_ANCHOR_FULL(gdl, PORT_HUD_ANCHOR_LEFT);
        gdl = textRenderOutlined(gdl, &x, &y, fpsText(), ptrFontBankGothicChars, ptrFontBankGothic, -1,
                                 0x646464FFU, viGetX(), viGetY(), 0, 0);
        PORT_HUD_ANCHOR_OFF(gdl);
        gdl = combiner_bayer_lod_perspective(gdl);
    }
    return gdl;
}

s32 portHudTakeTopReserved(void)
{
    const s32 bottom = topReserved;
    topReserved = 0;
    return bottom;
}


Gfx *portHudDrawMissionTimer(Gfx *gdl)
{
    char timer[64], glyph[3] = {'0', '\n', '\0'};
    s32 digitCell = 0, colonCell, height, width, i, x;
    s32 targetText = FALSE;
    const s32 stage = bossGetStageNum(), difficulty = lvlGetSelectedDifficulty();
    const s32 target = portHudMissionTargetSeconds(stage, difficulty);
    const s32 best = portHudMissionBestSeconds(stage, difficulty);
    const s32 ticks = getMissiontimer();
    const u32 targetColor = portHudMissionTargetColor(ticks, target);
    PortMissionClockLayout layout;
    if (!ptrFontBankGothic || !ptrFontBankGothicChars) return gdl;
    layout = clockLayout(best > 0);
    portHudMissionTimerText(ticks, target, timer, sizeof(timer));
    for (i = 0; i <= 9; i++) {
        glyph[0] = '0' + i;
        textMeasure(&height, &width, glyph, ptrFontBankGothicChars, ptrFontBankGothic, 0);
        if (width > digitCell) digitCell = width;
    }
    glyph[0] = ':';
    textMeasure(&height, &width, glyph, ptrFontBankGothicChars, ptrFontBankGothic, 0);
    colonCell = width;
    if (hudAnchorsToEdges()) PORT_HUD_ANCHOR(gdl, PORT_HUD_ANCHOR_LEFT);
    gdl = portHudScaleBegin(gdl, viGetViewLeft(), viGetViewTop());
    gdl = microcode_constructor(gdl);
    x = layout.x;
    for (i = 0; timer[i]; i++) {
        s32 cell;
        glyph[0] = timer[i];
        textMeasure(&height, &width, glyph, ptrFontBankGothicChars, ptrFontBankGothic, 0);
        cell = glyph[0] >= '0' && glyph[0] <= '9' ? digitCell : glyph[0] == ':' ? colonCell : width;
        if (glyph[0] == '/') targetText = TRUE;
        if (targetText) {
            s32 gx = x + (cell + 1) / 2 - (width + 1) / 2;
            s32 gy = layout.elapsedY - (height + 1) / 2;
            gdl = textRenderOutlined(gdl, &gx, &gy, (s8 *)glyph,
                ptrFontBankGothicChars, ptrFontBankGothic, targetColor, 0x202020ff,
                viGetX(), viGetY(), 0, 0);
        } else {
            gdl = gunDrawHudString(gdl, (s8 *)glyph, x + (cell + 1) / 2,
                HUDHALIGN_MIDDLE, layout.elapsedY, HUDVALIGN_MIDDLE, TRUE);
        }
        x += cell + 1;
    }
    if (best > 0) {
        /* Campaign records have whole-second precision and cap at 1023. */
        snprintf(timer, sizeof(timer), "BEST %02d:%02d%s\n", best/60, best%60, best == 1023 ? "+" : "");
        gdl = gunDrawHudString(gdl, (s8 *)timer, layout.x, HUDHALIGN_LEFT,
            layout.bestY, HUDVALIGN_MIDDLE, TRUE);
    }
    gdl = combiner_bayer_lod_perspective(gdl);
    gdl = portHudScaleEnd(gdl);
    if (hudAnchorsToEdges()) PORT_HUD_ANCHOR_OFF(gdl);
    return gdl;
}
