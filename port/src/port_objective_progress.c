#include <bondconstants.h>
#include "port_objective_progress.h"

/* The final briefing objective is an exit action or an ongoing casualty
 * safeguard on these stages. Objective order is shared by all difficulties. */
static s32 portFinalObjective(s32 stage)
{
    switch (stage) {
        case LEVELID_DAM: case LEVELID_RUNWAY: case LEVELID_SURFACE:
        case LEVELID_SURFACE2: case LEVELID_BUNKER2: case LEVELID_ARCHIVES:
        case LEVELID_STREETS: case LEVELID_DEPOT: case LEVELID_TRAIN:
        case LEVELID_JUNGLE: case LEVELID_CRADLE: case LEVELID_AZTEC:
        case LEVELID_EGYPT: case LEVELID_FACILITY: case LEVELID_SILO:
        case LEVELID_CAVERNS:
            return 1;
        default:
            return 0;
    }
}

s32 portMainObjectivesComplete(s32 stage, s32 difficulty, s32 count,
    struct objective_entry *const *objectives, const OBJECTIVESTATUS *statuses)
{
    s32 i, mainCount = 0;
    if (count < 1 || count > OBJECTIVES_MAX || !objectives || !statuses) return 0;
    for (i = 0; i < count; i++) {
        if (!objectives[i]) return 0;
        if (objectives[i]->difficulty > difficulty) continue;
        /* A failed safeguard also makes the mission impossible to complete. */
        if (statuses[i] == OBJECTIVESTATUS_FAILED) return 0;
        if ((portFinalObjective(stage) && i == count - 1)
            || (stage == LEVELID_STREETS && i == count - 2)) continue;
        mainCount++;
        if (statuses[i] != OBJECTIVESTATUS_COMPLETE) return 0;
    }
    return mainCount > 0;
}
