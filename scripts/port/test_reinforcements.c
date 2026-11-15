/* Included by test_ads.c: Reinforcements (FORK-081). The setting, the pool
 * sizes each level adds, which guards may clone, the per-guard limit and
 * cooldown, and the free slots a clone Rare would not make leaves for the
 * level. The game hooks (port_reinforcements_game.c) need the engine and are
 * checked in game with GE_REINFORCEMENTS_LOG. */
#include "../../port/src/port_reinforcements.c"
#include "port_reinforcements_director.h"

static void testReinforcementsSetting(void)
{
    const s32 opt = portOptFind("reinforcements");
    s32 value = -1;

    /* Original by default, three levels on the Gameplay options. */
    assert(opt >= 0 && portOptCategoryOf(opt) == PORTOPT_CAT_GAMEPLAY);
    assert(portOptValueCount(opt) == 3);
    assert(portJoyPortOptionGet("reinforcements", &value) && value == PORT_REINFORCEMENTS_DEFAULT && value == 0);
    assert(strcmp(portOptValueText(opt), "Original\n") == 0);

    /* midas.ini values outside 0..2 are kept to the nearest level. */
    assert(portJoyPortSetOption("reinforcements", "9"));
    assert(portReinforcementsSetting() == PORT_REINFORCEMENTS_RELENTLESS);
    assert(portJoyPortSetOption("reinforcements", "-3"));
    assert(portReinforcementsSetting() == PORT_REINFORCEMENTS_ORIGINAL);

    /* The difficulty page's confirm press steps through the levels and wraps. */
    portOptStep(opt, 1);
    assert(portReinforcementsSetting() == PORT_REINFORCEMENTS_MORE);
    assert(strcmp(portOptValueText(opt), "More\n") == 0);
    portOptStep(opt, 1);
    assert(portReinforcementsSetting() == PORT_REINFORCEMENTS_RELENTLESS);
    portOptStep(opt, 1);
    assert(portReinforcementsSetting() == PORT_REINFORCEMENTS_ORIGINAL);
}

static void testReinforcementsStage(void)
{
    PortReinforcementRules original = portReinforcementRulesFor(PORT_REINFORCEMENTS_ORIGINAL);
    PortReinforcementRules more = portReinforcementRulesFor(PORT_REINFORCEMENTS_MORE);
    PortReinforcementRules relentless = portReinforcementRulesFor(PORT_REINFORCEMENTS_RELENTLESS);

    /* Original adds nothing and leaves Rare's clone rule alone. */
    assert(original.extraChrs == 0 && original.clonesPerGuard == 0 && !original.afterSeen);
    assert(portReinforcementsExtraModels(original.extraChrs) == 0);
    assert(portReinforcementsExtraAnimModels(original.extraChrs) == 0);

    /* More and Relentless add slots, models for each guard and two guns,
     * more clones per guard and a shorter wait. */
    assert(more.extraChrs == 12 && relentless.extraChrs == 24);
    assert(portReinforcementsExtraAnimModels(more.extraChrs) == 12);
    assert(portReinforcementsExtraModels(relentless.extraChrs) == 48);
    assert(more.clonesPerGuard == 2 && relentless.clonesPerGuard == 3);
    assert(more.cooldownTicks == 8 * 60 && relentless.cooldownTicks == 4 * 60);
    assert(!more.afterSeen && relentless.afterSeen);

    /* Multiplayer (no placed guards) and demo playback stay Original. */
    assert(portReinforcementsStageLevel(PORT_REINFORCEMENTS_RELENTLESS, 30, FALSE) == PORT_REINFORCEMENTS_RELENTLESS);
    assert(portReinforcementsStageLevel(PORT_REINFORCEMENTS_RELENTLESS, 0, FALSE) == PORT_REINFORCEMENTS_ORIGINAL);
    assert(portReinforcementsStageLevel(PORT_REINFORCEMENTS_MORE, 30, TRUE) == PORT_REINFORCEMENTS_ORIGINAL);
    assert(portReinforcementsStageLevel(7, 30, FALSE) == PORT_REINFORCEMENTS_RELENTLESS);
}

static void testReinforcementsEligible(void)
{
    PortReinforcementRules original = portReinforcementRulesFor(PORT_REINFORCEMENTS_ORIGINAL);
    PortReinforcementRules more = portReinforcementRulesFor(PORT_REINFORCEMENTS_MORE);
    PortReinforcementGuard soldier = { TRUE, FALSE, FALSE, FALSE, TRUE };
    PortReinforcementGuard guard;

    /* Original never widens Rare's rule, even for a flagged guard. */
    guard = soldier;
    guard.cloneFlag = TRUE;
    assert(!portReinforcementsEligible(&original, &guard));

    /* A placed soldier may clone; a flagged guard may whatever its body. */
    assert(portReinforcementsEligible(&more, &soldier));
    guard = soldier;
    guard.soldierBody = FALSE;
    assert(!portReinforcementsEligible(&more, &guard));
    guard.cloneFlag = TRUE;
    assert(portReinforcementsEligible(&more, &guard));

    /* Spawned guards, clones and invincible characters never clone. */
    guard = soldier;
    guard.placed = FALSE;
    assert(!portReinforcementsEligible(&more, &guard));
    guard = soldier;
    guard.isClone = TRUE;
    assert(!portReinforcementsEligible(&more, &guard));
    guard = soldier;
    guard.invincible = TRUE;
    guard.cloneFlag = TRUE;
    assert(!portReinforcementsEligible(&more, &guard));

    /* Soldiers' uniforms count; named characters, Bond and civilians do not. */
    assert(portReinforcementsSoldierBody(BODY_Russian_Soldier));
    assert(portReinforcementsSoldierBody(BODY_Janus_Special_Forces));
    assert(portReinforcementsSoldierBody(BODY_Moonraker_Elite_2_Female));
    assert(!portReinforcementsSoldierBody(BODY_Ourumov));
    assert(!portReinforcementsSoldierBody(BODY_Natalya_Skirt));
    assert(!portReinforcementsSoldierBody(BODY_Brosnan_Tuxedo));
    assert(!portReinforcementsSoldierBody(BODY_Scientist_1_Male));
    assert(!portReinforcementsSoldierBody(BODY_Civilian_1_Female));
}

static void testReinforcementsCloneDue(void)
{
    PortReinforcementRules more = portReinforcementRulesFor(PORT_REINFORCEMENTS_MORE);

    /* The first clone comes at once; the next after the cooldown. */
    assert(portReinforcementsCloneDue(&more, 0, -1, 1000));
    assert(!portReinforcementsCloneDue(&more, 1, 1000, 1000 + more.cooldownTicks - 1));
    assert(portReinforcementsCloneDue(&more, 1, 1000, 1000 + more.cooldownTicks));
    /* No more than the limit stay alive at once. */
    assert(!portReinforcementsCloneDue(&more, 2, -1, 100000));

    /* A clone Rare would not make leaves Rare's 10 spare slots to the level. */
    assert(portReinforcementsFreeChrsNeeded(FALSE) == PORT_REINFORCEMENTS_UPSTREAM_FREE_CHRS);
    assert(portReinforcementsFreeChrsNeeded(TRUE) == PORT_REINFORCEMENTS_UPSTREAM_SPARE_CHRS);
}

static void testReinforcementsDirector(void)
{
    PortReinforcementRules original = portReinforcementRulesFor(PORT_REINFORCEMENTS_ORIGINAL);
    PortReinforcementRules more = portReinforcementRulesFor(PORT_REINFORCEMENTS_MORE);
    PortReinforcementRules relentless = portReinforcementRulesFor(PORT_REINFORCEMENTS_RELENTLESS);
    const s32 roomy = PORT_REINFORCEMENTS_UPSTREAM_SPARE_CHRS;

    /* Off at Original; every 20 s up to 4 (More) or 10 s up to 8 (Relentless). */
    assert(original.directorTicks == 0 && !portDirectorDue(&original, TRUE, 0, -1, 1000, 50));
    assert(more.directorTicks == 20 * 60 && more.directorCap == 4);
    assert(relentless.directorTicks == 10 * 60 && relentless.directorCap == 8);

    /* It sends only for 20 s after Bond was last noticed. */
    assert(!portDirectorActive(-1, 500));
    assert(portDirectorActive(100, 100 + PORT_DIRECTOR_NOTICED_TICKS - 1));
    assert(!portDirectorActive(100, 100 + PORT_DIRECTOR_NOTICED_TICKS));

    /* First guard at once, then the interval, the cap and Rare's spare slots. */
    assert(portDirectorDue(&more, TRUE, 0, -1, 1000, roomy));
    assert(!portDirectorDue(&more, FALSE, 0, -1, 1000, roomy));
    assert(!portDirectorDue(&more, TRUE, 0, 1000, 1000 + more.directorTicks - 1, roomy));
    assert(portDirectorDue(&more, TRUE, 0, 1000, 1000 + more.directorTicks, roomy));
    assert(!portDirectorDue(&more, TRUE, more.directorCap, -1, 1000, roomy));
    assert(!portDirectorDue(&relentless, TRUE, 0, -1, 1000, roomy - 1));

    /* Pads: 600 to 6000 units across the floor, within 400 up or down. */
    assert(!portDirectorPadInRange(300.0f, 0.0f, 300.0f));
    assert(portDirectorPadInRange(600.0f, 0.0f, 0.0f));
    assert(portDirectorPadInRange(3000.0f, -399.0f, 3000.0f));
    assert(!portDirectorPadInRange(3000.0f, 401.0f, 0.0f));
    assert(!portDirectorPadInRange(5000.0f, 0.0f, 5000.0f));

    /* Routes of 2 to 14 waypoint steps; 0 means no route at all. */
    assert(!portDirectorRouteInRange(0) && !portDirectorRouteInRange(1));
    assert(portDirectorRouteInRange(2) && portDirectorRouteInRange(14));
    assert(!portDirectorRouteInRange(15));

    /* Spawn clearance: 60 units across, ignoring other floors. */
    assert(portSpawnTooClose(40.0f, 0.0f, 40.0f, PORT_SPAWN_CLEARANCE));
    assert(!portSpawnTooClose(60.0f, 0.0f, 0.0f, PORT_SPAWN_CLEARANCE));
    assert(!portSpawnTooClose(10.0f, 250.0f, 10.0f, PORT_SPAWN_CLEARANCE));
}

static void testReinforcements(void)
{
    testReinforcementsSetting();
    testReinforcementsStage();
    testReinforcementsEligible();
    testReinforcementsCloneDue();
    testReinforcementsDirector();
}
