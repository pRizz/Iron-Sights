/* Distance-independent hit confirmation sound (FORK-043). gunfire.c still
 * plays and fades the native impact; a confirmed hit only raises its volume. */
#include <string.h>
#include <str.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "snd.h"
#include "game/lv.h"
#include "port_hit_sound.h"

extern s32 getPlayerCount(void);

static s32 hitSoundEnabled = 1;
/* The latest flesh impact. A player's bullet plays it just before applying
 * the damage (chr.c), so a confirmation in the same tick for the same prop is
 * that bullet's; anything older may name a finished, recycled voice. */
static ALSoundState *impactState;
static PropRecord *impactProp;
static s32 impactTimer;

s32 portHitSoundOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "hit_sounds") != 0) return FALSE;
    *value = hitSoundEnabled;
    return TRUE;
}

s32 portHitSoundOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "hit_sounds") != 0) return FALSE;
    hitSoundEnabled = value != 0;
    return TRUE;
}

void portHitSoundImpact(ALSoundState *state, PropRecord *prop)
{
    impactState = state;
    impactProp = prop;
    impactTimer = g_GlobalTimer;
}

void portHitSoundConfirmed(PropRecord *prop)
{
    ALSoundState *state = impactState;

    if (state == NULL || prop != impactProp || g_GlobalTimer != impactTimer) return;
    impactState = NULL;
    /* Shared speakers would announce one split-screen player's hits to all. */
    if (!hitSoundEnabled || getPlayerCount() != 1) return;
    /* Not started yet, so this sets the volume it starts at; the native pan stays. */
    sndCreatePostEvent(state, AL_SNDP_VOL_EVT, PORT_HIT_SOUND_FULL);
}
