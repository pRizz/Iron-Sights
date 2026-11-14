#include <bondconstants.h>
#include "port_hud.h"
#include "port_objective_messages.h"

extern LEVELID bossGetStageNum(void);
extern s32 getPlayerCount(void);
extern void hudmsgTopShow(char *string);

typedef struct {
    s32 stage;
    u16 text;
    u8 objective;
    u8 status;
} ObjectiveMessage;

typedef struct {
    char *text;
    s32 objective;
    OBJECTIVESTATUS status;
} HeldMessage;

/* Stage-script top messages that add nothing to the objective alert they
 * coincide with. Failures that explain a cause (an incorrectly placed bomb, a
 * destroyed mainframe, a killed contact) stay off this list. */
static const ObjectiveMessage pomMessages[] = {
    { LEVELID_DAM, getStringID(LDAM, 8), 1, OBJECTIVESTATUS_COMPLETE },        /* Covert modem installed. */
    { LEVELID_DAM, getStringID(LDAM, 10), 2, OBJECTIVESTATUS_COMPLETE },       /* Off-site data backup completed. */
    { LEVELID_DAM, getStringID(LDAM, 11), 2, OBJECTIVESTATUS_FAILED },         /* ... backup failed. */
    { LEVELID_SURFACE, getStringID(LSEVX, 8), 0, OBJECTIVESTATUS_COMPLETE },   /* Communications dish powered down. */
    { LEVELID_SURFACE2, getStringID(LSEVXB, 8), 1, OBJECTIVESTATUS_COMPLETE }, /* Communications link damaged. */
    { LEVELID_SURFACE2, getStringID(LSEVXB, 11), 2, OBJECTIVESTATUS_COMPLETE },/* Bomb placed on Hound MI-4. */
    { LEVELID_AZTEC, getStringID(LAZT, 8), 0, OBJECTIVESTATUS_COMPLETE },      /* Q's guidance data has been uploaded. */
    { LEVELID_BUNKER1, getStringID(LSEV, 22), 3, OBJECTIVESTATUS_COMPLETE },   /* Finished download. */
    { LEVELID_FRIGATE, getStringID(LDEST, 11), 3, OBJECTIVESTATUS_COMPLETE },  /* Tracker bug successfully planted... */
    { LEVELID_FRIGATE, getStringID(LDEST, 13), 1, OBJECTIVESTATUS_COMPLETE },  /* Bridge bomb successfully defused. */
    { LEVELID_FRIGATE, getStringID(LDEST, 14), 2, OBJECTIVESTATUS_COMPLETE },  /* Engine room bomb successfully defused. */
    { LEVELID_CRADLE, getStringID(LCRAD, 8), 0, OBJECTIVESTATUS_COMPLETE },    /* Control console destroyed! ... */
};

/* The native top window holds two messages; a held pair matches that. */
#define POM_HELD 2
static HeldMessage pomHeld[POM_HELD];
static s32 pomHeldCount;

s32 portObjectiveMessageFind(s32 stage, s32 textId, OBJECTIVESTATUS *status)
{
    u32 i;
    for (i = 0; i < sizeof(pomMessages) / sizeof(pomMessages[0]); i++) {
        if (pomMessages[i].stage != stage || pomMessages[i].text != textId) continue;
        if (status) *status = pomMessages[i].status;
        return pomMessages[i].objective;
    }
    return -1;
}

s32 portObjectiveMessageHold(s32 textId, char *text)
{
    OBJECTIVESTATUS status;
    s32 alerts = 0;
    const s32 objective = portObjectiveMessageFind(bossGetStageNum(), textId, &status);
    /* Without alerts (or outside solo play, where the objective pass that
     * releases held messages does not run) the message shows at once. */
    if (objective < 0 || !text || pomHeldCount == POM_HELD) return 0;
    if (!portHudOptionGet("objective_notifications", &alerts) || !alerts) return 0;
    if (getPlayerCount() != 1) return 0;
    /* Scripts often print before setting the objective flag in the same AI
     * step, so the decision waits for the next objective pass. */
    pomHeld[pomHeldCount].text = text;
    pomHeld[pomHeldCount].objective = objective;
    pomHeld[pomHeldCount].status = status;
    pomHeldCount++;
    return 1;
}

void portObjectiveMessagesRelease(void)
{
    s32 i;
    for (i = 0; i < pomHeldCount; i++) {
        /* No alert for this result (difficulty filter, a different outcome,
         * alerts switched off) keeps the stock message. */
        if (!portHudObjectiveQueued(pomHeld[i].objective, pomHeld[i].status))
            hudmsgTopShow(pomHeld[i].text);
    }
    pomHeldCount = 0;
}

void portObjectiveMessagesReset(void)
{
    pomHeldCount = 0;
}
