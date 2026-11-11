/* Fog (FORK-074): the setting and the state the level was loaded with.
 * The rules are in port_fog.h. */
#include <string.h>
#include "port_fog.h"

static s32 fogOn = PORT_FOG_DEFAULT;
static s32 fogApplied = PORT_FOG_DEFAULT;

s32 portFogOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "fog") != 0) return FALSE;
    *value = fogOn;
    return TRUE;
}

s32 portFogOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "fog") != 0) return FALSE;
    fogOn = value != 0; /* the next MoveBond tick reloads the level's fog */
    return TRUE;
}

s32 portFogApply(void)
{
    fogApplied = fogOn;
    return fogApplied;
}

s32 portFogNeedsReload(void)
{
    return fogOn != fogApplied;
}
