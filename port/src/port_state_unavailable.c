/* Suspend images record 32-bit addresses and the exact executable layout
 * (port_state.c), so 64-bit builds link this stand-in instead, as the browser
 * build links port/web/state.c. Campaign saves are separate and unaffected.
 * The watch row says so up front (portStateRowLabel, port_watchpages.c). */
#include <stdio.h>
#include <string.h>
#include "port_state.h"
extern void portConPrintf(s32 show, const char *fmt, ...);
extern void portStateInput(void);

const char *portStateMessage(void) { return "Suspend points are unavailable in 64-bit builds. Campaign saves work as usual."; }
void portStateRequest(s32 slot, s32 load)
{
    (void)slot; (void)load;
    fprintf(stderr, "ge: suspend: %s\n", portStateMessage());   /* the log line port_state.c's notices have */
    portConPrintf(1, "%s", portStateMessage());
}
void portStateMenuOpen(void) { portStateRequest(0, 0); }
/* As port_state.c's frame: joy_port.c takes the four suspend keys (each prints the
 * message here) and runs the Graphics hotkeys and Baked Lighting's slice (FORK-082). */
void portStateFrame(void) { portStateInput(); }
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
