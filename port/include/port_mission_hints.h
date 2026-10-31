#ifndef PORT_MISSION_HINTS_H
#define PORT_MISSION_HINTS_H
#include <ultra64.h>
s32 portMissionHintsOptionGet(const char *key, s32 *value);
s32 portMissionHintsOptionSet(const char *key, s32 value);

/* Stage starts arm the hints; transient HUD resets do not rearm them. */
void portMissionHintsReset(void);
void portMissionHintsDismiss(void);
void portMissionHintsTick(void);
Gfx *portMissionHintsDraw(Gfx *gdl);
#endif
