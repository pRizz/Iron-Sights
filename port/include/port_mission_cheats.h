#ifndef PORT_MISSION_CHEATS_H
#define PORT_MISSION_CHEATS_H
#include <ultra64.h>
typedef struct {
    s32 difficulty, seconds, earned, available;
} PortMissionCheatInfo;
s32 portMissionCheatInfo(s32 mission, s32 folder, PortMissionCheatInfo *info);
const char *portMissionCheatStatus(const PortMissionCheatInfo *info);
void portMissionCheatSummary(s32 mission, const PortMissionCheatInfo *info, char *text, s32 size);
void portMissionCheatRequirement(const PortMissionCheatInfo *info, s32 highestDifficulty, char *text, s32 size);
u32 portMissionCheatColor(const PortMissionCheatInfo *info);
#endif
