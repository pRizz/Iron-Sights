/* Mission-selection hints use the same target table as the unlock check. */
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "game/front.h"
#include "game/file2.h"
#include "port_mission_cheats.h"

extern save_data *fileGetSaveForFoldernum(u32 folder);
extern s32 portUnlockAll(void);

s32 portMissionCheatInfo(s32 mission, s32 folder, PortMissionCheatInfo *info)
{
    s32 difficulty;
    info->difficulty = -1;
    info->seconds = info->earned = info->available = 0;
    if (mission < 0 || mission >= 20 || folder < FOLDER1 || folder >= MAX_FOLDER_COUNT) return 0;
    for (difficulty = DIFFICULTY_AGENT; difficulty <= DIFFICULTY_00; difficulty++) {
        if (solo_target_time_array[mission][difficulty] > 0) {
            save_data *save = fileGetSaveForFoldernum(folder);
            info->difficulty = difficulty;
            info->seconds = solo_target_time_array[mission][difficulty];
            info->earned = save && portFileGetIsCheatUnlockedRaw(save, mission);
#ifdef GE_UNLOCK_ALL
            info->available = 1;
#else
            info->available = info->earned || portUnlockAll();
#endif
            return 1;
        }
    }
    return 0;
}

const char *portMissionCheatStatus(const PortMissionCheatInfo *info)
{
    return info->earned ? "CHEAT UNLOCKED" : info->available ? "CHEAT AVAILABLE - UNLOCK ALL" : "CHEAT TO UNLOCK";
}

void portMissionCheatRequirement(const PortMissionCheatInfo *info, s32 highestDifficulty, char *text, s32 size)
{
    static const char *const names[] = {"AGENT", "SECRET AGENT", "00 AGENT"};
    if (size <= 0) return;
    text[0] = '\0';
    if (info->difficulty < DIFFICULTY_AGENT || info->difficulty > DIFFICULTY_00 || info->seconds <= 0) return;
    snprintf(text, size, "%s  %02d:%02d%s", names[info->difficulty], info->seconds/60, info->seconds%60,
        highestDifficulty < info->difficulty ? " - DIFFICULTY LOCKED" : "");
}

u32 portMissionCheatColor(const PortMissionCheatInfo *info)
{
    return info->earned ? 0xb9ed9fff : info->available ? 0xa7d8ffff : 0xf0d994ff;
}
