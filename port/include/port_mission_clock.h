#ifndef PORT_MISSION_CLOCK_H
#define PORT_MISSION_CLOCK_H
#include <ultra64.h>

typedef struct {
    s32 x, elapsedY, bestY, bottom, toastY;
} PortMissionClockLayout;

/* Native viewport units; shrinking the legacy HUD only increases clearance. */
PortMissionClockLayout portMissionClockLayout(s32 left, s32 top, s32 lineHeight, s32 hasBest);

/* The top-left text corner. The show_fps counter, the mission clock,
 * subtitles, objective alerts, the debug readouts and console notices share
 * one layout, so none draws over another. Boxes are in screen units (after
 * HUD Scale); a hidden element's box is all zero. `safeX/safeY` is the
 * browser's safe-area inset in HUD units (port/web/lifecycle.c), 0 natively. */
typedef struct {
    s32 x, y, right, bottom;
} PortHudBox;
/* The FPS counter's text origin (inside its one-unit outline) and outer edges. */
PortHudBox portHudFpsLayout(s32 left, s32 top, s32 safeX, s32 safeY, s32 textWidth, s32 textHeight);
/* Layout-unit x offset for the clock, so its left edge clears the safe area
 * and, after HUD Scale (`scale` 1 or 2), the FPS counter's right edge. */
s32 portHudClockOffsetX(s32 left, s32 fpsRight, s32 safeX, s32 scale);
/* Keep the native subtitle bar below the complete clock block, when enabled,
 * and below `screenBottom` (the FPS counter where it shares the text's columns). */
s32 portHudSubtitleLayoutTop(s32 viewTop, s32 textY, s32 clockBottom, s32 smallHud, s32 screenBottom);
s32 portHudSubtitleTop(s32 textX, s32 textY);
/* Actual subtitle bar bounds, after native HUD scaling; reset each render. */
void portHudSubtitleBottom(s32 bottom);
/* The objective alert card's bottom edge while it shows, else 0; reset each render. */
void portHudObjectiveCardBottom(s32 bottom);
s32 portHudObjectiveLayoutTop(s32 viewTop, s32 clockToastY, s32 subtitleBottom, s32 screenBottom);
/* The debug readout rows (testing-man position, show_input) start at `base`
 * unless the corner's FPS counter, clock or NOCLIP label reach below it. */
s32 portHudDebugLayoutTop(s32 base, s32 fpsBottom, s32 clockBottom, s32 labelBottom);
s32 portHudDebugRowsTop(void);
Gfx *portHudDrawMissionTimer(Gfx *gdl);
s32 portHudObjectiveTop(void);
/* show_fps: draws the counter (player one only) and records how far down the
 * corner's text reaches this frame, for console notices. */
Gfx *portHudDrawFpsCounter(Gfx *gdl);
/* The lowest edge of this frame's top-left text, in screen units; 0 once read
 * or when no gameplay HUD drew. The console's notices start below it. */
s32 portHudTakeTopReserved(void);
/* The safe-area inset in HUD units for the current frame (0 natively). */
void portHudSafeArea(s32 *dx, s32 *dy);
#endif
