/* Fork clock presentation, separate from upstream ammo and gadget rendering. */
#include <stdio.h>
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

s32 portMissionTimerShown(void)
{
    extern s32 portMissionTimer(void);
    return portMissionTimer()
        && (g_CurrentPlayer->gunammooff & ~GUNAMMOREASON_OPTION) == 0
        && g_CurrentPlayer->mpmenuon == 0
        && getPlayerCount() == 1
        /* Dialogue owns the upper HUD while its text window is active. */
        && (g_UpperTextDisplayFlag != 0 || upper_text_window_timer < 0);
}

static PortMissionClockLayout clockLayout(s32 hasBest)
{
    s32 height = 0, width = 0;
    /* textMeasure accumulates height only at a newline, like ammo digits. */
    textMeasure(&height, &width, "0\n", ptrFontBankGothicChars, ptrFontBankGothic, 0);
    return portMissionClockLayout(viGetViewLeft(), viGetViewTop(), height, hasBest);
}

static s32 objectiveSubtitleBottom;

void portHudSubtitleBottom(s32 bottom)
{
    objectiveSubtitleBottom = bottom;
}

s32 portHudObjectiveLayoutTop(s32 viewTop, s32 clockToastY, s32 subtitleBottom)
{
    s32 top = viewTop + 28;
    if (clockToastY > top) top = clockToastY;
    if (subtitleBottom > 0 && subtitleBottom + 4 > top) top = subtitleBottom + 4;
    return top;
}

s32 portHudObjectiveTop(void)
{
    s32 clockToastY = 0;
    if (portMissionTimerShown() && ptrFontBankGothic && ptrFontBankGothicChars)
        clockToastY = clockLayout(portHudMissionBestSeconds(bossGetStageNum(), lvlGetSelectedDifficulty()) > 0).toastY;
    return portHudObjectiveLayoutTop(viGetViewTop(), clockToastY, objectiveSubtitleBottom);
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
