/* Included by test_ads.c (after test_ads_accuracy.c, whose ramromFlag stands
 * in for demo playback): Reload Speed (FORK-071) scales only the reload
 * states' timers, Instant ends them at once, and demos keep the native
 * reload. */
static void testReloadSpeed(void)
{
    const s32 opt = portOptFind("reload_speed");
    const s32 savedRamrom = ramromFlag;
    s32 setting = -1;

    /* The setting: Original by default, on the Gameplay options, with
     * Instant as 0 and any midas.ini number kept to 0..10. */
    assert(opt >= 0 && portOptCategoryOf(opt) == PORTOPT_CAT_GAMEPLAY);
    assert(portOptValueCount(opt) == 6);
    assert(portJoyPortOptionGet("reload_speed", &setting) && setting == PORT_RELOAD_SPEED_DEFAULT);
    assert(portJoyPortSetOption("reload_speed", "5"));
    assert(portJoyPortOptionGet("reload_speed", &setting) && setting == 5);
    assert(portJoyPortSetOption("reload_speed", "50"));
    assert(portJoyPortOptionGet("reload_speed", &setting) && setting == PORT_RELOAD_SPEED_MAX);
    assert(portJoyPortSetOption("reload_speed", "-2"));
    assert(portJoyPortOptionGet("reload_speed", &setting) && setting == PORT_RELOAD_SPEED_DEFAULT);
    assert(portJoyPortSetOption("reload_speed", "0"));
    assert(portJoyPortOptionGet("reload_speed", &setting) && setting == PORT_RELOAD_SPEED_INSTANT);

    /* The pure rules: only LOWER, SWAP and RAISE advance faster. START ends
     * on the tick it starts anyway; switches, the watch and firing keep
     * theirs. */
    assert(portReloadSpeedFieldsFor(1, FALSE, GUN_ANIM_STATE_RELOAD_LOWER, 2) == 2);
    assert(portReloadSpeedFieldsFor(10, FALSE, GUN_ANIM_STATE_RELOAD_LOWER, 2) == 20);
    assert(portReloadSpeedFieldsFor(3, FALSE, GUN_ANIM_STATE_RELOAD_SWAP, 1) == 3);
    assert(portReloadSpeedFieldsFor(5, FALSE, GUN_ANIM_STATE_RELOAD_RAISE, 1) == 5);
    assert(portReloadSpeedFieldsFor(10, FALSE, GUN_ANIM_STATE_RELOAD_START, 1) == 1);
    assert(portReloadSpeedFieldsFor(10, FALSE, GUN_ANIM_STATE_SWITCH_LOWER, 1) == 1);
    assert(portReloadSpeedFieldsFor(10, FALSE, GUN_ANIM_STATE_WATCH_RAISE, 1) == 1);
    assert(portReloadSpeedFieldsFor(10, FALSE, GUN_ANIM_STATE_FIRE, 1) == 1);
    assert(portReloadSpeedFieldsFor(10, TRUE, GUN_ANIM_STATE_RELOAD_LOWER, 2) == 2);
    assert(portReloadSpeedFieldsFor(PORT_RELOAD_SPEED_INSTANT, FALSE, GUN_ANIM_STATE_RELOAD_LOWER, 2) == 2);
    assert(portReloadInstantFor(PORT_RELOAD_SPEED_INSTANT, FALSE));
    assert(!portReloadInstantFor(PORT_RELOAD_SPEED_INSTANT, TRUE));
    assert(!portReloadInstantFor(10, FALSE));

    /* The gunfire.c hooks follow the setting and demo playback. */
    ramromFlag = 0;
    assert(portReloadInstant());
    ramromFlag = 1;
    assert(!portReloadInstant());
    assert(portReloadSpeedFields(GUN_ANIM_STATE_RELOAD_RAISE, 1) == 1);
    ramromFlag = 0;
    assert(portJoyPortOptionSet("reload_speed", 10));
    assert(!portReloadInstant());
    assert(portReloadSpeedFields(GUN_ANIM_STATE_RELOAD_RAISE, 1) == 10);
    assert(portReloadSpeedFields(GUN_ANIM_STATE_SWITCH_RAISE, 1) == 1);

    /* Each choice is reachable from the options list. */
    assert(portOptSetIndex(opt, 5));
    assert(portJoyPortOptionGet("reload_speed", &setting) && setting == PORT_RELOAD_SPEED_INSTANT);
    assert(portOptSetIndex(opt, 0));
    assert(portJoyPortOptionGet("reload_speed", &setting) && setting == PORT_RELOAD_SPEED_DEFAULT);
    ramromFlag = savedRamrom;
}
