/* Reinforcements (FORK-081): the setting. The rules are in
 * port_reinforcements.h; the game hooks in port_reinforcements_game.c. */
#include <string.h>
#include "port_reinforcements.h"

static s32 reinforcements = PORT_REINFORCEMENTS_DEFAULT;

s32 portReinforcementsOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "reinforcements") != 0) return FALSE;
    *value = reinforcements;
    return TRUE;
}

s32 portReinforcementsOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "reinforcements") != 0) return FALSE;
    reinforcements = portReinforcementsClamp(value);
    return TRUE;
}

s32 portReinforcementsSetting(void)
{
    return reinforcements;
}
