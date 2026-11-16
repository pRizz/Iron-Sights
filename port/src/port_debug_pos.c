/* Debug Position (FORK-084): the setting and its GE_DEBUGPOS override.
 * The rules are in port_debug_pos.h. */
#include <stdlib.h>
#include <string.h>
#include <str.h>
#include "port_debug_pos.h"

static s32 debugPosOn = PORT_DEBUG_POS_DEFAULT;

s32 portDebugPosOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "show_debug_pos") != 0) return FALSE;
    *value = debugPosOn;
    return TRUE;
}

s32 portDebugPosOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "show_debug_pos") != 0) return FALSE;
    debugPosOn = value != 0; /* read each frame: shows or hides at once */
    return TRUE;
}

/* -1 unread, 0 unset (the setting decides), 1 forced on, 2 forced off */
static s32 debugPosEnv = -1;

s32 portDebugPosShown(void)
{
    if (debugPosEnv < 0)
    {
        const char *e = getenv("GE_DEBUGPOS");
        /* as before the setting: anything but a leading '0' turns it on */
        debugPosEnv = (e == NULL || e[0] == '\0') ? 0 : (e[0] != '0' ? 1 : 2);
    }
    if (debugPosEnv == 1) return TRUE;
    if (debugPosEnv == 2) return FALSE;
    return debugPosOn;
}
