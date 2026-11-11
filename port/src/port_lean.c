/* Leaning (FORK-073): the settings, the lean controls and Lean Tilt. The
 * rules are in port_lean.h. */
#include <string.h>
#include <ultra64.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/player.h"
#include "port_lean.h"

extern s32 portJoyPortLeanHeld(s32 *left, s32 *right); /* joy_port.c: 1 = controls live */
extern s32 portNoclipActive(void);                      /* port_noclip.c */
extern s32 get_is_ramrom_flag(void);
/* The tree's math.h hides its float declarations from GCC. */
extern float sinf(float);
extern float cosf(float);
extern float sqrtf(float);
#define LEAN_DEG_TO_RAD 0.017453292f

static s32 leaning = PORT_LEANING_DEFAULT;
static s32 leanTilt = PORT_LEAN_TILT_DEFAULT;

s32 portLeanOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "lean_tilt") == 0)
    {
        *value = leanTilt;
        return TRUE;
    }
    if (strcmp(key, "leaning") != 0) return FALSE;
    *value = leaning;
    return TRUE;
}

s32 portLeanOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "lean_tilt") == 0)
    {
        leanTilt = portLeanTiltClamp(value);
        return TRUE;
    }
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

void portLeanTilt(f32 *look, f32 *up)
{
    const struct player *pl = g_CurrentPlayer;
    f32 lateral, deg, a, c, s, len, kx, ky, kz, dot, cx, cy, cz;

    if (pl == NULL || leanTilt <= 0) return;
    /* The body's offset along Bond's right (theta_transform is his facing),
     * signed as the sway target: negative = left. */
    lateral = -pl->swayoffset0 * pl->field_488.theta_transform.f[2]
              + pl->swayoffset2 * pl->field_488.theta_transform.f[0];
    deg = portLeanTiltDegFor(leanTilt, get_is_ramrom_flag() != 0, portNoclipActive(), lateral);
    if (deg == 0.0f) return;
    len = sqrtf(look[0] * look[0] + look[1] * look[1] + look[2] * look[2]);
    if (len < 0.0001f) return;
    kx = look[0] / len;
    ky = look[1] / len;
    kz = look[2] / len;
    /* Rodrigues: turn up around the line of sight. */
    a = deg * LEAN_DEG_TO_RAD;
    c = cosf(a);
    s = sinf(a);
    dot = kx * up[0] + ky * up[1] + kz * up[2];
    cx = ky * up[2] - kz * up[1];
    cy = kz * up[0] - kx * up[2];
    cz = kx * up[1] - ky * up[0];
    up[0] = up[0] * c + cx * s + kx * dot * (1.0f - c);
    up[1] = up[1] * c + cy * s + ky * dot * (1.0f - c);
    up[2] = up[2] * c + cz * s + kz * dot * (1.0f - c);
}
