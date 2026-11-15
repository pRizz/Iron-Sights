/* Headshots Only (FORK-083): headshot completions and best times per campaign
 * folder, kept beside the save in Data/Saves/headshots.ini:
 *
 *     [headshots_only]
 *     version=1
 *     f1_m00_d0=95      folder 1, Dam (SP_LEVEL_DAM), Agent: 01:35
 *
 * The N64 save has no room for them, so they live in their own file. A
 * missing key reads as none, an unknown or out-of-range one is ignored, and
 * 1023 marks a completion too long to time, as in the stock save. Erasing a
 * folder clears its records and copying one copies them (file2.c). The
 * difficulty page only shows a record whose stock completion the save also
 * has, so a replaced or reset eeprom.bin never shows stale marks.
 *
 * Harness runs (GE_NOINISAVE or GE_WINDOW) read the file but never write it. */
#include <stdio.h>
#include <stdlib.h>
#include <fr.h>
#include "game/file2.h"
#include "port_headshots_only.h"

#define RECORDS_FILE "Data/Saves/headshots.ini"
#define RECORDS_VERSION 1

extern const char *portRootPath(const char *name);   /* main.c */
extern save_data *fileGetSaveForFoldernum(u32 folder);

static s16 records[PORT_HEADSHOTS_FOLDERS][SP_LEVEL_MAX][PORT_HEADSHOTS_DIFFICULTIES];
static s32 loaded = FALSE;

/* Clears one folder, or copies another into it (from < 0 clears). */
static void setFolder(s32 to, s32 from)
{
    s32 mission, difficulty;
    for (mission = 0; mission < SP_LEVEL_MAX; mission++)
        for (difficulty = 0; difficulty < PORT_HEADSHOTS_DIFFICULTIES; difficulty++)
            records[to][mission][difficulty] = from < 0 ? 0 : records[from][mission][difficulty];
}

static s32 inRange(s32 folder, s32 mission, s32 difficulty)
{
    return folder >= FOLDER1 && folder < PORT_HEADSHOTS_FOLDERS
        && mission >= SP_LEVEL_DAM && mission < SP_LEVEL_MAX
        && difficulty >= DIFFICULTY_AGENT && difficulty < PORT_HEADSHOTS_DIFFICULTIES;
}

/* One "fN_mNN_dN=seconds" line; FALSE for anything else. */
static s32 parseLine(const char *line, s32 *folder, s32 *mission, s32 *difficulty, s32 *seconds)
{
    int f, m, d, s;
    if (sscanf(line, "f%d_m%d_d%d=%d", &f, &m, &d, &s) != 4) return FALSE;
    *folder = f - 1;
    *mission = m;
    *difficulty = d;
    *seconds = s;
    return inRange(*folder, m, d) && s > 0 && s <= PORT_HEADSHOTS_UNTIMED;
}

static void load(void)
{
    char line[96];
    s32 folder;
    FILE *f;
    if (loaded) return;
    loaded = TRUE;
    for (folder = 0; folder < PORT_HEADSHOTS_FOLDERS; folder++) setFolder(folder, -1);
    f = fopen(portRootPath(RECORDS_FILE), "r");
    if (f == NULL) return;
    while (fgets(line, sizeof(line), f) != NULL) {
        s32 mission, difficulty, seconds;
        if (parseLine(line, &folder, &mission, &difficulty, &seconds)) {
            records[folder][mission][difficulty] = (s16)seconds;
        }
    }
    fclose(f);
}

static s32 writesAllowed(void)
{
    return getenv("GE_NOINISAVE") == NULL && getenv("GE_WINDOW") == NULL;
}

static void save(void)
{
    s32 folder, mission, difficulty;
    FILE *f;
    if (!writesAllowed()) return;
    f = fopen(portRootPath(RECORDS_FILE), "w");
    if (f == NULL) {
        fprintf(stderr, "ge: headshots: cannot write %s\n", portRootPath(RECORDS_FILE));
        return;
    }
    fprintf(f, "[headshots_only]\nversion=%d\n", RECORDS_VERSION);
    for (folder = 0; folder < PORT_HEADSHOTS_FOLDERS; folder++)
        for (mission = SP_LEVEL_DAM; mission < SP_LEVEL_MAX; mission++)
            for (difficulty = 0; difficulty < PORT_HEADSHOTS_DIFFICULTIES; difficulty++)
                if (records[folder][mission][difficulty] > 0)
                    fprintf(f, "f%d_m%02d_d%d=%d\n", (int)folder + 1, (int)mission, (int)difficulty,
                        (int)records[folder][mission][difficulty]);
    fclose(f);
}

s32 portHeadshotsRecordGet(s32 folder, s32 mission, s32 difficulty)
{
    if (!inRange(folder, mission, difficulty)) return 0;
    load();
    return records[folder][mission][difficulty];
}

s32 portHeadshotsRecordShown(s32 folder, s32 mission, s32 difficulty)
{
    save_data *maybeSave;
    if (portHeadshotsRecordGet(folder, mission, difficulty) <= 0) return FALSE;
    maybeSave = fileGetSaveForFoldernum(folder);
    return maybeSave != NULL && fileGetSaveStageDifficultyTime(maybeSave, mission, difficulty) != 0;
}

s32 portHeadshotsBestTimeText(s32 folder, s32 mission, s32 difficulty, char *text, s32 size)
{
    s32 seconds;
    if (size <= 0) return FALSE;
    snprintf(text, size, "--:--");
    if (!portHeadshotsRecordShown(folder, mission, difficulty)) return FALSE;
    seconds = portHeadshotsRecordGet(folder, mission, difficulty);
    if (seconds <= 0) return FALSE;
    /* 1023 is a completion of 17:03 or slower, as in the stock save */
    if (seconds > PORT_HEADSHOTS_UNTIMED) seconds = PORT_HEADSHOTS_UNTIMED;
    snprintf(text, size, "%02d:%02d%s", seconds / 60, seconds % 60, seconds == PORT_HEADSHOTS_UNTIMED ? "+" : "");
    return TRUE;
}

void portHeadshotsMissionComplete(s32 folder, s32 mission, s32 difficulty, s32 seconds)
{
    s16 *kept;
    if (!portHeadshotsOnlyStageOn() || !inRange(folder, mission, difficulty)) return;
    load();
    kept = &records[folder][mission][difficulty];
    seconds = portHeadshotsRecordSeconds(seconds);
    fprintf(stderr, "ge: headshots: folder %d stage %d difficulty %d in %ds (kept %d)\n",
        folder, mission, difficulty, seconds, *kept);
    if (!portHeadshotsRecordBetter(*kept, seconds)) return;
    *kept = (s16)seconds;
    save();
}

void portHeadshotsFolderErased(s32 folder)
{
    if (folder < FOLDER1 || folder >= PORT_HEADSHOTS_FOLDERS) return;
    load();
    setFolder(folder, -1);
    save();
}

void portHeadshotsFolderCopied(s32 from, s32 to)
{
    if (from < FOLDER1 || from >= PORT_HEADSHOTS_FOLDERS || to < FOLDER1 || to >= PORT_HEADSHOTS_FOLDERS
        || from == to) return;
    load();
    setFolder(to, from);
    save();
}
