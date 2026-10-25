#ifndef MIDAS_WEB_TOUCH_H
#define MIDAS_WEB_TOUCH_H
#include <ultra64.h>
struct MoveData;
/* Browser-local virtual controls, player one only. No saved bind IDs change. */
void portWebTouchReset(void);
void portWebTouchPoll(OSContPad *pad);
void portWebTouchApplyMove(struct MoveData *move);
#endif
