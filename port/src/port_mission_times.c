/* Campaign records beside the inherited difficulty list and completion marks:
 * the stock best times under a red checkmark heading, then (FORK-083) the
 * Headshots Only records (port_headshots_records.c) under the crosshair mark. */
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <fr.h>
#include "game/file2.h"
#include "game/textrelated.h"
#include "game/front.h"
#include "game/image_bank.h"
#include "game/othermodemicrocode.h"
#include "game/bondwalk2.h"
#include "assets/image_externs.h"
#include "port_mission_times.h"
#include "port_headshots_only.h"   /* FORK-083: the headshot records column */
#include "port_headshots_menu.h"

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
    /* Zero is unplayed. The save caps times at 1023 s, so 1023 is a
     * completion of 17:03 or slower: "17:03+", as the Mission Timer's BEST. */
    if (seconds <= 0) return 0;
    if (seconds > 0x3ff) seconds = 0x3ff;
    snprintf(text, size, "%02d:%02d%s", seconds / 60, seconds % 60, seconds == 0x3ff ? "+" : "");
    return 1;
}

/* Two time columns between the difficulty names (and cheat-target badges,
 * which end near x 238) and the dossier paper's edge at x 388: the stock
 * records first, then the Headshots Only ones. */
#define TIMES_X 300
#define HEADSHOT_TIMES_X 362
#define HEADING_ICON_Y 149.0f
/* the checkmark's bottom point sits 4.2 units left of its texture's centre;
 * the heading puts that point over the column's centre */
#define CHECK_TIP_OFFSET_X 4.2f
/* reach about 9: the mark spans about the 18-unit square around the
 * checkmark heading */
#define HEADSHOT_HEADING_RING 6.6f
#define ROWS_Y 180
#define ROWS_PITCH 30
#define MARK_RED 0xB40000FFu   /* the checkmark's red (front.c draws it 0xB4,0,0) */

static Gfx *portMissionTimeLabel(Gfx *dl, char *text, s32 columnCenterX, s32 y)
{
    s32 height, width, x;
    textMeasure(&height, &width, text, ptrFontZurichBoldChars, ptrFontZurichBold, 0);
    x = columnCenterX - width / 2;
    dl = microcode_constructor(dl);
    return frontPrintText(dl, &x, &y, text, ptrFontZurichBoldChars, ptrFontZurichBold,
        0x000000ff, viGetX(), viGetY(), 0, 0);
}

/* The stock completion checkmark as the stock column's heading. The port no
 * longer draws it on each completed row (front.c keeps that for N64): the
 * times say the same. */
static Gfx *portMissionCheckHeading(Gfx *dl, f32 x, f32 y)
{
    struct sImageTableEntry *image = &mainfolderimages[IMAGE_CHECK];
    f32 xypos[2], halfedxy[2];
    xypos[0] = x;
    xypos[1] = y;
    halfedxy[0] = image->width * 0.5f;
    halfedxy[1] = image->height * 0.5f;
    texSelect(&dl, image, 4, 0, 0);
    display_image_at_position(&dl, xypos, halfedxy, image->width, image->height, 0, 0, 1,
        0xB4, 0, 0, 0xFF, image->level > 0, 0);
    return dl;
}

Gfx *portMissionBestTimesDraw(Gfx *dl, s32 mission, s32 folder, s32 highestDifficulty)
{
    s32 difficulty, rows = highestDifficulty + 1;
    if (!ptrFontZurichBoldChars || !ptrFontZurichBold) return dl;
    if (rows < 1) rows = 1;
    if (rows > DIFFICULTY_MAX) rows = DIFFICULTY_MAX;
    /* the icon headings first: each text label below resets the combiner */
    dl = portMissionCheckHeading(dl, (f32)TIMES_X + CHECK_TIP_OFFSET_X, HEADING_ICON_Y);
    dl = portHeadshotsMarkDraw(dl, (f32)HEADSHOT_TIMES_X, HEADING_ICON_Y, HEADSHOT_HEADING_RING, MARK_RED);
    for (difficulty = DIFFICULTY_AGENT; difficulty < rows; difficulty++) {
        char text[8];
        const s32 y = ROWS_Y + difficulty * ROWS_PITCH;
        portMissionBestTimeText(mission, folder, difficulty, text, sizeof(text));
        dl = portMissionTimeLabel(dl, text, TIMES_X, y);
        /* 007 is never recorded, so its row reads --:-- in both columns */
        portHeadshotsBestTimeText(folder, mission, difficulty, text, sizeof(text));
        dl = portMissionTimeLabel(dl, text, HEADSHOT_TIMES_X, y);
    }
    return dl;
}
