/* Explosive Rounds (FORK-078): the settings. The rules are in
 * port_explosive_rounds.h; the game hooks in port_explosive_rounds_game.c. */
#include <string.h>
#include <str.h>
#include "port_explosive_rounds.h"

static s32 explosiveRounds = PORT_EXPLOSIVE_ROUNDS_DEFAULT;
static s32 explosiveRoundsGuards = PORT_EXPLOSIVE_ROUNDS_GUARDS_DEFAULT;

s32 portExplosiveRoundsOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "explosive_rounds") == 0) {
        *value = explosiveRounds;
        return TRUE;
    }
    if (strcmp(key, "explosive_rounds_guards") == 0) {
        *value = explosiveRoundsGuards;
        return TRUE;
    }
    return FALSE;
}

s32 portExplosiveRoundsOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "explosive_rounds") == 0) {
        explosiveRounds = portExplosiveRoundsClamp(value);
        return TRUE;
    }
    if (strcmp(key, "explosive_rounds_guards") == 0) {
        explosiveRoundsGuards = value != 0;
        return TRUE;
    }
    return FALSE;
}

s32 portExplosiveRoundsType(s32 demo, s32 shooter)
{
    return portExplosiveRoundsTypeFor(explosiveRounds, explosiveRoundsGuards, demo, shooter);
}
