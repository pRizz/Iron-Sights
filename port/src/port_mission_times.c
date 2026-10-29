/* Campaign records beside the inherited difficulty list and completion marks. */
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <fr.h>
#include "game/file2.h"
#include "game/textrelated.h"
#include "port_hud.h"
#include "port_mission_times.h"

extern save_data *fileGetSaveForFoldernum(u32 folder);

s32 portMissionBestTimeText(s32 mission, s32 folder, s32 difficulty, char *text, s32 size)
{
    save_data *maybeSave;
    s32 seconds;
    if (size <= 0) return 0;
    snprintf(text, size, "--:--");
    if (mission < SP_LEVEL_DAM || mission >= SP_LEVEL_MAX
        || folder < FOLDER1 || folder >= MAX_FOLDER_COUNT
        || difficulty < DIFFICULTY_AGENT || difficulty > DIFFICULTY_00) return 0;
    maybeSave = fileGetSaveForFoldernum(folder);
    if (!maybeSave) return 0;
    seconds = fileGetSaveStageDifficultyTime(maybeSave, mission, difficulty);
    /* Zero is unplayed; 1023 is the inherited completion/overflow sentinel. */
    if (seconds <= 0 || seconds >= 0x3ff) return 0;
    snprintf(text, size, "%02d:%02d", seconds / 60, seconds % 60);
    return 1;
}

static Gfx *portMissionTimeLabel(Gfx *dl, char *text, s32 y, f32 scale, u32 color)
{
    s32 height, width, x;
    portTextScale(scale);
    textMeasure(&height, &width, text, ptrFontZurichBoldChars, ptrFontZurichBold, 0);
    x = 382 - width;
    dl = microcode_constructor(dl);
    dl = textRender(dl, &x, &y, text, ptrFontZurichBoldChars, ptrFontZurichBold,
        color, viGetX(), viGetY(), 0, 0);
    portTextScale(1.0f);
    return dl;
}

Gfx *portMissionBestTimesDraw(Gfx *dl, s32 mission, s32 folder, s32 highestDifficulty)
{
    s32 difficulty, rows = highestDifficulty + 1;
    if (!ptrFontZurichBoldChars || !ptrFontZurichBold) return dl;
    if (rows < 1) rows = 1;
    if (rows > DIFFICULTY_MAX) rows = DIFFICULTY_MAX;
    dl = portMissionTimeLabel(dl, "BEST TIME", 143, 0.85f, 0x252b29ff);
    dl = portHudPanel(dl, 304, 170, 86, rows * 30 - 6, 0x10171cd0);
    dl = portHudPanel(dl, 304, 170, 86, 1, 0xb9a76a90);
    for (difficulty = DIFFICULTY_AGENT; difficulty < rows; difficulty++) {
        char text[8];
        const s32 recorded = portMissionBestTimeText(mission, folder, difficulty, text, sizeof(text));
        dl = portMissionTimeLabel(dl, text, 180 + difficulty * 30, 1.0f,
            recorded ? 0xf2ebceff : 0x929b9fff);
    }
    return dl;
}
