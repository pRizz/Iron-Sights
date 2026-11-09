#ifndef PORT_STARTUP_H
#define PORT_STARTUP_H
#include <ultra64.h>

Gfx *portStartupAttribution(Gfx *gdl);
/* Dim build stamp under SELECT FILE; left/top are its art's lower-left corner. */
Gfx *portFileSelectBuildStamp(Gfx *gdl, s32 left, s32 top);

#endif
