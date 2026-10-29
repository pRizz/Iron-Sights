/* Campaign records beside the inherited difficulty list and completion marks. */
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <fr.h>
#include "game/file2.h"
#include "game/textrelated.h"
#include "port_mission_times.h"

extern save_data *fileGetSaveForFoldernum(u32 folder);
extern Gfx *frontPrintText(Gfx *dl, s32 *x, s32 *y, s8 *text, s32 chars, s32 font,
    s32 color, s32 viewX, s32 viewY, s32 arg9, s32 arga);

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

static Gfx *portMissionTimeLabel(Gfx *dl, char *text, s32 y)
{
    s32 height, width, x;
    textMeasure(&height, &width, text, ptrFontZurichBoldChars, ptrFontZurichBold, 0);
    x = 382 - width;
    dl = microcode_constructor(dl);
    return frontPrintText(dl, &x, &y, text, (s32)ptrFontZurichBoldChars, (s32)ptrFontZurichBold,
        0x000000ff, viGetX(), viGetY(), 0, 0);
}

Gfx *portMissionBestTimesDraw(Gfx *dl, s32 mission, s32 folder, s32 highestDifficulty)
{
    s32 difficulty, rows = highestDifficulty + 1;
    if (!ptrFontZurichBoldChars || !ptrFontZurichBold) return dl;
    if (rows < 1) rows = 1;
    if (rows > DIFFICULTY_MAX) rows = DIFFICULTY_MAX;
    dl = portMissionTimeLabel(dl, "Best Time", 143);
    for (difficulty = DIFFICULTY_AGENT; difficulty < rows; difficulty++) {
        char text[8];
        portMissionBestTimeText(mission, folder, difficulty, text, sizeof(text));
        dl = portMissionTimeLabel(dl, text, 180 + difficulty * 30);
    }
    return dl;
}
