#ifndef PORT_OBJECTIVE_MESSAGES_H
#define PORT_OBJECTIVE_MESSAGES_H
#include <bondtypes.h>

/* Stage-script top-of-screen messages that only restate an objective result
 * yield to the matching Objective Alert (FORK-008). */

/* The objective a scripted message restates, or -1; *status gets the result. */
s32 portObjectiveMessageFind(s32 stage, s32 textId, OBJECTIVESTATUS *status);

/* AI_TextPrintTop: true when the message waits for the next objective pass
 * instead of showing now. The caller then skips hudmsgTopShow. */
s32 portObjectiveMessageHold(s32 textId, char *text);

/* End of the objective pass: drop held messages whose objective result has an
 * alert on screen or queued, and show the others as the game would have. */
void portObjectiveMessagesRelease(void);

/* Mission start: forget held messages from the previous stage. */
void portObjectiveMessagesReset(void);
#endif
