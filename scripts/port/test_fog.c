/* Included by test_noclip.c: Fog (FORK-074) is on by default, and changing it
 * asks for one reload of the level's environment, at the state applied. */
#include "../../port/src/port_fog.c"

static void testFog(void)
{
    s32 setting = -1;

    assert(portJoyPortOptionGet("fog", &setting) && setting == PORT_FOG_DEFAULT && setting == 1);
    assert(portOptFind("fog") >= 0 && portOptIsToggle(portOptFind("fog")));
    assert(portFogApply() == 1 && !portFogNeedsReload());

    /* Off: the noclip tick reloads once, and the load applies it. */
    selectPlayer(0);
    controlsLive = 1;
    portNoclipResetPlayer(0);
    noclipFogReloads = 0;
    assert(portJoyPortOptionSet("fog", 0) && portFogNeedsReload());
    portNoclipTick();
    assert(noclipFogReloads == 1 && !portFogNeedsReload());
    portNoclipTick();
    assert(noclipFogReloads == 1);
    assert(portJoyPortOptionSet("fog", PORT_FOG_DEFAULT));
    portNoclipTick();
    assert(noclipFogReloads == 2 && !portFogNeedsReload());
}
