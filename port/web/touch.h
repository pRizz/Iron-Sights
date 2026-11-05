#ifndef MIDAS_WEB_TOUCH_H
#define MIDAS_WEB_TOUCH_H
#include <ultra64.h>
struct MoveData;
enum WebTouchContext {
    WEB_TOUCH_NAVIGATION = 0,
    WEB_TOUCH_CINEMATIC = 1,
    WEB_TOUCH_GAME = 2,
    WEB_TOUCH_WATCH = 3,
    WEB_TOUCH_VALUE_EDIT = 4, /* an Options folder slider is open for editing */
    WEB_TOUCH_PICK_LIST = 5,  /* an Options folder row's pick list is up */
    WEB_TOUCH_BIND_CAPTURE = 6, /* an Options folder bind row waits for an input */
    WEB_TOUCH_ASSIGN = 7,     /* Input Device waits for a press on the device */
    WEB_TOUCH_HOLD_DRAG = 8,  /* a menu set by holding A and moving the cursor (007 mode) */
    WEB_TOUCH_PAD = 9,        /* a menu only a pad drives (multiplayer Player Options, Health) */
    WEB_TOUCH_TEXT = 10       /* a name keyboard is open (Save Layout / Set / Settings As) */
};
s32 portWebTouchContext(void);
s32 portWebTouchHasGadgets(void);
/* Browser-local virtual controls, player one only. No saved bind IDs change. */
void portWebTouchReset(void);
void portWebTouchPoll(OSContPad *pad);
void portWebTouchApplyMove(struct MoveData *move);
#endif
