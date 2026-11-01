#ifndef PORT_OBJECTIVE_PROGRESS_H
#define PORT_OBJECTIVE_PROGRESS_H
#include <bondtypes.h>

/* True when all active prerequisites are complete and no objective has failed. */
s32 portMainObjectivesComplete(s32 stage, s32 difficulty, s32 count,
    struct objective_entry *const *objectives, const OBJECTIVESTATUS *statuses);
#endif
