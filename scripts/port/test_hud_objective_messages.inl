/* Scripted top messages that repeat an objective alert (FORK-008), included by
 * test_hud.c after its HUD stubs. Text banks verify the audited text IDs. */
#include "../../assets/obseg/text/LdamE.c"
#include "../../assets/obseg/text/LsevxE.c"
#include "../../assets/obseg/text/LsevxbE.c"
#include "../../assets/obseg/text/LaztE.c"
#include "../../assets/obseg/text/LsevE.c"
#include "../../assets/obseg/text/LdestE.c"
#include "../../assets/obseg/text/LcradE.c"

static LEVELID testStage = LEVELID_DAM;
static char *topShown[4];
static s32 topShownCount;
LEVELID bossGetStageNum(void) { return testStage; }
void hudmsgTopShow(char *string) { assert(topShownCount < 4); topShown[topShownCount++] = string; }

static void testScriptedObjectiveEchoTable(void)
{
    /* Arrange: each suppressed text ID must still name the English echo it was
     * audited as; a shifted ID would hide an unrelated script message. */
    static const struct { s32 stage, bank, slot; char **strings; const char *text; s32 objective, status; } echoes[] = {
        { LEVELID_DAM, LDAM, 8, LdamE, "Covert modem installed.\n", 1, OBJECTIVESTATUS_COMPLETE },
        { LEVELID_DAM, LDAM, 10, LdamE, "Off-site data backup completed.\n", 2, OBJECTIVESTATUS_COMPLETE },
        { LEVELID_DAM, LDAM, 11, LdamE, "... backup failed.\n", 2, OBJECTIVESTATUS_FAILED },
        { LEVELID_SURFACE, LSEVX, 8, LsevxE, "Communications dish powered down.\n", 0, OBJECTIVESTATUS_COMPLETE },
        { LEVELID_SURFACE2, LSEVXB, 8, LsevxbE, "Communications link damaged.\n", 1, OBJECTIVESTATUS_COMPLETE },
        { LEVELID_SURFACE2, LSEVXB, 11, LsevxbE, "Bomb placed on Hound MI-4.\n", 2, OBJECTIVESTATUS_COMPLETE },
        { LEVELID_AZTEC, LAZT, 8, LaztE, "Q's guidance data has been uploaded.\n", 0, OBJECTIVESTATUS_COMPLETE },
        { LEVELID_BUNKER1, LSEV, 22, LsevE, "Finished download.\n", 3, OBJECTIVESTATUS_COMPLETE },
        { LEVELID_FRIGATE, LDEST, 11, LdestE, "Tracker bug successfully planted on Pirate\n", 3, OBJECTIVESTATUS_COMPLETE },
        { LEVELID_FRIGATE, LDEST, 13, LdestE, "Bridge bomb successfully defused.\n", 1, OBJECTIVESTATUS_COMPLETE },
        { LEVELID_FRIGATE, LDEST, 14, LdestE, "Engine room bomb successfully defused.\n", 2, OBJECTIVESTATUS_COMPLETE },
        { LEVELID_CRADLE, LCRAD, 8, LcradE, "Control console destroyed!\nAntenna inoperative.\n", 0, OBJECTIVESTATUS_COMPLETE },
    };
    OBJECTIVESTATUS status;
    u32 i;
    for (i = 0; i < sizeof(echoes) / sizeof(echoes[0]); i++) {
        assert(strcmp(echoes[i].strings[echoes[i].slot], echoes[i].text) == 0);
        status = -1;
        assert(portObjectiveMessageFind(echoes[i].stage, getStringID(echoes[i].bank, echoes[i].slot), &status)
            == echoes[i].objective);
        assert(status == echoes[i].status);
    }
    assert(sizeof(echoes) / sizeof(echoes[0]) == sizeof(pomMessages) / sizeof(pomMessages[0]));
    /* Failures that explain their cause keep the stock message. */
    assert(strcmp(LdamE[33], "Covert modem incorrectly installed.\n") == 0);
    assert(portObjectiveMessageFind(LEVELID_DAM, getStringID(LDAM, 33), NULL) == -1);
    assert(portObjectiveMessageFind(LEVELID_DAM, getStringID(LDAM, 14), NULL) == -1);
    assert(portObjectiveMessageFind(LEVELID_SURFACE, getStringID(LSEVX, 9), NULL) == -1);
    assert(portObjectiveMessageFind(LEVELID_SURFACE2, getStringID(LSEVXB, 12), NULL) == -1);
    assert(portObjectiveMessageFind(LEVELID_BUNKER1, getStringID(LSEV, 46), NULL) == -1);
    /* A text ID only counts on its own stage. */
    assert(portObjectiveMessageFind(LEVELID_SURFACE, getStringID(LDAM, 8), NULL) == -1);
}

static void testScriptedObjectiveEchoYields(void)
{
    char modem[] = "Covert modem installed.", backup[] = "... backup failed.", other[] = "Security gate A";
    portHudReset(0);
    portObjectiveMessagesReset();
    testStage = LEVELID_DAM;
    topShownCount = 0;

    /* The script prints before the flag lands; the objective pass then queues
     * the alert, and the echo is dropped in its favour. */
    assert(portObjectiveMessageHold(getStringID(LDAM, 8), modem));
    portHudObjective(1, 1, OBJECTIVESTATUS_COMPLETE, "install covert modem");
    portObjectiveMessagesRelease();
    assert(topShownCount == 0);

    /* An alert already on screen for the result also covers a later echo. */
    assert(portObjectiveMessageHold(getStringID(LDAM, 8), modem));
    portObjectiveMessagesRelease();
    assert(topShownCount == 0);

    /* No alert for that result (difficulty-filtered objective, or a different
     * outcome) shows the stock message once, one pass later. */
    portHudReset(0);
    assert(portObjectiveMessageHold(getStringID(LDAM, 8), modem));
    portObjectiveMessagesRelease();
    assert(topShownCount == 1 && topShown[0] == modem);
    portObjectiveMessagesRelease();
    assert(topShownCount == 1);
    portHudObjective(2, 2, OBJECTIVESTATUS_COMPLETE, "intercept data backup");
    assert(portObjectiveMessageHold(getStringID(LDAM, 11), backup));
    portObjectiveMessagesRelease();
    assert(topShownCount == 2 && topShown[1] == backup);

    /* Unlisted messages, disabled alerts and multiplayer never wait. */
    assert(!portObjectiveMessageHold(getStringID(LDAM, 15), other));
    assert(!portObjectiveMessageHold(getStringID(LDAM, 8), NULL));
    portHudOptionSet("objective_notifications", 0);
    assert(!portObjectiveMessageHold(getStringID(LDAM, 8), modem));
    portHudOptionSet("objective_notifications", 1);
    playerCount = 2;
    assert(!portObjectiveMessageHold(getStringID(LDAM, 8), modem));
    playerCount = 1;

    /* The held pair mirrors the native two-message top window; mission
     * reset forgets messages from the previous stage. */
    assert(portObjectiveMessageHold(getStringID(LDAM, 8), modem));
    assert(portObjectiveMessageHold(getStringID(LDAM, 11), backup));
    assert(!portObjectiveMessageHold(getStringID(LDAM, 10), other));
    portObjectiveMessagesReset();
    portObjectiveMessagesRelease();
    assert(topShownCount == 2);
    portHudReset(0);
}

