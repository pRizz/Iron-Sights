/* Reload Speed (FORK-071): the setting and the gunfire.c hooks. The rules
 * are in port_reload_speed.h. */
#include <string.h>
#include "port_reload_speed.h"

extern s32 get_is_ramrom_flag(void);

static s32 reloadSpeed = PORT_RELOAD_SPEED_DEFAULT;

s32 portReloadSpeedOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "reload_speed") != 0) return FALSE;
    *value = reloadSpeed;
    return TRUE;
}

s32 portReloadSpeedOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "reload_speed") != 0) return FALSE;
    reloadSpeed = portReloadSpeedClamp(value);
    return TRUE;
}

s32 portReloadSpeedFields(s32 state, s32 clockTimer)
{
    return portReloadSpeedFieldsFor(reloadSpeed, get_is_ramrom_flag() != 0, state, clockTimer);
}

s32 portReloadInstant(void)
{
    return portReloadInstantFor(reloadSpeed, get_is_ramrom_flag() != 0);
}
