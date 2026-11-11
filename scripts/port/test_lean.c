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

    /* Lean Tilt: off by default; degrees scale with how far the body has
     * moved, clamp at a full lean, and stop in demos and noclip flight. */
    assert(portJoyPortOptionGet("lean_tilt", &setting) && setting == PORT_LEAN_TILT_DEFAULT && setting == 0);
    assert(portOptFind("lean_tilt") >= 0 && portOptValueCount(portOptFind("lean_tilt")) == 3);
    assert(portLeanTiltDegFor(12, FALSE, FALSE, -75.0f) == -12.0f);
    assert(portLeanTiltDegFor(12, FALSE, FALSE, 37.5f) == 6.0f);
    assert(portLeanTiltDegFor(6, FALSE, FALSE, 150.0f) == 6.0f);
    assert(portLeanTiltDegFor(0, FALSE, FALSE, -75.0f) == 0.0f);
    assert(portLeanTiltDegFor(12, TRUE, FALSE, -75.0f) == 0.0f);
    assert(portLeanTiltDegFor(12, FALSE, TRUE, -75.0f) == 0.0f);
    assert(portLeanTiltClamp(99) == PORT_LEAN_TILT_MAX && portLeanTiltClamp(-1) == 0);
    {
        /* The roll turns up around the look and leaves the look alone:
         * facing +z, a full left lean (body offset -75 along his right, +x)
         * at Strong turns up by 12 degrees. */
        f32 look[3] = { 0.0f, 0.0f, 1.0f };
        f32 up[3] = { 0.0f, 1.0f, 0.0f };
        const s32 savedRamrom = ramromFlag;

        ramromFlag = 0;
        g_CurrentPlayer->field_488.theta_transform.f[0] = 0.0f;
        g_CurrentPlayer->field_488.theta_transform.f[2] = 1.0f;
        g_CurrentPlayer->swayoffset0 = 75.0f; /* lateral = -x * 1 = -75: left */
        g_CurrentPlayer->swayoffset2 = 0.0f;
        portLeanTilt(look, up); /* off: unchanged */
        assert(up[0] == 0.0f && up[1] == 1.0f);
        assert(portJoyPortOptionSet("lean_tilt", 12));
        portLeanTilt(look, up);
        assert(look[0] == 0.0f && look[1] == 0.0f && look[2] == 1.0f);
        assert(fabsf(up[1] - cosf(12.0f * LEAN_DEG_TO_RAD)) < 0.0001f);
        assert(fabsf(fabsf(up[0]) - sinf(12.0f * LEAN_DEG_TO_RAD)) < 0.0001f && up[2] == 0.0f);
        assert(portJoyPortOptionSet("lean_tilt", PORT_LEAN_TILT_DEFAULT));
        g_CurrentPlayer->swayoffset0 = 0.0f;
        ramromFlag = savedRamrom;
    }
}
