#ifndef PORT_MISSION_TIMES_H
#define PORT_MISSION_TIMES_H
#include <ultra64.h>

/* Returns true only for a measured record in the selected campaign. */
s32 portMissionBestTimeText(s32 mission, s32 folder, s32 difficulty, char *text, s32 size);
Gfx *portMissionBestTimesDraw(Gfx *dl, s32 mission, s32 folder, s32 highestDifficulty);
#endif
