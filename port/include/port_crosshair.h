#ifndef PORT_CROSSHAIR_H
#define PORT_CROSSHAIR_H
#include <ultra64.h>

#define PORT_CROSSHAIR_SIZE_MIN 10
#define PORT_CROSSHAIR_SIZE_MAX 200
#define PORT_CROSSHAIR_SIZE_DEFAULT 100
#define PORT_CROSSHAIR_SIZE_STEP 5

s32 portCrosshairSize(void);
void portCrosshairSizeSet(s32 percent);
f32 portCrosshairSizeScale(s32 highHud);
s32 portCrosshairSizeFromFraction(f32 fraction);
s32 portCrosshairSizeOption(s32 option);
Gfx *portCrosshairSliderDraw(Gfx *g, f32 x, f32 y, f32 width, f32 previewX,
                             s32 rgb, s32 highHud);
/* Bounded pointer dragging shares the same value grid as pad/key input. */
s32 portCrosshairSliderPick(s32 option, f32 mx, f32 my, f32 x, f32 y, f32 width,
                            s32 held);
#endif
