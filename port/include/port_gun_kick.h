#ifndef PORT_GUN_KICK_H
#define PORT_GUN_KICK_H
#include <ultra64.h>

/* Weapon Kick (FORK-061): every round fired pushes the first-person gun back
 * and up on a stiff spring, so held automatic fire shows a jolt per shot and a
 * capped climb. Enhanced, Ludicrous and Cartoon replace the native whole-gun recoil
 * animation, which only played after the trigger was released; Original keeps
 * that animation.
 * Purely visual: the shot ray, fire rate and game RNG are untouched. */
#define PORT_GUN_KICK_ORIGINAL 0
#define PORT_GUN_KICK_ENHANCED 1
#define PORT_GUN_KICK_LUDICROUS 2 /* Enhanced with 3x the turns and climb */
#define PORT_GUN_KICK_CARTOON 3   /* Enhanced with 5.5x the turns and climb */
#define PORT_GUN_KICK_DEFAULT PORT_GUN_KICK_ENHANCED

/* A weapon's kick at the hip. Single-shot values are the peak a shot reaches
 * from rest; climb is the extra muzzle rise sustained fire builds. */
typedef struct PortGunKickProfile {
    f32 back;      /* gun units towards the camera */
    f32 rise;      /* gun units up */
    f32 pitch;     /* degrees, muzzle up */
    f32 yaw;       /* degrees, random side either way */
    f32 roll;      /* degrees, random twist either way */
    f32 climbStep; /* degrees of climb added per shot */
    f32 climbMax;  /* climb ceiling, degrees */
    f32 omega;     /* spring natural frequency, rad/s: higher returns sooner */
    f32 damping;   /* damping ratio, below 1 so it overshoots a little */
} PortGunKickProfile;

enum PortGunKickChannel { KICK_BACK, KICK_RISE, KICK_PITCH, KICK_YAW, KICK_ROLL, KICK_CHANNELS };

typedef struct PortGunKickState {
    f32 pos[KICK_CHANNELS];
    f32 vel[KICK_CHANNELS];
    f32 climbTarget;
    f32 climb;
    u32 rng;
    s32 item;
} PortGunKickState;

/* Offset added to the drawn gun: gun units and radians (pitch > 0 is muzzle up). */
typedef struct PortGunKickPose {
    f32 x, y, z;
    f32 pitch, yaw, roll;
} PortGunKickPose;

/* Pure core. */
const PortGunKickProfile *portGunKickMaybeProfile(s32 item);
void portGunKickImpulse(PortGunKickState *state, const PortGunKickProfile *profile);
void portGunKickStep(PortGunKickState *state, const PortGunKickProfile *profile, f32 seconds);
/* `profile` scaled for Ludicrous or Cartoon `mode`; other modes copy it. */
void portGunKickExaggerate(const PortGunKickProfile *profile, s32 mode, PortGunKickProfile *out);
void portGunKickPoseOf(const PortGunKickState *state, f32 scale, PortGunKickPose *out);

/* Game hooks (current player). */
s32 portGunKickOptionGet(const char *key, s32 *value);
s32 portGunKickOptionSet(const char *key, s32 value);
/* gunfire.c RECOIL1: multiplier on the native recoil kick, 0 while the spring owns it. */
f32 portGunKickNativeScale(s32 item);
/* gunfire.c: a round left `hand`. */
void portGunKickFire(s32 hand, s32 item);
/* gunfire.c gun pose: advance by `fields` (60 Hz field units) and return the
 * offset to draw, scaled by ADS Recoil; FALSE when there is none. */
s32 portGunKickAdvance(s32 hand, s32 item, f32 fields, PortGunKickPose *out);

#endif
