/* Included by test_mission_times.c: Headshots Only (FORK-083) records, with
 * the inherited save codec. Completion, best-time replacement, the stock
 * completion gate, folder erase and copy, the Best Time column's text and the
 * Data/Saves/headshots.ini round trip. The file2.c hooks that call erase and
 * copy run in game (the save writes they sit beside need the eeprom). */
#include <unistd.h>
#include "../../port/src/port_headshots_only.c"
#include "../../port/src/port_headshots_records.c"

static char sHeadshotsRoot[256];

const char *portRootPath(const char *name)
{
    static char path[512];
    snprintf(path, sizeof(path), "%s%s", sHeadshotsRoot, name);
    return path;
}

/* The packed times themselves: the stock setter stores 0 as 79 seconds. */
static void headshotsClearSave(save_data *save)
{
    u32 i;
    for (i = 0; i < sizeof(save->times); i++) save->times[i] = 0;
}

static void headshotsReload(void)
{
    loaded = FALSE;
}

static void testHeadshotsRecords(void)
{
    char text[8];
    char dir[300];
    FILE *f;

    snprintf(sHeadshotsRoot, sizeof(sHeadshotsRoot), "/tmp/midas-headshots-%d/", (int)getpid());
    snprintf(dir, sizeof(dir), "mkdir -p %sData/Saves && rm -f %sData/Saves/headshots.ini", sHeadshotsRoot, sHeadshotsRoot);
    assert(system(dir) == 0);
    headshotsReload();
    headshotsClearSave(&saves[0]);
    headshotsClearSave(&saves[1]);

    /* Not a Headshots Only run: nothing is recorded. */
    portHeadshotsOnlyOptionSet("headshots_only", 0);
    portHeadshotsOnlyBeginStage(TRUE, FALSE);
    portHeadshotsMissionComplete(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT, 95);
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 0);

    /* A run: recorded; only a faster run replaces it. */
    portHeadshotsOnlyOptionSet("headshots_only", 1);
    portHeadshotsOnlyBeginStage(TRUE, FALSE);
    portHeadshotsMissionComplete(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT, 95);
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 95);
    portHeadshotsMissionComplete(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT, 120);
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 95);
    portHeadshotsMissionComplete(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT, 80);
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 80);
    /* Untimed (over 1023 s) still counts as a completion. */
    portHeadshotsMissionComplete(FOLDER1, SP_LEVEL_FACILITY, DIFFICULTY_00, 5000);
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_FACILITY, DIFFICULTY_00) == PORT_HEADSHOTS_UNTIMED);
    /* 007 and out-of-range values are ignored. */
    portHeadshotsMissionComplete(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_007, 60);
    portHeadshotsMissionComplete(MAX_FOLDER_COUNT, SP_LEVEL_DAM, DIFFICULTY_AGENT, 60);
    portHeadshotsMissionComplete(FOLDER1, SP_LEVEL_MAX, DIFFICULTY_AGENT, 60);
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_007) == 0);
    assert(portHeadshotsRecordGet(MAX_FOLDER_COUNT, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 0);

    /* Shown only beside the save's own completion. */
    assert(!portHeadshotsRecordShown(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT));
    assert(!portHeadshotsBestTimeText(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT, text, sizeof(text)));
    assert(strcmp(text, "--:--") == 0);
    fileSetDifficultyStageTime(&saves[0], SP_LEVEL_DAM, DIFFICULTY_AGENT, 70);
    fileSetDifficultyStageTime(&saves[0], SP_LEVEL_FACILITY, DIFFICULTY_00, 1023);
    assert(portHeadshotsRecordShown(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT));
    assert(portHeadshotsBestTimeText(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT, text, sizeof(text)));
    assert(strcmp(text, "01:20") == 0);
    /* A completion slower than the save's 17:03 cap reads "17:03+". */
    assert(portHeadshotsRecordShown(FOLDER1, SP_LEVEL_FACILITY, DIFFICULTY_00));
    assert(portHeadshotsBestTimeText(FOLDER1, SP_LEVEL_FACILITY, DIFFICULTY_00, text, sizeof(text)));
    assert(strcmp(text, "17:03+") == 0);

    /* The file round trip, ignoring lines it does not know. */
    f = fopen(portRootPath("Data/Saves/headshots.ini"), "a");
    assert(f != NULL);
    fputs("f9_m00_d0=10\nf1_m00_d3=10\nf1_m01_d0=0\nf1_m02_d1=2000\njunk\n", f);
    fclose(f);
    headshotsReload();
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 80);
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_FACILITY, DIFFICULTY_00) == PORT_HEADSHOTS_UNTIMED);
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_DAM + 1, DIFFICULTY_AGENT) == 0);
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_DAM + 2, DIFFICULTY_SECRET) == 0);

    /* Copying a folder copies its records; erasing clears them, on disk too. */
    portHeadshotsFolderCopied(FOLDER1, FOLDER2);
    assert(portHeadshotsRecordGet(FOLDER2, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 80);
    portHeadshotsFolderErased(FOLDER1);
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 0);
    headshotsReload();
    assert(portHeadshotsRecordGet(FOLDER1, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 0);
    assert(portHeadshotsRecordGet(FOLDER2, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 80);


    headshotsClearSave(&saves[0]);
    headshotsClearSave(&saves[1]);
    portHeadshotsOnlyOptionSet("headshots_only", 0);
    portHeadshotsOnlyBeginStage(TRUE, FALSE);
    snprintf(dir, sizeof(dir), "rm -rf %s", sHeadshotsRoot);
    assert(system(dir) == 0);
}
