/* Included by test_noclip.c (so test_ads.c keeps its line budget): Leaning
 * (FORK-073) drives the stock lean from the Lean Left / Lean Right controls,
 * only with the setting on, the controls live and noclip not flying. */
#include "../../port/src/port_lean.c"

static void testLean(void)
{
    s32 setting = -1;

    /* The pure rule: left wins, as in the stock code. */
    assert(portLeanFor(TRUE, TRUE, FALSE, TRUE, FALSE) == -1);
    assert(portLeanFor(TRUE, TRUE, FALSE, FALSE, TRUE) == 1);
    assert(portLeanFor(TRUE, TRUE, FALSE, TRUE, TRUE) == -1);
    assert(portLeanFor(TRUE, TRUE, FALSE, FALSE, FALSE) == 0);
    assert(portLeanFor(FALSE, TRUE, FALSE, TRUE, FALSE) == 0);
    assert(portLeanFor(TRUE, FALSE, FALSE, TRUE, FALSE) == 0);
    assert(portLeanFor(TRUE, TRUE, TRUE, TRUE, FALSE) == 0);

    /* The setting and the binds: on by default, Z and C, appended after the
     * noclip controls so saved bindings keep their slots. */
    assert(portJoyPortOptionGet("leaning", &setting) && setting == PORT_LEANING_DEFAULT && setting == 1);
    assert(portOptFind("leaning") >= 0 && portOptIsToggle(portOptFind("leaning")));
    assert(PORTACT_LEAN_LEFT == 31 && strcmp(portJoyPortBindDefault(PORTACT_LEAN_LEFT), "Z") == 0);
    assert(PORTACT_LEAN_RIGHT == 32 && strcmp(portJoyPortBindDefault(PORTACT_LEAN_RIGHT), "C") == 0);
    /* video.c's PORT_BIND_MAX (64) must cover every action: portJoyPortPoll reads
     * PORTACT_COUNT entries from its pdown arrays, and at 32 the 33rd read a
     * stray byte that pressed Lean Right at random. */
    assert(PORTACT_COUNT <= 64);

    /* Held controls reach the game side for the player whose tick this is. */
    selectPlayer(0);
    controlsLive = 1;
    portNoclipResetPlayer(0);
    portJoyPortPadDown(0, PORTACT_LEAN_LEFT);
    portJoyPortPollCommit();
    assert(portLeanInput() == -1);
    portJoyPortPadDown(0, PORTACT_LEAN_RIGHT);
    portJoyPortPollCommit();
    assert(portLeanInput() == 1);
    controlsLive = 0;
    assert(portLeanInput() == 0);
    controlsLive = 1;
    assert(portJoyPortOptionSet("leaning", 0));
    assert(portLeanInput() == 0);
    assert(portJoyPortOptionSet("leaning", PORT_LEANING_DEFAULT));
    portJoyPortPollCommit(); /* released */
    assert(portLeanInput() == 0);
}
