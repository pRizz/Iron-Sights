// Touch Controls Opacity (FORK-080): the gameplay controls rest at the
// setting's strength and show at 80% or more under a finger; menus keep full
// strength. Real overlay handlers and CSS; fake native boundary only.
import assert from 'node:assert/strict';

export async function auditTouchOpacity(page) {
    const fire = page.locator('[data-touch-action=fire]:not([data-touch-side=left])');
    const stick = page.locator('.touch-stick');
    const body = page.locator('body');
    const opacity = (target) =>
        target.evaluate((element) => Number(getComputedStyle(element).opacity));
    const touch = (target, type, id) =>
        target.dispatchEvent(type, {
            pointerId: id,
            pointerType: 'touch',
            bubbles: true,
            cancelable: true,
        });
    // The resting fade eases over 160 ms; event times are real, so wait here.
    const settle = () => new Promise((resolve) => setTimeout(resolve, 350));
    const near = (actual, expected, message) =>
        assert.ok(Math.abs(actual - expected) < 0.01, `${message}: ${actual}`);

    await page.evaluate(() => touchFixture.controlsOpacity(50));
    await settle();
    near(await opacity(fire), 0.5, 'Fire did not rest at the 50% default');
    near(await opacity(stick), 0.5, 'The stick did not rest at the 50% default');

    await touch(fire, 'pointerdown', 951);
    near(await opacity(fire), 0.8, 'A touched Fire button did not show at 80%');
    near(await opacity(stick), 0.5, 'An untouched control brightened with Fire');
    await touch(body, 'pointerup', 951);
    await settle();
    near(await opacity(fire), 0.5, 'Fire stayed bright after the finger lifted');

    await touch(stick, 'pointerdown', 952);
    near(await opacity(stick), 0.8, 'A touched stick did not show at 80%');
    await touch(body, 'pointercancel', 952);
    await settle();
    near(await opacity(stick), 0.5, 'A cancelled stick touch stayed bright');

    await page.evaluate(() => touchFixture.controlsOpacity(100));
    await settle();
    near(await opacity(fire), 1, 'Fire did not follow a 100% setting');
    await touch(fire, 'pointerdown', 953);
    near(await opacity(fire), 1, 'A touched control dimmed below a 100% setting');
    await touch(body, 'pointerup', 953);

    await page.evaluate(() => {
        touchFixture.controlsOpacity(25);
        touchFixture.menu(true);
    });
    await settle();
    const menuButton = page.locator('.touch-controls button').first();
    near(await opacity(menuButton), 1, 'A menu control faded with the gameplay setting');
    await page.evaluate(() => {
        touchFixture.menu(false);
        touchFixture.controlsOpacity(50);
    });
    await settle();
}
