#ifndef MIDAS_WEB_TOUCH_H
#define MIDAS_WEB_TOUCH_H
#include <ultra64.h>
struct MoveData;
enum WebTouchContext {
    WEB_TOUCH_NAVIGATION = 0,
    WEB_TOUCH_CINEMATIC = 1,
    WEB_TOUCH_GAME = 2,
    WEB_TOUCH_WATCH = 3
};
s32 portWebTouchContext(void);
s32 portWebTouchHasGadgets(void);
/* Browser-local virtual controls, player one only. No saved bind IDs change. */
void portWebTouchReset(void);
void portWebTouchPoll(OSContPad *pad);
void portWebTouchApplyMove(struct MoveData *move);
#endif
