/* Sprint movement checks, included by test_ads.c: Walk / Sprint (FORK-006) and
 * Ludicrous Sprint, their settings rows and saved keys. */

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

