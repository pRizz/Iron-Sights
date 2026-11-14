/* Sprint feedback (FORK-006): the Sprint Indicator chevrons and the Sprint FOV
 * widening, driven by one fade per player. The rules are in
 * port_sprint_feedback.h. */
#include <string.h>
#include <ultra64.h>
#include <bondtypes.h>
#include <fr.h>
#include "game/bondview.h"
#include "game/lv.h"
#include "game/player.h"
#include "port_hud.h"
#include "port_sprint_feedback.h"

extern s32 portJoyPortTakeSprintBoost(s32 pad); /* joy_port.c: boosted last tick, then cleared */

#define SPRINT_PLAYERS 4
/* Chevron size in native viewport units: half height, and the stroke width as
 * a multiple of the crosshair's line. */
#define SPRINT_CHEVRON_HALF 2.0f
#define SPRINT_CHEVRON_STROKE 1.2f
/* Above the weapon strip's caption row, so the two never overlap. */
#define SPRINT_CHEVRON_GAP 7

static s32 sIndicator = PORT_SPRINT_INDICATOR_DEFAULT;
static s32 sFov = PORT_SPRINT_FOV_DEFAULT;
static f32 sBlend[SPRINT_PLAYERS];

static s32 *sprintOptionFor(const char *key)
{
    if (strcmp(key, "sprint_indicator") == 0) return &sIndicator;
    if (strcmp(key, "sprint_fov") == 0) return &sFov;
    return NULL;
}

s32 portSprintFeedbackOptionGet(const char *key, s32 *value)
{
    s32 *option = sprintOptionFor(key);
    if (option == NULL) return 0;
    *value = *option;
    return 1;
}

s32 portSprintFeedbackOptionSet(const char *key, s32 value)
{
    s32 *option = sprintOptionFor(key);
    if (option == NULL) return 0;
    if (option == &sFov) *option = value < 0 ? 0 : value > PORT_SPRINT_FOV_CRAZY ? PORT_SPRINT_FOV_CRAZY : value;
    else *option = value != 0;
    return 1;
}

static s32 sprintPlayer(void)
{
    const s32 p = get_cur_playernum();
    return (p >= 0 && p < SPRINT_PLAYERS) ? p : -1;
}

void portSprintFeedbackTick(void)
{
    const s32 p = sprintPlayer();
    if (p < 0) return;
    sBlend[p] = portSprintFadeStep(sBlend[p], portJoyPortTakeSprintBoost(p), g_GlobalTimerDelta);
}

f32 portSprintFeedbackBlend(void)
{
    const s32 p = sprintPlayer();
    return p < 0 ? 0.0f : sBlend[p];
}

f32 portSprintFovScale(void)
{
    return portSprintFovScaleFor(sFov, portSprintFeedbackBlend());
}

void portSprintFeedbackReset(s32 player)
{
    if (player >= 0 && player < SPRINT_PLAYERS) sBlend[player] = 0.0f;
}

Gfx *portSprintIndicatorDraw(Gfx *gdl)
{
    const f32 blend = portSprintFeedbackBlend();
    const f32 x = viGetViewLeft() + viGetViewWidth() * 0.5f;
    const f32 y = viGetViewTop() + viGetViewHeight() - portHudWeaponStripTopInset() - SPRINT_CHEVRON_GAP;
    u32 alpha;
    if (!sIndicator || blend <= 0.0f || g_CurrentPlayer == NULL || g_CurrentPlayer->bonddead) return gdl;
    alpha = (u32)(255.0f * PORT_SPRINT_INDICATOR_ALPHA * portSprintEase(blend) + 0.5f);
    if (alpha == 0) return gdl;
    /* A soft shadow half a unit down-right keeps it legible on bright walls. */
    gdl = portHudChevrons(gdl, x + 0.5f, y + 0.5f, SPRINT_CHEVRON_HALF, SPRINT_CHEVRON_STROKE, alpha * 3 / 5);
    return portHudChevrons(gdl, x, y, SPRINT_CHEVRON_HALF, SPRINT_CHEVRON_STROKE, 0xe1e5ea00 | alpha);
}
