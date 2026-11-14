/* Sprint movement checks, included by test_ads.c: Walk / Sprint (FORK-006) and
 * Ludicrous Sprint, their settings rows and saved keys, Sprint (Toggle) and the
 * sprint feedback (indicator and FOV). */
#include "../../port/src/port_sprint_feedback.c"

static void testSprint(void)
{
    s32 setting;
    const s32 opt = portOptFind("sprint");
    assert(opt >= 0 && portOptAvailable(opt) && portOptIsToggle(opt));
    assert(portOptCategoryOf(opt) == PORTOPT_CAT_GAMEPLAY);
    assert(portJoyPortOptionGet("sprint", &setting) && setting == 1);
    assert(strcmp(portJoyPortBindDefault(PORTACT_WALK), "Left Shift") == 0);
    assert(portJoyPortMoveScale(0) == 1.0f);
    portJoyPortPadDown(0, PORTACT_WALK);
    portJoyPortPollCommit();
    assert(portJoyPortMoveScale(0) == 1.5f);
    assert(portJoyPortMoveScale(1) == 1.0f); /* native crouch slowdown still applies */
    g_CurrentPlayer->insightaimmode = 1;
    assert(portJoyPortMoveScale(0) == 1.0f);
    g_CurrentPlayer->insightaimmode = 0;
    normalAim(1, 1, 0);
    assert(portJoyPortMoveScale(0) == 0.5f); /* ADS wins over held sprint */
    assert(portJoyPortMoveScale(1) == 0.5f);
    normalAim(0, 0, 0);

    selectPlayer(1);
    assert(portJoyPortMoveScale(0) == 1.0f); /* player 1's key cannot sprint player 2 */
    portJoyPortPadDown(1, PORTACT_WALK);
    portJoyPortPollCommit();
    assert(portJoyPortMoveScale(0) == 1.5f);
    selectPlayer(0);
    assert(portJoyPortMoveScale(0) == 1.0f);
    portJoyPortPadDown(0, PORTACT_WALK);
    portJoyPortPollCommit();
    controlsLive = 0;
    assert(portJoyPortMoveScale(0) == 1.0f);
    controlsLive = 1;
    g_PlayerIsInTank = 1;
    assert(portJoyPortMoveScale(0) == 1.0f);
    g_PlayerIsInTank = 0;
    currentPlayer = -1;
    assert(portJoyPortMoveScale(0) == 1.0f);
    selectPlayer(0);

    assert(portJoyPortSetOption("sprint", "0")); /* saved ini uses the string bridge */
    assert(portJoyPortOptionGet("sprint", &setting) && setting == 0);
    assert(portJoyPortMoveScale(0) == 0.5f);
    assert(portJoyPortMoveScale(1) == 0.5f);
    normalAim(1, 1, 0);
    assert(portJoyPortMoveScale(0) == 0.5f); /* walk and ADS slowdowns do not stack */
    normalAim(0, 0, 0);
    portJoyPortPollCommit();
    assert(portJoyPortMoveScale(0) == 1.0f);
    assert(portJoyPortOptionSet("sprint", 1)); /* menus use the integer bridge */
    assert(portJoyPortOptionGet("sprint", &setting) && setting == 1);
    portOptStep(opt, 1);
    assert(portOptGet(opt) == 0 && savedSprint == 0);
    portOptStep(opt, -1);
    assert(portOptGet(opt) == 1 && savedSprint == 1);
}

static void testLudicrousSprint(void)
{
    s32 setting;
    const s32 opt = portOptFind("ludicrous_sprint");
    assert(opt >= 0 && portOptAvailable(opt) && portOptIsToggle(opt));
    assert(portOptCategoryOf(opt) == PORTOPT_CAT_GAMEPLAY);
    assert(portJoyPortOptionGet("ludicrous_sprint", &setting) && setting == 0);
    portJoyPortPadDown(0, PORTACT_WALK);
    portJoyPortPollCommit();
    assert(portJoyPortMoveScale(0) == 1.5f);
    assert(portJoyPortSetOption("ludicrous_sprint", "1"));
    assert(portJoyPortMoveScale(0) == 5.0f);
    assert(portJoyPortMoveScale(1) == 1.0f); /* crouching still prevents the boost */
    normalAim(1, 1, 0);
    assert(portJoyPortMoveScale(0) == 0.5f); /* ADS still wins */
    normalAim(0, 0, 0);
    assert(portJoyPortOptionSet("sprint", 0));
    assert(portJoyPortMoveScale(0) == 0.5f); /* no effect while Sprint is off */
    assert(portJoyPortOptionSet("sprint", 1));
    portJoyPortPollCommit();
    assert(portJoyPortMoveScale(0) == 1.0f); /* only while the bind is held */
    portOptStep(opt, 1);
    assert(portOptGet(opt) == 0 && savedLudicrousSprint == 0);
    portOptStep(opt, -1);
    assert(portOptGet(opt) == 1 && savedLudicrousSprint == 1);
    assert(portJoyPortOptionSet("ludicrous_sprint", 0));
}


static s32 sprintNear(f32 a, f32 b) { return a - b < 0.0001f && b - a < 0.0001f; }

static void pressSprintToggle(s32 pad)
{
    portJoyPortPadDown(pad, PORTACT_SPRINT_TOGGLE);
    portJoyPortPollCommit();
    portJoyPortPollCommit(); /* released: the next press is a fresh edge */
}

static void moveAt(f32 forwards, f32 sideways)
{
    g_CurrentPlayer->speedforwards = forwards;
    g_CurrentPlayer->speedsideways = sideways;
}

/* Sprint (Toggle): the click-to-sprint latch the controller layouts put on the
 * left stick click (port_sprint_toggle.h). */
static void testSprintToggle(void)
{
    s32 i;
    selectPlayer(0);
    controlsLive = 1;
    g_GlobalTimerDelta = 1.0f;
    assert(strcmp(portJoyPortBindName(PORTACT_SPRINT_TOGGLE), "sprint_toggle") == 0);
    assert(strcmp(portJoyPortBindDefault(PORTACT_SPRINT_TOGGLE), "") == 0); /* Shift holds sprint */

    /* Arrange / act / assert: a press while moving sprints with nothing held,
     * and stopping ends it for good. */
    moveAt(1.0f, 0.0f);
    assert(portJoyPortMoveScale(0) == 1.0f);
    pressSprintToggle(0);
    assert(portJoyPortMoveScale(0) == 1.5f);
    assert(portJoyPortMoveScale(0) == 1.5f);
    moveAt(0.0f, 0.25f); /* under the threshold */
    assert(portJoyPortMoveScale(0) == 1.0f);
    moveAt(1.0f, 0.0f);
    assert(portJoyPortMoveScale(0) == 1.0f);

    /* A press made standing still arms the latch for half a second. */
    moveAt(0.0f, 0.0f);
    pressSprintToggle(0);
    for (i = 0; i < 20; i++) assert(portJoyPortMoveScale(0) == 1.0f);
    moveAt(0.0f, -0.8f); /* strafing counts as moving, as with held sprint */
    assert(portJoyPortMoveScale(0) == 1.5f);
    moveAt(0.0f, 0.0f);
    assert(portJoyPortMoveScale(0) == 1.0f);
    pressSprintToggle(0);
    for (i = 0; i < 30; i++) portJoyPortMoveScale(0);
    moveAt(1.0f, 0.0f);
    assert(portJoyPortMoveScale(0) == 1.0f); /* the armed press lapsed */

    /* A second press cancels. */
    pressSprintToggle(0);
    assert(portJoyPortMoveScale(0) == 1.5f);
    pressSprintToggle(0);
    assert(portJoyPortMoveScale(0) == 1.0f);

    /* Crouching, aiming and ADS end the sprint; it does not come back. */
    pressSprintToggle(0);
    assert(portJoyPortMoveScale(0) == 1.5f);
    assert(portJoyPortMoveScale(1) == 1.0f);
    assert(portJoyPortMoveScale(0) == 1.0f);
    pressSprintToggle(0);
    assert(portJoyPortMoveScale(1) == 1.0f); /* a press while crouched is dropped */
    assert(portJoyPortMoveScale(0) == 1.0f);
    pressSprintToggle(0);
    g_CurrentPlayer->insightaimmode = 1;
    assert(portJoyPortMoveScale(0) == 1.0f);
    g_CurrentPlayer->insightaimmode = 0;
    assert(portJoyPortMoveScale(0) == 1.0f);
    pressSprintToggle(0);
    assert(portJoyPortMoveScale(0) == 1.5f);
    normalAim(1, 1, 0);
    assert(portJoyPortMoveScale(0) == 0.5f);
    normalAim(0, 0, 0);
    assert(portJoyPortMoveScale(0) == 1.0f);

    /* Controls going dead (pause, watch, death) end it too. */
    pressSprintToggle(0);
    assert(portJoyPortMoveScale(0) == 1.5f);
    controlsLive = 0;
    assert(portJoyPortMoveScale(0) == 1.0f);
    controlsLive = 1;
    assert(portJoyPortMoveScale(0) == 1.0f);

    /* Ludicrous Sprint applies; Sprint off makes the toggle do nothing at all. */
    assert(portJoyPortOptionSet("ludicrous_sprint", 1));
    pressSprintToggle(0);
    assert(portJoyPortMoveScale(0) == 5.0f);
    assert(portJoyPortOptionSet("ludicrous_sprint", 0));
    assert(portJoyPortMoveScale(0) == 1.5f);
    assert(portJoyPortOptionSet("sprint", 0));
    assert(portJoyPortMoveScale(0) == 1.0f);
    pressSprintToggle(0);
    assert(portJoyPortMoveScale(0) == 1.0f); /* never the half-speed walk */
    assert(portJoyPortOptionSet("sprint", 1));
    assert(portJoyPortMoveScale(0) == 1.0f);

    /* Each pad has its own latch. */
    pressSprintToggle(0);
    assert(portJoyPortMoveScale(0) == 1.5f);
    selectPlayer(1);
    moveAt(1.0f, 0.0f);
    assert(portJoyPortMoveScale(0) == 1.0f);
    pressSprintToggle(1);
    assert(portJoyPortMoveScale(0) == 1.5f);
    moveAt(0.0f, 0.0f);
    assert(portJoyPortMoveScale(0) == 1.0f);
    selectPlayer(0);
    assert(portJoyPortMoveScale(0) == 1.5f); /* player 1 still sprints */

    /* Loading a suspend point clears it. */
    portStateResetInput();
    assert(portJoyPortMoveScale(0) == 1.0f);
    moveAt(0.0f, 0.0f);
    selectPlayer(1);
    moveAt(0.0f, 0.0f);
    selectPlayer(0);
}

/* Sprint Indicator and Sprint FOV (port_sprint_feedback.h): their settings,
 * the shared fade and the field-of-view scale. */
static void testSprintFeedback(void)
{
    const s32 fovOpt = portOptFind("sprint_fov"), indicatorOpt = portOptFind("sprint_indicator");
    s32 setting, i;
    f32 previous;
    selectPlayer(0);
    controlsLive = 1;
    g_GlobalTimerDelta = 1.0f;
    assert(fovOpt >= 0 && portOptAvailable(fovOpt) && !portOptIsToggle(fovOpt));
    assert(indicatorOpt >= 0 && portOptAvailable(indicatorOpt) && portOptIsToggle(indicatorOpt));
    assert(portOptCategoryOf(fovOpt) == PORTOPT_CAT_GAMEPLAY && portOptCategoryOf(indicatorOpt) == PORTOPT_CAT_GAMEPLAY);
    assert(portJoyPortOptionGet("sprint_fov", &setting) && setting == PORT_SPRINT_FOV_NORMAL); /* default */
    assert(strcmp(portOptValueText(fovOpt), "Normal\n") == 0);
    assert(portJoyPortOptionGet("sprint_indicator", &setting) && setting == 0); /* off by default */

    /* Arrange: a fresh player. Act / assert: no boost, no fade. */
    portSprintFeedbackReset(0);
    portJoyPortTakeSprintBoost(0);
    portSprintFeedbackTick();
    assert(portSprintFeedbackBlend() == 0.0f && portSprintFovScale() == 1.0f);

    /* Holding Walk / Sprint while moving fades in over 9 frames, easing. */
    portJoyPortPadDown(0, PORTACT_WALK);
    portJoyPortPollCommit();
    previous = 1.0f;
    for (i = 0; i < 9; i++) {
        assert(portJoyPortMoveScale(0) == 1.5f);
        portSprintFeedbackTick();
        assert(portSprintFovScale() > previous);
        previous = portSprintFovScale();
    }
    assert(portSprintFeedbackBlend() == 1.0f);
    assert(portSprintFovScale() > 1.0799f && portSprintFovScale() < 1.0801f);

    /* Crouching stops the boost: it fades out over 15 frames. */
    for (i = 0; i < 14; i++) {
        assert(portJoyPortMoveScale(1) == 1.0f);
        portSprintFeedbackTick();
        assert(portSprintFeedbackBlend() > 0.0f && portSprintFovScale() < previous);
        previous = portSprintFovScale();
    }
    portJoyPortMoveScale(1);
    portSprintFeedbackTick();
    assert(portSprintFeedbackBlend() == 0.0f && portSprintFovScale() == 1.0f);

    /* A tick without a movement scale (the tank, a cutscene) counts as stopped. */
    portJoyPortMoveScale(0);
    portSprintFeedbackTick();
    assert(portSprintFeedbackBlend() > 0.0f);
    portSprintFeedbackTick();
    assert(portSprintFeedbackBlend() < 1.0f / PORT_SPRINT_FADE_IN);

    /* The toggle latch drives it too; the step follows the tick length. */
    portJoyPortPollCommit(); /* Walk / Sprint released */
    portSprintFeedbackReset(0);
    moveAt(1.0f, 0.0f);
    pressSprintToggle(0);
    g_GlobalTimerDelta = 4.5f;
    assert(portJoyPortMoveScale(0) == 1.5f);
    portSprintFeedbackTick();
    assert(portSprintFeedbackBlend() == 0.5f);
    g_GlobalTimerDelta = 1.0f;

    /* Each level widens further at full fade; Off keeps the view, and the
     * fade still runs for the indicator. Out-of-range values clamp. */
    assert(portSprintFovScaleFor(PORT_SPRINT_FOV_OFF, 1.0f) == 1.0f);
    assert(sprintNear(portSprintFovScaleFor(PORT_SPRINT_FOV_NORMAL, 1.0f), 1.08f));
    assert(sprintNear(portSprintFovScaleFor(PORT_SPRINT_FOV_WIDER, 1.0f), 1.16f));
    assert(sprintNear(portSprintFovScaleFor(PORT_SPRINT_FOV_CRAZY, 1.0f), 1.30f));
    assert(portSprintFovScaleFor(PORT_SPRINT_FOV_CRAZY, 0.0f) == 1.0f);
    assert(portJoyPortSetOption("sprint_fov", "0"));
    assert(portSprintFovScale() == 1.0f && portSprintFeedbackBlend() == 0.5f);
    assert(portJoyPortSetOption("sprint_fov", "3")); /* saved ini uses the string bridge */
    assert(portSprintFovScale() > 1.08f);
    assert(portJoyPortOptionSet("sprint_fov", 9) && portJoyPortOptionGet("sprint_fov", &setting)
           && setting == PORT_SPRINT_FOV_CRAZY);
    assert(portJoyPortOptionSet("sprint_fov", -2) && portJoyPortOptionGet("sprint_fov", &setting) && setting == 0);
    assert(portJoyPortOptionSet("sprint_fov", PORT_SPRINT_FOV_NORMAL));

    /* Each player has a fade of their own, and a reset clears it. */
    selectPlayer(1);
    assert(portSprintFeedbackBlend() == 0.0f && portSprintFovScale() == 1.0f);
    selectPlayer(0);
    portSprintFeedbackReset(0);
    assert(portSprintFeedbackBlend() == 0.0f);
    pressSprintToggle(0); /* cancel the latch */
    portJoyPortMoveScale(0);
    moveAt(0.0f, 0.0f);

    /* The menu rows reach the settings: Sprint FOV steps Normal, Wider, Crazy
     * Wide and back to Off; the indicator toggles (saving is the shared row
     * path, covered for Sprint above). */
    portOptStep(fovOpt, 1);
    assert(portJoyPortOptionGet("sprint_fov", &setting) && setting == PORT_SPRINT_FOV_WIDER);
    assert(strcmp(portOptValueText(fovOpt), "Wider\n") == 0);
    portOptStep(fovOpt, 1);
    assert(strcmp(portOptValueText(fovOpt), "Crazy Wide\n") == 0);
    portOptStep(fovOpt, -1); portOptStep(fovOpt, -1); portOptStep(fovOpt, -1);
    assert(portJoyPortOptionGet("sprint_fov", &setting) && setting == PORT_SPRINT_FOV_OFF);
    assert(strcmp(portOptValueText(fovOpt), "Off\n") == 0);
    assert(portJoyPortOptionSet("sprint_fov", PORT_SPRINT_FOV_NORMAL));
    portOptStep(indicatorOpt, 1);
    assert(portOptGet(indicatorOpt) == 1 && portJoyPortOptionGet("sprint_indicator", &setting) && setting == 1);
    portOptStep(indicatorOpt, -1);
    assert(portOptGet(indicatorOpt) == 0 && portJoyPortOptionGet("sprint_indicator", &setting) && setting == 0);
}
