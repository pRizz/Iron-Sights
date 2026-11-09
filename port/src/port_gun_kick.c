/* Weapon Kick (FORK-061): a per-shot spring kick on the first-person gun.
 * gunfire.c calls portGunKickFire for each round and adds the advanced pose to
 * the drawn gun; the native recoil animation is zeroed while the spring owns it. */
#include <math.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "port_ads_accuracy.h"
#include "port_gun_kick.h"

extern s32 get_cur_playernum(void);
/* The project's math.h leaves expf undeclared; the C library provides it. */
extern float expf(float);

#define KICK_DEG_TO_RAD 0.017453292f
/* Climb drains with this time constant and the drawn climb trails its target
 * by this one, so the per-shot steps don't show as stairs. */
#define KICK_CLIMB_DECAY_S 0.22f
#define KICK_CLIMB_FOLLOW_S 0.04f
#define KICK_FIELDS_PER_S 60.0f

/*                                        back  rise  pitch yaw  roll  climb max  omega damp */
static const PortGunKickProfile kPistol   = { 1.2f, 0.25f, 5.0f, 0.8f, 1.5f, 0.0f, 0.0f, 34.0f, 0.55f };
static const PortGunKickProfile kSilenced = { 0.8f, 0.15f, 3.2f, 0.6f, 1.0f, 0.0f, 0.0f, 36.0f, 0.55f };
static const PortGunKickProfile kDD44     = { 1.4f, 0.30f, 6.0f, 1.0f, 1.8f, 0.0f, 0.0f, 32.0f, 0.55f };
static const PortGunKickProfile kMagnum   = { 2.0f, 0.50f, 13.0f, 1.2f, 2.5f, 0.0f, 0.0f, 22.0f, 0.60f };
static const PortGunKickProfile kSmg      = { 0.55f, 0.12f, 1.4f, 0.7f, 1.0f, 0.30f, 2.5f, 50.0f, 0.50f };
static const PortGunKickProfile kSmgQuiet = { 0.45f, 0.10f, 1.1f, 0.6f, 0.8f, 0.25f, 2.0f, 50.0f, 0.50f };
static const PortGunKickProfile kP90      = { 0.45f, 0.10f, 1.0f, 0.6f, 0.8f, 0.20f, 2.5f, 54.0f, 0.50f };
static const PortGunKickProfile kRifle    = { 0.75f, 0.15f, 1.8f, 0.6f, 0.8f, 0.40f, 3.5f, 46.0f, 0.50f };
static const PortGunKickProfile kShotgun  = { 3.0f, 0.60f, 12.0f, 1.5f, 3.0f, 0.0f, 0.0f, 18.0f, 0.60f };
static const PortGunKickProfile kAutoshot = { 2.2f, 0.45f, 8.0f, 1.2f, 2.2f, 0.0f, 0.0f, 22.0f, 0.60f };
static const PortGunKickProfile kSniper   = { 1.0f, 0.20f, 3.5f, 0.4f, 0.8f, 0.0f, 0.0f, 30.0f, 0.55f };
static const PortGunKickProfile kGrenade  = { 2.6f, 0.70f, 11.0f, 1.0f, 2.0f, 0.0f, 0.0f, 16.0f, 0.60f };
/* Shoulder-fired and close to the camera, so a heavy shove and less travel. */
static const PortGunKickProfile kRocket   = { 1.8f, 0.80f, 7.0f, 1.0f, 1.5f, 0.0f, 0.0f, 14.0f, 0.65f };

/* The exaggerated modes multiply rotation and climb by `turn`, travel by the
 * smaller `shift` so close guns stay clear of the camera, and the spring
 * frequency by `spring` so the bigger kick reads as heavier. */
typedef struct KickExaggeration { f32 turn, shift, spring; } KickExaggeration;
static const KickExaggeration kLudicrous = { 3.0f, 1.5f, 0.8f };
static const KickExaggeration kCartoon   = { 5.5f, 2.0f, 0.7f };

const PortGunKickProfile *portGunKickMaybeProfile(s32 item)
{
    switch (item)
    {
    case ITEM_WPPK: case ITEM_GOLDWPPK: case ITEM_SILVERWPPK: case ITEM_FLAREPISTOL: return &kPistol;
    case ITEM_WPPKSIL: return &kSilenced;
    case ITEM_TT33: return &kDD44;
    case ITEM_RUGER: case ITEM_GOLDENGUN: return &kMagnum;
    case ITEM_UZI: case ITEM_MP5K: case ITEM_SPECTRE: case ITEM_SKORPION: return &kSmg;
    case ITEM_MP5KSIL: return &kSmgQuiet;
    case ITEM_FNP90: return &kP90;
    case ITEM_AK47: case ITEM_M16: return &kRifle;
    case ITEM_SHOTGUN: return &kShotgun;
    case ITEM_AUTOSHOT: return &kAutoshot;
    case ITEM_SNIPERRIFLE: return &kSniper;
    case ITEM_GRENADELAUNCH: return &kGrenade;
    case ITEM_ROCKETLAUNCH: return &kRocket;
    default: return NULL; /* knife, fists, taser, lasers, gadgets: native only */
    }
}

void portGunKickExaggerate(const PortGunKickProfile *profile, s32 mode, PortGunKickProfile *out)
{
    const KickExaggeration *maybeScale = mode == PORT_GUN_KICK_LUDICROUS ? &kLudicrous
                                       : mode == PORT_GUN_KICK_CARTOON ? &kCartoon : NULL;

    *out = *profile;
    if (maybeScale == NULL) return;
    out->back *= maybeScale->shift;
    out->rise *= maybeScale->shift;
    out->pitch *= maybeScale->turn;
    out->yaw *= maybeScale->turn;
    out->roll *= maybeScale->turn;
    out->climbStep *= maybeScale->turn;
    out->climbMax *= maybeScale->turn;
    out->omega *= maybeScale->spring;
}

/* xorshift32, private so the kick never draws on the game's random stream. */
static f32 kickRandomSigned(PortGunKickState *state)
{
    u32 x = state->rng ? state->rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state->rng = x;
    return (f32) (x >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

static f32 kickDamping(const PortGunKickProfile *profile)
{
    return profile->damping < 0.2f ? 0.2f : profile->damping > 0.95f ? 0.95f : profile->damping;
}

/* atan for x >= 0, within 1e-5 rad. Local because the game's atan2f is a
 * coarse table lookup. */
static f32 kickAtan(f32 x)
{
    const s32 invert = x > 1.0f;
    const f32 t = invert ? 1.0f / x : x;
    const f32 t2 = t * t;
    const f32 a = t * (0.9998660f + t2 * (-0.3302995f + t2 * (0.1801410f + t2 * (-0.0851330f + t2 * 0.0208351f))));
    return invert ? 1.5707963f - a : a;
}

/* Starting velocity whose free response from rest peaks at `peak`:
 * x(t) = v0 / wd * e^(-zeta w t) sin(wd t) tops out at v0 / w * gain. */
static f32 kickVelocityForPeak(f32 peak, const PortGunKickProfile *profile)
{
    const f32 zeta = kickDamping(profile);
    const f32 root = sqrtf(1.0f - zeta * zeta);
    const f32 gain = expf(-zeta * kickAtan(root / zeta) / root);
    return peak * profile->omega / gain;
}

void portGunKickImpulse(PortGunKickState *state, const PortGunKickProfile *profile)
{
    const f32 peaks[KICK_CHANNELS] = {
        profile->back * (1.0f + 0.15f * kickRandomSigned(state)),
        profile->rise,
        profile->pitch * (1.0f + 0.15f * kickRandomSigned(state)),
        profile->yaw * kickRandomSigned(state),
        profile->roll * kickRandomSigned(state),
    };
    s32 i;

    for (i = 0; i < KICK_CHANNELS; i++)
    {
        state->vel[i] += kickVelocityForPeak(peaks[i], profile);
    }

    /* Saturating, so climb approaches climbMax and never passes it. */
    if (profile->climbMax > 0.0f)
    {
        state->climbTarget += profile->climbStep * (1.0f - state->climbTarget / profile->climbMax);
    }
}

/* Exact underdamped spring update, so the motion is the same at any tick rate. */
void portGunKickStep(PortGunKickState *state, const PortGunKickProfile *profile, f32 seconds)
{
    const f32 omega = profile->omega;
    const f32 zeta = kickDamping(profile);
    const f32 decay = zeta * omega;
    const f32 wd = omega * sqrtf(1.0f - zeta * zeta);
    f32 fade, c, s;
    s32 i;

    if (seconds <= 0.0f) return;

    fade = expf(-decay * seconds);
    c = cosf(wd * seconds);
    s = sinf(wd * seconds);

    for (i = 0; i < KICK_CHANNELS; i++)
    {
        const f32 x = state->pos[i];
        const f32 v = state->vel[i];
        state->pos[i] = fade * (x * c + (v + decay * x) / wd * s);
        state->vel[i] = fade * (v * c - (decay * v + omega * omega * x) / wd * s);
    }

    state->climbTarget *= expf(-seconds / KICK_CLIMB_DECAY_S);
    state->climb += (state->climbTarget - state->climb) * (1.0f - expf(-seconds / KICK_CLIMB_FOLLOW_S));
}

void portGunKickPoseOf(const PortGunKickState *state, f32 scale, PortGunKickPose *out)
{
    out->x = 0.0f;
    out->y = state->pos[KICK_RISE] * scale;
    out->z = state->pos[KICK_BACK] * scale;
    out->pitch = (state->pos[KICK_PITCH] + state->climb) * KICK_DEG_TO_RAD * scale;
    out->yaw = state->pos[KICK_YAW] * KICK_DEG_TO_RAD * scale;
    out->roll = state->pos[KICK_ROLL] * KICK_DEG_TO_RAD * scale;
}

/* Game shell: the setting and one spring per player and hand. */
static s32 weaponKick = PORT_GUN_KICK_DEFAULT;
static PortGunKickState kickStates[4][2];

s32 portGunKickOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "weapon_kick") != 0) return FALSE;
    *value = weaponKick;
    return TRUE;
}

s32 portGunKickOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "weapon_kick") != 0) return FALSE;
    weaponKick = value < PORT_GUN_KICK_ORIGINAL ? PORT_GUN_KICK_ORIGINAL
               : value > PORT_GUN_KICK_CARTOON ? PORT_GUN_KICK_CARTOON : value;
    return TRUE;
}

static PortGunKickState *kickSlot(s32 hand, s32 item)
{
    s32 player = get_cur_playernum();
    PortGunKickState *state;

    if (player < 0 || player > 3) player = 0;
    state = &kickStates[player][hand & 1];

    if (state->item != item)
    {
        memset(state, 0, sizeof(*state));
        state->item = item;
        state->rng = 0x2545F491u * (u32) (player * 2 + (hand & 1) + 1);
    }

    return state;
}

/* The current mode's profile for `item`, NULL when the spring is off for it. */
static const PortGunKickProfile *kickMaybeActiveProfile(s32 item, PortGunKickProfile *scratch)
{
    const PortGunKickProfile *maybeProfile = portGunKickMaybeProfile(item);

    if (weaponKick == PORT_GUN_KICK_ORIGINAL || maybeProfile == NULL) return NULL;
    if (weaponKick == PORT_GUN_KICK_ENHANCED) return maybeProfile;
    portGunKickExaggerate(maybeProfile, weaponKick, scratch);
    return scratch;
}

f32 portGunKickNativeScale(s32 item)
{
    PortGunKickProfile scratch;
    return kickMaybeActiveProfile(item, &scratch) != NULL ? 0.0f : 1.0f;
}

void portGunKickFire(s32 hand, s32 item)
{
    PortGunKickProfile scratch;
    const PortGunKickProfile *maybeProfile = kickMaybeActiveProfile(item, &scratch);

    if (maybeProfile == NULL) return;
    portGunKickImpulse(kickSlot(hand, item), maybeProfile);
}

s32 portGunKickAdvance(s32 hand, s32 item, f32 fields, PortGunKickPose *out)
{
    PortGunKickProfile scratch;
    const PortGunKickProfile *maybeProfile = kickMaybeActiveProfile(item, &scratch);
    PortGunKickState *state;

    memset(out, 0, sizeof(*out));
    if (maybeProfile == NULL) return FALSE;

    state = kickSlot(hand, item);
    portGunKickStep(state, maybeProfile, fields / KICK_FIELDS_PER_S);
    portGunKickPoseOf(state, portAdsRecoilScale(item), out);
    return TRUE;
}
