// Double-Tap Fire (FORK-079) on the look surface, through real overlay
// handlers and document-level release routing; fake native boundary only.
import assert from 'node:assert/strict';

const FIRE = 0;

export async function auditDoubleTapFire(page) {
    const snapshot = () => page.evaluate(() => touchFixture.snapshot());
    const look = page.locator('.touch-look');
    const body = page.locator('body');
    const send = (target, type, id, x, y) =>
        target.dispatchEvent(type, {
            pointerId: id,
            pointerType: 'touch',
            bubbles: true,
            cancelable: true,
            clientX: x,
            clientY: y,
        });
    const { width, height } = page.viewportSize();
    const x = Math.round(width * 0.5),
        y = Math.round(height * 0.3);
    const tap = async (id, atX = x, atY = y) => {
        await send(look, 'pointerdown', id, atX, atY);
        await send(body, 'pointerup', id, atX, atY);
    };
    const fires = async () => (await snapshot()).presses.filter((id) => id === FIRE).length;
    // Event timestamps are real time; the page clock is faked, so wait here.
    const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
    const settle = () => pause(400);

    await page.evaluate(() => touchFixture.doubleTapFire(true));
    await settle();
    let before = await fires();
    await tap(901);
    assert.equal(await fires(), before, 'A single look tap fired');
    await send(look, 'pointerdown', 902, x + 12, y + 6);
    let current = await snapshot();
    assert.ok(current.held.includes(FIRE), 'Tap then touch did not hold Fire');
    assert.equal(await fires(), before + 1, 'Double tap fired more than once');
    const aim = current.look[0];
    await send(body, 'pointermove', 902, x + 80, y + 6);
    current = await snapshot();
    assert.ok(current.look[0] > aim + 1, 'The firing finger stopped aiming');
    assert.ok(current.held.includes(FIRE), 'Dragging the firing finger released Fire');
    await send(body, 'pointerup', 902, x + 80, y + 6);
    assert.ok(!(await snapshot()).held.includes(FIRE), 'Lifting the finger left Fire held');
    // The firing touch never arms a third tap.
    await send(look, 'pointerdown', 903, x + 80, y + 6);
    assert.ok(!(await snapshot()).held.includes(FIRE), 'A third quick touch fired again');
    await send(body, 'pointerup', 903, x + 80, y + 6);

    await settle();
    before = await fires();
    await send(look, 'pointerdown', 904, x, y);
    await send(body, 'pointermove', 904, x + 60, y);
    await send(body, 'pointerup', 904, x + 60, y);
    await send(look, 'pointerdown', 905, x + 60, y);
    assert.equal(await fires(), before, 'A look swipe and quick re-touch fired');
    await send(body, 'pointerup', 905, x + 60, y);

    await settle();
    await tap(906);
    await send(look, 'pointerdown', 907, x + 120, y);
    assert.equal(await fires(), before, 'A second touch far from the tap fired');
    await send(body, 'pointerup', 907, x + 120, y);

    await settle();
    await tap(908);
    await pause(450);
    await send(look, 'pointerdown', 909, x, y);
    assert.equal(await fires(), before, 'A slow second touch fired');
    await send(body, 'pointerup', 909, x, y);

    await settle();
    await page.evaluate(() => touchFixture.doubleTapFire(false));
    await tap(910);
    await send(look, 'pointerdown', 911, x, y);
    assert.equal(await fires(), before, 'Double-Tap Fire off still fired');
    await send(body, 'pointerup', 911, x, y);
    await page.evaluate(() => touchFixture.doubleTapFire(true));
    await settle();
}
