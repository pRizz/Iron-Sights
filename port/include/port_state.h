#ifndef PORT_STATE_H
#define PORT_STATE_H
#include <ultra64.h>
#include <platform_info.h>
/* Suspend images record 32-bit addresses (save_states.md), so 64-bit builds
 * link port_state_unavailable.c. The browser build keeps 1 and its own notice. */
#if IS_64_BIT
#define PORT_STATE_AVAILABLE 0
#else
#define PORT_STATE_AVAILABLE 1
#endif
enum { PORT_STATE_MANUAL = 4, PORT_STATE_QUICK = 4, PORT_STATE_UNDO = 5, PORT_STATE_SLOTS = 6 };
typedef struct {
    u32 present, compatible, stage, difficulty, timestamp, seconds;
    f32 health, armour;
} PortStateInfo;
/* Commands are queued; world memory is accessed only at the boss frame boundary. */
void portStateRequest(s32 slot, s32 load);
void portStateFrame(void);
void portStateStageReady(void);
s32 portStateInfo(s32 slot, PortStateInfo *info);
const char *portStateMessage(void);
void portStateMenuOpen(void);
s32 portStatePaused(void);
s32 portStateMenuInput(s32 key);
Gfx *portStateRender(Gfx *gdl);
/* The watch's Suspend Points row. Inline, and called by the watch page only
 * where PORT_STATE_AVAILABLE is 0, so a 32-bit program gains no code. */
static inline const char *portStateRowLabel(void)
{
    return PORT_STATE_AVAILABLE ? "Suspend Points\n" : "Suspend Points: unavailable\n";
}
#endif
