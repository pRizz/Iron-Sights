/* Exercise packed campaign times through the actual inherited save codec. */
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
extern int strcmp(const char *, const char *);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-function-declaration"
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic ignored "-Woverflow"
#include "../../src/game/file2.c"
#pragma GCC diagnostic pop
#include "../../port/src/port_mission_times.c"

s32 portUnlockAll(void) { return 1; }
void osSyncPrintf(const char *fmt, ...)
{
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args); abort();
}

static void testRecordedTimes(void)
{
    /* Arrange: distinct packed records for every mission and standard difficulty. */
    s32 mission, difficulty;
    char text[8], expected[8];
    for (mission = SP_LEVEL_DAM; mission < SP_LEVEL_MAX; mission++) {
        for (difficulty = DIFFICULTY_AGENT; difficulty <= DIFFICULTY_00; difficulty++) {
            const s32 seconds = 1 + mission * 47 + difficulty * 31;
            fileSetDifficultyStageTime(&saves[0], mission, difficulty, seconds);
            snprintf(expected, sizeof(expected), "%02d:%02d", seconds / 60, seconds % 60);
            /* Act / Assert: the selected mission/difficulty reads its own record. */
            assert(portMissionBestTimeText(mission, FOLDER1, difficulty, text, sizeof(text)));
            assert(strcmp(text, expected) == 0);
        }
    }
    fileSetDifficultyStageTime(&saves[1], SP_LEVEL_DAM, DIFFICULTY_AGENT, 59);
    assert(portMissionBestTimeText(SP_LEVEL_DAM, FOLDER2, DIFFICULTY_AGENT, text, sizeof(text)));
    assert(strcmp(text, "00:59") == 0);
    fileSetDifficultyStageTime(&saves[0], SP_LEVEL_DAM, DIFFICULTY_AGENT, 1022);
    assert(portMissionBestTimeText(SP_LEVEL_DAM, FOLDER1, DIFFICULTY_AGENT, text, sizeof(text)));
    assert(strcmp(text, "17:02") == 0);
}

static void testUnavailableTimes(void)
{
    /* Arrange: one empty campaign and one completion/overflow sentinel. */
    char text[8];
    s32 mission, difficulty;
    fileSetDifficultyStageTime(&saves[0], SP_LEVEL_DAM, DIFFICULTY_SECRET, 1023);
    /* Act / Assert: Unlock All and 007 never manufacture a measured time. */
    assert(!portMissionBestTimeText(SP_LEVEL_DAM, FOLDER1, DIFFICULTY_SECRET, text, sizeof(text)));
    assert(strcmp(text, "--:--") == 0);
    for (mission = SP_LEVEL_DAM; mission < SP_LEVEL_MAX; mission++)
        for (difficulty = DIFFICULTY_AGENT; difficulty <= DIFFICULTY_007; difficulty++) {
            assert(!portMissionBestTimeText(mission, FOLDER3, difficulty, text, sizeof(text)));
            assert(strcmp(text, "--:--") == 0);
        }
    assert(!portMissionBestTimeText(SP_LEVEL_DAM, FOLDER1, DIFFICULTY_007, text, sizeof(text)));
    assert(!portMissionBestTimeText(SP_LEVEL_DAM, FOLDER4, DIFFICULTY_AGENT, text, sizeof(text)));
    assert(!portMissionBestTimeText(-1, FOLDER1, DIFFICULTY_AGENT, text, sizeof(text)));
    assert(!portMissionBestTimeText(SP_LEVEL_MAX, FOLDER1, DIFFICULTY_AGENT, text, sizeof(text)));
    assert(!portMissionBestTimeText(SP_LEVEL_DAM, FOLDER_INVALID, DIFFICULTY_AGENT, text, sizeof(text)));
    assert(!portMissionBestTimeText(SP_LEVEL_DAM, MAX_FOLDER_COUNT, DIFFICULTY_AGENT, text, sizeof(text)));
    assert(!portMissionBestTimeText(SP_LEVEL_DAM, FOLDER1, -1, text, sizeof(text)));
    assert(strcmp(text, "--:--") == 0);
}

static void testBoundedOutput(void)
{
    /* Arrange */
    char text[8] = "!";
    /* Act / Assert */
    assert(!portMissionBestTimeText(SP_LEVEL_DAM, FOLDER1, DIFFICULTY_AGENT, text, 0));
    assert(text[0] == '!');
    assert(portMissionBestTimeText(SP_LEVEL_DAM, FOLDER1, DIFFICULTY_AGENT, text, 1));
    assert(text[0] == '\0');
    assert(portMissionBestTimeText(SP_LEVEL_DAM, FOLDER1, DIFFICULTY_AGENT, text, 4));
    assert(strcmp(text, "17:") == 0);
}

int main(void)
{
    s32 slot;
    for (slot = 0; slot < SAVESLOTMAX; slot++) saves[slot].completion_bitflags = SAVEFLAG_DORESET;
    saves[0].completion_bitflags = FOLDER1;
    saves[1].completion_bitflags = FOLDER2;
    saves[2].completion_bitflags = FOLDER3;
    testRecordedTimes();
    testUnavailableTimes();
    testBoundedOutput();
    puts("Mission best-time checks passed: packed records, folders, sentinel, 007, Unlock All and bounded output.");
    return 0;
}
