#ifndef PORT_STATE_H
#define PORT_STATE_H
#include <ultra64.h>
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
#endif
