/* Suspend images record 32-bit addresses and the exact executable layout
 * (port_state.c), so 64-bit builds link this stand-in instead, as the browser
 * build links port/web/state.c. Campaign saves are separate and unaffected.
 * The menus' and manual's notice is LP64-007's (scripts/port/native_64bit_plan.md). */
#include <string.h>
#include "port_state.h"
#include "port_graphics.h"
extern void portConPrintf(s32 show, const char *fmt, ...);

const char *portStateMessage(void) { return "Suspend points are unavailable in 64-bit builds."; }
void portStateRequest(s32 slot, s32 load)
{
    (void)slot; (void)load;
    portConPrintf(1, "%s", portStateMessage());
}
void portStateMenuOpen(void) { portStateRequest(0, 0); }
/* FORK-082: the Graphics hotkeys and Baked Lighting's slice run here natively too */
void portStateFrame(void) { portGraphicsFrame(); }
void portStateStageReady(void) {}
void portStatePresentationReset(void) {}
s32 portStatePaused(void) { return 0; }
s32 portStateMenuInput(s32 key) { (void)key; return 0; }
Gfx *portStateRender(Gfx *gdl) { return gdl; }
s32 portStateInfo(s32 slot, PortStateInfo *info)
{
    (void)slot;
    memset(info, 0, sizeof(*info));
    return 0;
}
