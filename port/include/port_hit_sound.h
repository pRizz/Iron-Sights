#ifndef PORT_HIT_SOUND_H
#define PORT_HIT_SOUND_H
#include <ultra64.h>
#include <bondtypes.h>

/* The native flesh impact fades with distance (sub_GAME_7F0539E4). When a
 * solo player's shot damages the character it struck, that same impact is
 * raised to point-blank volume, so a distant hit sounds as it does up close.
 * A second copy cannot do this: the synth's loudness grows with the square of
 * a voice's volume, so two partial voices fall well short of one full one. */
#define PORT_HIT_SOUND_FULL 32767

s32 portHitSoundOptionGet(const char *key, s32 *value);
s32 portHitSoundOptionSet(const char *key, s32 value);
/* gunfire.c: a bullet's flesh impact on prop has just started. */
void portHitSoundImpact(ALSoundState *state, PropRecord *prop);
/* chraction.c: a player's shot damaged the character that owns prop. */
void portHitSoundConfirmed(PropRecord *prop);

#endif
