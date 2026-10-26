#ifndef PORT_MISSION_CLOCK_H
#define PORT_MISSION_CLOCK_H
#include <ultra64.h>

typedef struct {
    s32 x, elapsedY, bestY, bottom, toastY;
} PortMissionClockLayout;

/* Native viewport units; shrinking the legacy HUD only increases clearance. */
PortMissionClockLayout portMissionClockLayout(s32 left, s32 top, s32 lineHeight, s32 hasBest);
Gfx *portHudDrawMissionTimer(Gfx *gdl);
s32 portHudObjectiveTop(void);
#endif
