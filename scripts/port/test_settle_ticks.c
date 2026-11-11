/* Included by test_ads.c: a settled projectile slides the same at any tick
 * rate (FORK-070). At 60 Hz objSettle and the slide keep the inherited
 * step; above it the friction is a whole field's and the spin is the last
 * whole field's, even when the object settles on an in-between tick. */
static void testSettleTicks(void)
{
    const coord3d a = { 1.0f, 0.0f, 0.0f };
    const coord3d b = { 0.0f, 1.0f, 0.0f };
    PortSettleSpin rec = { { 0.0f, 0.0f, 0.0f }, 0 };
    coord3d prev;
    f32 step;

    /* Friction: the tick's step at 60 Hz (2 on a frame of two fields), a
     * field above it. */
    assert(portSettleFrictionStepFor(FALSE, 2.0f) == 2.0f);
    assert(portSettleFrictionStepFor(TRUE, 60.0f / 144.0f) == 1.0f);

    /* No whole-field spin seen yet: the current axis over one field. */
    portSettleSpinPick(&rec, TRUE, &b, 0.4166667f, &prev, &step);
    assert(prev.y == 1.0f && step == 1.0f);

    /* A whole-field tick records the axis it spins from; in-between ticks
     * (g_ClockTimer 0) leave it, so a settle on one sees that field. */
    portSettleSpinRecord(&rec, 1, &a);
    portSettleSpinRecord(&rec, 0, &b);
    portSettleSpinPick(&rec, TRUE, &b, 0.4166667f, &prev, &step);
    assert(prev.x == 1.0f && prev.y == 0.0f && step == 1.0f);

    /* A tick of two fields spins twice. */
    portSettleSpinRecord(&rec, 2, &b);
    portSettleSpinPick(&rec, TRUE, &a, 0.8333333f, &prev, &step);
    assert(prev.y == 1.0f && step == 2.0f);

    /* 60 Hz: exactly what the game passes, over the tick's step. */
    portSettleSpinPick(&rec, FALSE, &a, 2.0f, &prev, &step);
    assert(prev.x == 1.0f && step == 2.0f);
}
