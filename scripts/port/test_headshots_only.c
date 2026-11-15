/* Included by test_ads.c (through test_tracers.c): Headshots Only (FORK-083).
 * The two settings, the stage latch and which hits lose their damage. The
 * records are checked with the real save codec in test_mission_times.c; the
 * game hooks need the engine and are checked in game. */
#include "../../port/src/port_headshots_only.c"

static void testHeadshotsOnlySettings(void)
{
    const s32 mode = portOptFind("headshots_only");
    const s32 melee = portOptFind("headshots_only_melee");
    s32 value = -1;

    /* Off by default, with melee allowed, both Gameplay toggles. */
    assert(mode >= 0 && portOptCategoryOf(mode) == PORTOPT_CAT_GAMEPLAY);
    assert(melee >= 0 && portOptCategoryOf(melee) == PORTOPT_CAT_GAMEPLAY);
    assert(portJoyPortOptionGet("headshots_only", &value) && value == PORT_HEADSHOTS_ONLY_DEFAULT && value == 0);
    assert(portJoyPortOptionGet("headshots_only_melee", &value) && value == PORT_HEADSHOTS_ONLY_MELEE_DEFAULT && value == 1);
    assert(strcmp(portOptValueText(mode), "OFF\n") == 0);
    assert(strcmp(portOptValueText(melee), "Allowed\n") == 0);

    /* The difficulty page's confirm press flips the mode and wraps. */
    portOptStep(mode, 1);
    assert(portHeadshotsOnlySetting() == 1);
    assert(strcmp(portOptValueText(mode), "ON\n") == 0);
    portOptStep(mode, 1);
    assert(portHeadshotsOnlySetting() == 0);

    /* midas.ini values read as on/off. */
    assert(portJoyPortSetOption("headshots_only", "7") && portHeadshotsOnlySetting() == 1);
    assert(portJoyPortSetOption("headshots_only_melee", "0") && portHeadshotsOnlyMeleeSetting() == 0);
    assert(strcmp(portOptValueText(melee), "Blocked\n") == 0);
    assert(portJoyPortSetOption("headshots_only", "0") && portHeadshotsOnlySetting() == 0);
    assert(portJoyPortSetOption("headshots_only_melee", "1") && portHeadshotsOnlyMeleeSetting() == 1);
}

static void testHeadshotsOnlyLatch(void)
{
    /* Read as a stage loads: solo only, never in demo playback. */
    portJoyPortSetOption("headshots_only", "1");
    portHeadshotsOnlyBeginStage(TRUE, FALSE);
    assert(portHeadshotsOnlyStageOn());
    portHeadshotsOnlyBeginStage(FALSE, FALSE);
    assert(!portHeadshotsOnlyStageOn());
    portHeadshotsOnlyBeginStage(TRUE, TRUE);
    assert(!portHeadshotsOnlyStageOn());

    /* Turning it off mid-mission keeps the run's rule until the next stage. */
    portHeadshotsOnlyBeginStage(TRUE, FALSE);
    portJoyPortSetOption("headshots_only", "0");
    assert(portHeadshotsOnlyStageOn());
    portHeadshotsOnlyBeginStage(TRUE, FALSE);
    assert(!portHeadshotsOnlyStageOn());
}

static void testHeadshotsOnlyRules(void)
{
    static const s32 bodyParts[] = {
        HIT_NULL_PART, HIT_LEFT_FOOT, HIT_LEFT_LEG, HIT_LEFT_THIGH, HIT_RIGHT_FOOT, HIT_RIGHT_LEG,
        HIT_RIGHT_THIGH, HIT_PELVIS, HIT_LEFT_HAND, HIT_LEFT_ARM, HIT_LEFT_SHOULDER, HIT_RIGHT_HAND,
        HIT_RIGHT_ARM, HIT_RIGHT_SHOULDER, HIT_CHEST,
    };
    s32 i;

    /* Off: nothing is blocked. */
    assert(!portHeadshotsOnlyBlocks(FALSE, TRUE, TRUE, HIT_CHEST, ITEM_WPPK));

    /* On: every body part of a player's bullet or thrown knife is blocked... */
    for (i = 0; i < (s32)(sizeof(bodyParts) / sizeof(bodyParts[0])); i++) {
        assert(portHeadshotsOnlyBlocks(TRUE, TRUE, TRUE, bodyParts[i], ITEM_WPPK));
        assert(portHeadshotsOnlyBlocks(TRUE, TRUE, TRUE, bodyParts[i], ITEM_THROWKNIFE));
    }
    /* ...but not the head, nor a hat or a held gun (stock: no damage anyway). */
    assert(!portHeadshotsOnlyBlocks(TRUE, TRUE, TRUE, HIT_HEAD, ITEM_WPPK));
    assert(!portHeadshotsOnlyBlocks(TRUE, TRUE, TRUE, HIT_HEAD, ITEM_THROWKNIFE));
    assert(!portHeadshotsOnlyBlocks(TRUE, TRUE, TRUE, HIT_HAT, ITEM_WPPK));
    assert(!portHeadshotsOnlyBlocks(TRUE, TRUE, TRUE, HIT_GUN, ITEM_WPPK));

    /* Guards' fire and scripted damage are never blocked. */
    assert(!portHeadshotsOnlyBlocks(TRUE, FALSE, FALSE, HIT_CHEST, ITEM_AK47));

    /* Melee (always a chest hit) follows Headshots Melee. */
    assert(!portHeadshotsOnlyBlocks(TRUE, TRUE, TRUE, HIT_CHEST, ITEM_FIST));
    assert(!portHeadshotsOnlyBlocks(TRUE, TRUE, TRUE, HIT_CHEST, ITEM_KNIFE));
    assert(portHeadshotsOnlyBlocks(TRUE, FALSE, TRUE, HIT_CHEST, ITEM_FIST));
    assert(portHeadshotsOnlyBlocks(TRUE, FALSE, TRUE, HIT_CHEST, ITEM_KNIFE));
    /* A thrown knife is not melee. */
    assert(!portHeadshotsOnlyMelee(ITEM_THROWKNIFE));

    /* The hook reads the latched stage and the live melee setting. */
    portJoyPortSetOption("headshots_only", "1");
    portHeadshotsOnlyBeginStage(TRUE, FALSE);
    assert(portHeadshotsOnlyBlocksHit(TRUE, HIT_LEFT_LEG, ITEM_WPPK));
    assert(!portHeadshotsOnlyBlocksHit(TRUE, HIT_LEFT_LEG, ITEM_FIST));
    portJoyPortSetOption("headshots_only_melee", "0");
    assert(portHeadshotsOnlyBlocksHit(TRUE, HIT_LEFT_LEG, ITEM_FIST));
    portJoyPortSetOption("headshots_only_melee", "1");
    portJoyPortSetOption("headshots_only", "0");
    portHeadshotsOnlyBeginStage(TRUE, FALSE);
    assert(!portHeadshotsOnlyBlocksHit(TRUE, HIT_LEFT_LEG, ITEM_WPPK));

    /* Record seconds stay in the stock save's 1..1023 range. */
    assert(portHeadshotsRecordSeconds(0) == 1);
    assert(portHeadshotsRecordSeconds(95) == 95);
    assert(portHeadshotsRecordSeconds(99999999) == PORT_HEADSHOTS_UNTIMED);
    assert(portHeadshotsRecordBetter(0, 500) && portHeadshotsRecordBetter(500, 499));
    assert(!portHeadshotsRecordBetter(500, 500) && !portHeadshotsRecordBetter(500, 501));
}

static void testHeadshotsOnly(void)
{
    testHeadshotsOnlySettings();
    testHeadshotsOnlyLatch();
    testHeadshotsOnlyRules();
}
