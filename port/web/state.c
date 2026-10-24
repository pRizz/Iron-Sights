/* Native snapshots depend on PE/ELF layouts. Browser campaign saves are separate. */
#include <string.h>
#include "port_state.h"
extern void portConPrintf(s32 show, const char *fmt, ...);

const char *portStateMessage(void) { return "Suspend points are unavailable in the browser."; }
void portStateRequest(s32 slot, s32 load)
{
    (void)slot; (void)load;
    portConPrintf(1, "%s", portStateMessage());
}
void portStateMenuOpen(void) { portStateRequest(0, 0); }
void portStateFrame(void) {}
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
