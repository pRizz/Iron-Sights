/* Leaning (FORK-073): the setting and the lean controls. The rules are in
 * port_lean.h. */
#include <string.h>
#include "port_lean.h"

extern s32 portJoyPortLeanHeld(s32 *left, s32 *right); /* joy_port.c: 1 = controls live */
extern s32 portNoclipActive(void);                      /* port_noclip.c */

static s32 leaning = PORT_LEANING_DEFAULT;

s32 portLeanOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "leaning") != 0) return FALSE;
    *value = leaning;
    return TRUE;
}

s32 portLeanOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "leaning") != 0) return FALSE;
    leaning = value != 0;
    return TRUE;
}

s32 portLeanInput(void)
{
    s32 left = 0, right = 0;
    const s32 permitted = portJoyPortLeanHeld(&left, &right);

    return portLeanFor(leaning, permitted, portNoclipActive(), left, right);
}
