#ifndef PORT_MISSION_CLOCK_H
#define PORT_MISSION_CLOCK_H
#include <ultra64.h>

typedef struct {
    s32 x, elapsedY, bestY, bottom, toastY;
} PortMissionClockLayout;

/* Native viewport units; shrinking the legacy HUD only increases clearance. */
PortMissionClockLayout portMissionClockLayout(s32 left, s32 top, s32 lineHeight, s32 hasBest);
/* Keep the native subtitle bar below the complete clock block, when enabled. */
s32 portHudSubtitleLayoutTop(s32 viewTop, s32 textY, s32 clockBottom, s32 smallHud);
s32 portHudSubtitleTop(s32 textY);
/* Actual subtitle bar bounds, after native HUD scaling; reset each render. */
void portHudSubtitleBottom(s32 bottom);
s32 portHudObjectiveLayoutTop(s32 viewTop, s32 clockToastY, s32 subtitleBottom);
Gfx *portHudDrawMissionTimer(Gfx *gdl);
s32 portHudObjectiveTop(void);
#endif
