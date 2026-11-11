/* Settled-projectile slide at raised tick rates (FORK-070).
 *
 * When a thrown object stops bouncing, objSettle (propobj.c) sets how fast it
 * turns flat from how far it spun over the last tick, divided by the tick's
 * step, and objectTick then slides it a whole field per pass with friction
 * unk94 scaled by the step. The spin and the slide only advance on
 * whole-field ticks (g_ClockTimer), so above 60 Hz a tick's fraction made the
 * friction too weak (a grenade slid 2.4 times as far after turning at
 * 144 Hz) and a settle on an in-between tick saw no spin (the slowest turn,
 * so a longer frictionless slide). This keeps both per field, as at 60 Hz;
 * at 60 Hz nothing changes. */
#ifndef PORT_SETTLE_TICKS_H
#define PORT_SETTLE_TICKS_H

#include <ultra64.h>
#include <bondtypes.h>

/* A projectile's x axis before its last whole-field spin, and the fields
 * that spin covered (0: none seen yet). */
typedef struct PortSettleSpin {
    coord3d before;
    s32 fields;
} PortSettleSpin;

/* The friction step a slide pass uses: one field above 60 Hz. */
static inline f32 portSettleFrictionStepFor(s32 raised, f32 timerDelta)
{
    return raised ? 1.0f : timerDelta;
}

/* At the start of an airborne tick: a tick that spins it (g_ClockTimer > 0)
 * records the axis it spins from. */
static inline void portSettleSpinRecord(PortSettleSpin *rec, s32 clockTimer, const coord3d *axis)
{
    if (clockTimer <= 0) return;
    rec->before = *axis;
    rec->fields = clockTimer;
}

/* objSettle's previous axis and step. Above 60 Hz they are the last
 * whole-field spin's (or, before any, the current axis over one field). */
static inline void portSettleSpinPick(const PortSettleSpin *rec, s32 raised, const coord3d *axis, f32 timerDelta,
                                      coord3d *prev, f32 *step)
{
    if (!raised)
    {
        *prev = *axis;
        *step = timerDelta;
    }
    else if (rec->fields > 0)
    {
        *prev = rec->before;
        *step = (f32) rec->fields;
    }
    else
    {
        *prev = *axis;
        *step = 1.0f;
    }
}

/* The game side (port_settle_ticks.c). */
f32 portSettleFrictionStep(void);
void portSettleSpinNote(Projectile *projectile, const coord3d *axisBefore);
void portSettleSpinPrevious(Projectile *projectile, const coord3d *axis, coord3d *prev, f32 *step);

#endif
