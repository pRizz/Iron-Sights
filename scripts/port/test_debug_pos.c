/* Included by test_noclip.c: Debug Position (FORK-084) is off by default, is a
 * toggle row, shows the readout as soon as it changes, and GE_DEBUGPOS
 * overrides it either way. */
#include "../../port/src/port_debug_pos.c"

static void testDebugPos(void)
{
    s32 setting = -1;
    const s32 row = portOptFind("show_debug_pos");

    assert(getenv("GE_DEBUGPOS") == NULL); /* the override cases set it below */
    assert(portJoyPortOptionGet("show_debug_pos", &setting) && setting == PORT_DEBUG_POS_DEFAULT && setting == 0);
    assert(row >= 0 && portOptIsToggle(row));
    assert(!portDebugPosShown());

    /* Live: the next frame's read follows the setting both ways. */
    assert(portJoyPortOptionSet("show_debug_pos", 1) && portDebugPosShown());
    assert(portJoyPortOptionSet("show_debug_pos", 0) && !portDebugPosShown());

    /* GE_DEBUGPOS, read once: anything but a leading 0 forces it on, 0 off. */
    debugPosEnv = -1;
    setenv("GE_DEBUGPOS", "1", 1);
    assert(portDebugPosShown());
    debugPosEnv = -1;
    setenv("GE_DEBUGPOS", "0", 1);
    assert(portJoyPortOptionSet("show_debug_pos", 1) && !portDebugPosShown());
    assert(portJoyPortOptionGet("show_debug_pos", &setting) && setting == 1); /* the setting keeps its value */
    unsetenv("GE_DEBUGPOS");
    debugPosEnv = -1;
    assert(portDebugPosShown());
    assert(portJoyPortOptionSet("show_debug_pos", PORT_DEBUG_POS_DEFAULT) && !portDebugPosShown());
}
