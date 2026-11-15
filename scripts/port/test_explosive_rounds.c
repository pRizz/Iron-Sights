/* Included by test_tracers.c (in test_ads.c): Explosive Rounds (FORK-078). Its two settings,
 * which rounds explode and as which explosion type, the explosion slots a
 * round leaves free, the screen shake and hit markers rounds skip, and the
 * one-blast-per-shotgun-shot queue. The game hooks themselves
 * (port_explosive_rounds_game.c) need the engine and are checked in game. */
#include "../../port/src/port_explosive_rounds.c"

static void testExplosiveRoundsSettings(void)
{
    const s32 mode = portOptFind("explosive_rounds");
    const s32 guards = portOptFind("explosive_rounds_guards");
    s32 value = -1;

    /* Off and Everyone by default, both on the Gameplay options. */
    assert(mode >= 0 && portOptCategoryOf(mode) == PORTOPT_CAT_GAMEPLAY);
    assert(guards >= 0 && portOptCategoryOf(guards) == PORTOPT_CAT_GAMEPLAY);
    assert(portOptValueCount(mode) == 3);
    assert(portOptIsToggle(guards));
    assert(portJoyPortOptionGet("explosive_rounds", &value) && value == PORT_EXPLOSIVE_ROUNDS_DEFAULT && value == 0);
    assert(portJoyPortOptionGet("explosive_rounds_guards", &value) && value == PORT_EXPLOSIVE_ROUNDS_GUARDS_DEFAULT && value == 1);

    /* midas.ini values outside 0..2 are kept to the nearest mode. */
    assert(portJoyPortSetOption("explosive_rounds", "2"));
    assert(portJoyPortOptionGet("explosive_rounds", &value) && value == PORT_EXPLOSIVE_ROUNDS_EXPLOSIVE);
    assert(portJoyPortSetOption("explosive_rounds", "9"));
    assert(portJoyPortOptionGet("explosive_rounds", &value) && value == PORT_EXPLOSIVE_ROUNDS_EXPLOSIVE);
    assert(portJoyPortSetOption("explosive_rounds", "-1"));
    assert(portJoyPortOptionGet("explosive_rounds", &value) && value == PORT_EXPLOSIVE_ROUNDS_OFF);
    assert(portJoyPortSetOption("explosive_rounds_guards", "5"));
    assert(portJoyPortOptionGet("explosive_rounds_guards", &value) && value == 1);
    assert(portJoyPortSetOption("explosive_rounds_guards", "0"));
    assert(portJoyPortOptionGet("explosive_rounds_guards", &value) && value == 0);

    /* Each choice is reachable from the options list. */
    assert(portOptSetIndex(mode, 1));
    assert(portJoyPortOptionGet("explosive_rounds", &value) && value == PORT_EXPLOSIVE_ROUNDS_VISUAL);
    assert(portOptSetIndex(mode, 2));
    assert(portJoyPortOptionGet("explosive_rounds", &value) && value == PORT_EXPLOSIVE_ROUNDS_EXPLOSIVE);
    assert(portOptSetIndex(guards, 1));
    assert(portJoyPortOptionGet("explosive_rounds_guards", &value) && value == 1);

    /* The stored setting drives the type the hooks ask for. */
    assert(portExplosiveRoundsType(FALSE, PORT_ROUND_BY_PLAYER) == PORT_EXPLOSION_ROUND_PLAYER);
    assert(portExplosiveRoundsType(FALSE, PORT_ROUND_BY_GUARD) == PORT_EXPLOSION_ROUND_GUARD);
    assert(portExplosiveRoundsType(TRUE, PORT_ROUND_BY_PLAYER) == -1);
    assert(portOptSetIndex(mode, 0));
    assert(portExplosiveRoundsType(FALSE, PORT_ROUND_BY_PLAYER) == -1);
    assert(portJoyPortOptionSet("explosive_rounds_guards", PORT_EXPLOSIVE_ROUNDS_GUARDS_DEFAULT));
}

static void testExplosiveRoundsRules(void)
{
    /* Off and demo playback make no blast; Players skips guards. */
    assert(portExplosiveRoundsTypeFor(PORT_EXPLOSIVE_ROUNDS_OFF, 1, FALSE, PORT_ROUND_BY_PLAYER) == -1);
    assert(portExplosiveRoundsTypeFor(PORT_EXPLOSIVE_ROUNDS_EXPLOSIVE, 1, TRUE, PORT_ROUND_BY_PLAYER) == -1);
    assert(portExplosiveRoundsTypeFor(PORT_EXPLOSIVE_ROUNDS_VISUAL, 1, TRUE, PORT_ROUND_BY_GUARD) == -1);
    assert(portExplosiveRoundsTypeFor(PORT_EXPLOSIVE_ROUNDS_EXPLOSIVE, 0, FALSE, PORT_ROUND_BY_GUARD) == -1);
    assert(portExplosiveRoundsTypeFor(PORT_EXPLOSIVE_ROUNDS_EXPLOSIVE, 0, FALSE, PORT_ROUND_BY_PLAYER) == PORT_EXPLOSION_ROUND_PLAYER);

    /* Visual is one damage-free row for everyone; Explosive splits by shooter. */
    assert(portExplosiveRoundsTypeFor(PORT_EXPLOSIVE_ROUNDS_VISUAL, 1, FALSE, PORT_ROUND_BY_PLAYER) == PORT_EXPLOSION_ROUND_VISUAL);
    assert(portExplosiveRoundsTypeFor(PORT_EXPLOSIVE_ROUNDS_VISUAL, 1, FALSE, PORT_ROUND_BY_GUARD) == PORT_EXPLOSION_ROUND_VISUAL);
    assert(portExplosiveRoundsTypeFor(PORT_EXPLOSIVE_ROUNDS_EXPLOSIVE, 1, FALSE, PORT_ROUND_BY_GUARD) == PORT_EXPLOSION_ROUND_GUARD);

    /* The rows follow Rare's 22 (EXPLOSION_DEF_21 is the last). */
    assert(PORT_EXPLOSION_ROUND_VISUAL == EXPLOSION_DEF_21 + 1);
    assert(PORT_EXPLOSION_ROUND_GUARD == PORT_EXPLOSION_ROUND_VISUAL + 2);

    /* A round leaves the last two slots for grenades, mines and props. */
    assert(!portExplosiveRoundsSlotAllowed(0));
    assert(!portExplosiveRoundsSlotAllowed(PORT_EXPLOSIVE_ROUNDS_RESERVE));
    assert(portExplosiveRoundsSlotAllowed(PORT_EXPLOSIVE_ROUNDS_RESERVE + 1));

    /* Rare's shake rule is kept; rounds never start it. */
    assert(portExplosionStartsShake(EXPLOSION_DEF_STANDARD));
    assert(portExplosionStartsShake(2));
    assert(!portExplosionStartsShake(1));
    assert(!portExplosionStartsShake(0x10));
    assert(!portExplosionStartsShake(PORT_EXPLOSION_ROUND_VISUAL));
    assert(!portExplosionStartsShake(PORT_EXPLOSION_ROUND_PLAYER));
    assert(!portExplosionStartsShake(PORT_EXPLOSION_ROUND_GUARD));

    /* Only a guard's round hides the player's hit marker. */
    assert(portExplosionShowsHitMarker(EXPLOSION_DEF_STANDARD));
    assert(portExplosionShowsHitMarker(PORT_EXPLOSION_ROUND_PLAYER));
    assert(!portExplosionShowsHitMarker(PORT_EXPLOSION_ROUND_GUARD));
}

static void testExplosiveRoundsQueue(void)
{
    PortRoundQueue queue;
    PortRoundBlast first, second, out;

    memset(&queue, 0, sizeof(queue));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    first.pos.x = 1.0f;
    first.type = PORT_EXPLOSION_ROUND_PLAYER;
    second.pos.x = 2.0f;
    second.type = PORT_EXPLOSION_ROUND_PLAYER;

    /* Outside a shotgun shot every blast goes at once. */
    assert(portRoundQueueOffer(&queue, &first));
    assert(!portRoundQueueEnd(&queue, &out));

    /* Within one, only the first pellet's blast is kept, for the end. */
    portRoundQueueBegin(&queue);
    assert(!portRoundQueueOffer(&queue, &first));
    assert(!portRoundQueueOffer(&queue, &second));
    assert(portRoundQueueEnd(&queue, &out) && out.pos.x == 1.0f);
    assert(portRoundQueueOffer(&queue, &second));

    /* A shot whose pellets all missed leaves nothing. */
    portRoundQueueBegin(&queue);
    assert(!portRoundQueueEnd(&queue, &out));
    assert(portRoundQueueOffer(&queue, &first));
}

static void testExplosiveRounds(void)
{
    testExplosiveRoundsSettings();
    testExplosiveRoundsRules();
    testExplosiveRoundsQueue();
}
