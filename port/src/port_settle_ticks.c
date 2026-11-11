/* Settled-projectile slide at raised tick rates (FORK-070); see
 * port_settle_ticks.h. One record per g_Projectiles slot. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chrai.h"
#include "game/frametiming.h"
#include "port_settle_ticks.h"

extern s32 g_ClockTimer;
extern f32 g_GlobalTimerDelta;

static PortSettleSpin sSpin[PROJECTILES_ARR_MAX];

static PortSettleSpin *spinRecord(Projectile *projectile)
{
    s32 i = (s32) (projectile - g_Projectiles);

    return (projectile != NULL && i >= 0 && i < PROJECTILES_ARR_MAX) ? &sSpin[i] : NULL;
}

f32 portSettleFrictionStep(void)
{
    return portSettleFrictionStepFor(portTickHz() != 60, g_GlobalTimerDelta);
}

/* objectTick's airborne branch, where it reads the axis before the spin. */
void portSettleSpinNote(Projectile *projectile, const coord3d *axisBefore)
{
    PortSettleSpin *rec = spinRecord(projectile);

    if (rec != NULL) portSettleSpinRecord(rec, g_ClockTimer, axisBefore);
}

/* objSettle: the previous axis and step it measures the spin with. */
void portSettleSpinPrevious(Projectile *projectile, const coord3d *axis, coord3d *prev, f32 *step)
{
    static const PortSettleSpin none = { { 0.0f, 0.0f, 0.0f }, 0 };
    PortSettleSpin *rec = spinRecord(projectile);

    portSettleSpinPick((rec != NULL) ? rec : &none, portTickHz() != 60, axis, g_GlobalTimerDelta, prev, step);
    if (rec != NULL) rec->fields = 0;   /* settled: the next throw from this slot starts afresh */
}
