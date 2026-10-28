/* Mission-selection hints use the same target table as the unlock check. */
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "game/front.h"
#include "game/file2.h"
#include "game/cheat.h"
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

void portMissionCheatSummary(s32 mission, const PortMissionCheatInfo *info, char *text, s32 size)
{
    /* Same mission order as the inherited target table and earned save bits. */
    static const CHEAT_ID cheats[] = {
        CHEAT_PAINTBALL, CHEAT_INVINCIBILITY, CHEAT_DK_MODE, CHEAT_2X_GRENADE_LAUNCHER,
        CHEAT_2X_ROCKET_LAUNCHER, CHEAT_TURBO_MODE, CHEAT_NO_RADAR_MP, CHEAT_TINY_BOND,
        CHEAT_2X_THROWING_KNIFE, CHEAT_FAST_ANIMATION, CHEAT_INVISIBILITY, CHEAT_ENEMY_ROCKETS,
        CHEAT_SLOW_ANIMATION, CHEAT_SILVER_PP7, CHEAT_2X_HUNTING_KNIFE, CHEAT_INFINITE_AMMO,
        CHEAT_2X_RCP90, CHEAT_GOLD_PP7, CHEAT_2X_LASER, CHEAT_ALLGUNS
    };
    const char *name;
    s32 length;
    if (size <= 0) return;
    if (!info->earned || mission < SP_LEVEL_DAM || mission > SP_LEVEL_EGYPT) {
        snprintf(text, size, "%s", portMissionCheatStatus(info));
        return;
    }
    name = cheatGetMenuTextPointer(cheats[mission]);
    for (length = 0; name[length] && name[length] != '\n' && name[length] != '\r'; length++) {}
    snprintf(text, size, "%s - %.*s", portMissionCheatStatus(info), length, name);
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
