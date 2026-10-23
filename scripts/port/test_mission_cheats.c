/* Exercise the real save-bit reader with both runtime and compile-time Unlock All. */
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
extern int strcmp(const char *, const char *);
/* The inherited save module has legacy pointer/signature warnings outside
 * the save-bit functions exercised here. Keep new code under strict checks. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-function-declaration"
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic ignored "-Woverflow"
#include "../../src/game/file2.c"
#pragma GCC diagnostic pop
#include "../../port/src/port_mission_cheats.c"
static s32 unlockAll;
s32 portUnlockAll(void) { return unlockAll; }
s16 solo_target_time_array[20][3] = {{0,160,0}, {0,0,125}, {300,0,0}};
void osSyncPrintf(const char *fmt, ...)
{
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args); abort();
}
int main(void)
{
    PortMissionCheatInfo info;
    char text[96];
    s32 i;
    for (i = 0; i < SAVESLOTMAX; i++) saves[i].completion_bitflags = SAVEFLAG_DORESET;
    saves[0].completion_bitflags = FOLDER1;
    saves[1].completion_bitflags = FOLDER2;
    assert(portMissionCheatInfo(0, FOLDER1, &info) && info.difficulty == DIFFICULTY_SECRET && info.seconds == 160 && !info.earned);
    portMissionCheatRequirement(&info, DIFFICULTY_AGENT, text, sizeof(text));
    assert(strcmp(text, "SECRET AGENT  02:40 - DIFFICULTY LOCKED") == 0);
    portMissionCheatRequirement(&info, DIFFICULTY_007, text, sizeof(text));
    assert(strcmp(text, "SECRET AGENT  02:40") == 0);
#ifndef GE_UNLOCK_ALL
    assert(strcmp(portMissionCheatStatus(&info), "CHEAT TO UNLOCK") == 0 && portMissionCheatColor(&info) == 0xf0d994ff);
#endif
    unlockAll = 1;
    assert(portMissionCheatInfo(0, FOLDER1, &info) && info.available && !info.earned);
    assert(strcmp(portMissionCheatStatus(&info), "CHEAT AVAILABLE - UNLOCK ALL") == 0 && portMissionCheatColor(&info) == 0xa7d8ffff);
    assert(fileGetIsCheatUnlocked(&saves[0], 0));
    assert(!portFileGetIsCheatUnlockedRaw(&saves[0], 0));
    fileSetSaveCheatUnlocked(&saves[0], 0);
    assert(portMissionCheatInfo(0, FOLDER1, &info) && info.earned && info.available);
    assert(strcmp(portMissionCheatStatus(&info), "CHEAT UNLOCKED") == 0 && portMissionCheatColor(&info) == 0xb9ed9fff);
    assert(portMissionCheatInfo(0, FOLDER2, &info) && !info.earned); /* folder isolation */
    assert(portMissionCheatInfo(0, FOLDER3, &info) && !info.earned); /* absent save */
    unlockAll = 0;
    assert(portMissionCheatInfo(1, FOLDER1, &info) && info.difficulty == DIFFICULTY_00 && info.seconds == 125 && !info.earned);
    assert(portMissionCheatInfo(2, FOLDER1, &info) && info.difficulty == DIFFICULTY_AGENT && info.seconds == 300);
    assert(!portMissionCheatInfo(-1, FOLDER1, &info));
    assert(!portMissionCheatInfo(20, FOLDER1, &info));
    assert(!portMissionCheatInfo(3, FOLDER1, &info)); /* no target */
    assert(!portMissionCheatInfo(0, FOLDER_INVALID, &info));
    assert(!portMissionCheatInfo(0, MAX_FOLDER_COUNT, &info));
    assert(info.difficulty == -1 && info.seconds == 0);
    portMissionCheatRequirement(&info, DIFFICULTY_007, text, sizeof(text));
    assert(!text[0]);
    for (i = 0; i < 20; i++) {
        fileSetSaveCheatUnlocked(&saves[1], i);
        assert(portFileGetIsCheatUnlockedRaw(&saves[1], i));
    }
    puts("Mission cheat status checks passed: target difficulty/time, earned bits, Unlock All, folders and invalid entries.");
    return 0;
}
