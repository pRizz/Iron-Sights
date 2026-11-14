/* Sprint feedback (FORK-006): what a sprint looks like.
 *
 * - Sprint Indicator ([display] sprint_indicator, default off, HUD Additions):
 *   a pair of faint chevrons (>>) at the bottom centre of the player's view,
 *   above the weapon strip, while the sprint boost applies.
 * - Sprint FOV ([display] sprint_fov, Aiming and Movement): while sprinting
 *   the view widens by a fraction of the gameplay field of view: 0 Off,
 *   1 Normal (8%, the default), 2 Wider (16%) or 3 Crazy Wide (30%).
 *
 * Both follow one per-player fade: up over PORT_SPRINT_FADE_IN frames when
 * joy_port.c's movement scale applied the sprint boost on the last tick
 * (held Walk / Sprint or the Sprint (Toggle) latch), and back down over
 * PORT_SPRINT_FADE_OUT frames (60 Hz frames) once it stops, whatever stopped
 * it: letting go, aiming, ADS, crouching, the tank, pause or death. */
#ifndef PORT_SPRINT_FEEDBACK_H
#define PORT_SPRINT_FEEDBACK_H

#include <ultra64.h>

#define PORT_SPRINT_INDICATOR_DEFAULT 0
#define PORT_SPRINT_FOV_OFF 0
#define PORT_SPRINT_FOV_NORMAL 1
#define PORT_SPRINT_FOV_WIDER 2
#define PORT_SPRINT_FOV_CRAZY 3
#define PORT_SPRINT_FOV_DEFAULT PORT_SPRINT_FOV_NORMAL
#define PORT_SPRINT_FADE_IN 9.0f   /* 0.15 s */
#define PORT_SPRINT_FADE_OUT 15.0f /* 0.25 s */
/* Peak chevron opacity: partial, so it reads as a hint rather than a HUD row. */
#define PORT_SPRINT_INDICATOR_ALPHA 0.5f

s32 portSprintFeedbackOptionGet(const char *key, s32 *value);
s32 portSprintFeedbackOptionSet(const char *key, s32 value);
/* Per player tick (bondview2.c, beside the noclip tick). */
void portSprintFeedbackTick(void);
/* The current player's fade, 0..1. */
f32 portSprintFeedbackBlend(void);
/* Multiplier for the current player's gameplay field of view (1 = none). */
f32 portSprintFovScale(void);
/* Called from portHudDraw, which already checks the HUD is showing. */
Gfx *portSprintIndicatorDraw(Gfx *gdl);
/* A player's fade back to off (portHudReset: a new mission, a respawn, a load). */
void portSprintFeedbackReset(s32 player);

/** The fade after `ticks` 60 Hz frames towards on (`sprinting`) or off. */
static inline f32 portSprintFadeStep(f32 blend, s32 sprinting, f32 ticks)
{
    /* Snap the last sliver, which float steps of 1/9 or 1/15 leave behind. */
    if (sprinting) {
        blend += ticks / PORT_SPRINT_FADE_IN;
        return blend > 0.999f ? 1.0f : blend;
    }
    blend -= ticks / PORT_SPRINT_FADE_OUT;
    return blend < 0.001f ? 0.0f : blend;
}

/** Smoothstep, so the widening eases in and out instead of starting abruptly. */
static inline f32 portSprintEase(f32 blend)
{
    return blend * blend * (3.0f - 2.0f * blend);
}

/** The full widening for a sprint_fov value, as a fraction of the field of view. */
static inline f32 portSprintFovWiden(s32 level)
{
    switch (level) {
    case PORT_SPRINT_FOV_NORMAL: return 0.08f;
    case PORT_SPRINT_FOV_WIDER: return 0.16f;
    case PORT_SPRINT_FOV_CRAZY: return 0.30f;
    default: return 0.0f;
    }
}

static inline f32 portSprintFovScaleFor(s32 level, f32 blend)
{
    return 1.0f + portSprintFovWiden(level) * portSprintEase(blend);
}

#endif
