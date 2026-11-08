import assert from 'node:assert/strict';
import { expect } from '@playwright/test';

const GAP = 8;

// Corner ammo stays put; the gameplay buttons over it rise until they clear
// its top, never drop back for a shorter icon, and rest again outside play.
export async function auditHudClearance(page) {
    const overlay = page.locator('.touch-controls');
    const layout = await overlay.getAttribute('data-layout');
    const snapshot = () => page.evaluate(() => touchFixture.snapshot());
    const bottom = (selector) =>
        page.locator(selector).evaluate((element) => element.getBoundingClientRect().bottom);
    const rect = (selector) =>
        page.locator(selector).evaluate((element) => element.getBoundingClientRect().toJSON());
    const viewport = { width: 1024, height: 768 };
    // The runtime reports canvas fractions; the fixture's canvas is a small box.
    const fractions = (left, right, top, bottomEdge) =>
        page.evaluate(
            ([l, r, t, b]) => {
                const canvas = document.querySelector('canvas').getBoundingClientRect();
                const x = (value) => (value - canvas.left) / canvas.width;
                const y = (value) => (value - canvas.top) / canvas.height;
                return [x(l), x(r), y(t), y(b)];
            },
            [left, right, top, bottomEdge],
        );
    // An ammo row whose top is at y CSS pixels, reaching under a control group.
    const under = async (selector, top) => {
        const box = await rect(selector);
        return fractions(box.left - 20, box.left + 60, top, top + 40);
    };
    const ammo = (side, box) => page.evaluate(([s, b]) => touchFixture.ammo(s, b), [side, box]);
    await page.setViewportSize(viewport);
    await page.evaluate(() => touchFixture.layout('n64'));
    await page.evaluate(() => touchFixture.context(2));
    await expect(overlay).toHaveAttribute('data-context', 'game');
    await expect.poll(async () => (await snapshot()).corners).toEqual([true, true]);
    const rest = await bottom('.touch-n64');
    await ammo(1, await under('.touch-n64', 690));
    await expect.poll(() => bottom('.touch-n64')).toBeLessThanOrEqual(690 - GAP);
    assert.ok((await bottom('.touch-n64')) > 690 - GAP - 1.5, 'N64 cluster rose too far');
    // A hidden counter or shorter icon never drops the settled cluster.
    await page.waitForTimeout(800);
    await ammo(1, undefined);
    await page.waitForTimeout(300);
    assert.ok((await bottom('.touch-n64')) <= 690 - GAP, 'cluster fell back');
    // The stick and the left Z row rise together above a dual-wield counter.
    const leftActions = await bottom('.touch-left-actions');
    const stickRest = await bottom('.touch-stick');
    await ammo(0, await under('.touch-stick', 705));
    await expect.poll(() => bottom('.touch-stick')).toBeLessThanOrEqual(705 - GAP);
    const raised = stickRest - (await bottom('.touch-stick'));
    assert.ok(Math.abs(leftActions - raised - (await bottom('.touch-left-actions'))) < 1);
    await ammo(0, undefined);
    // A new layout is measured afresh: Modern actions rest until ammo is under them.
    await page.evaluate(() => touchFixture.layout('modern'));
    await expect(overlay).toHaveAttribute('data-layout', 'modern');
    await expect
        .poll(() => bottom('.touch-actions'), { message: 'Modern actions kept a stale lift' })
        .toBeCloseTo(rest, 0);
    await ammo(1, await under('.touch-actions', 675));
    await expect.poll(() => bottom('.touch-actions')).toBeLessThanOrEqual(675 - GAP);
    // Ammo beside the controls, as in a phone's 4:3 band, moves nothing.
    await page.setViewportSize({ width: 844, height: 390 });
    const actions = await rect('.touch-actions');
    await ammo(1, await fractions(actions.left - 120, actions.left - 10, 340, 380));
    await page.waitForTimeout(800);
    assert.ok(Math.abs((await bottom('.touch-actions')) - (390 - 40)) < 2, 'beside ammo lifted');
    // Leaving play uncovers both corners and the controls come back to rest.
    await page.evaluate(() => touchFixture.context(0));
    await expect(overlay).toHaveAttribute('data-context', 'navigation');
    await expect.poll(async () => (await snapshot()).corners).toEqual([false, false]);
    await ammo(1, undefined);
    await page.evaluate((next) => touchFixture.layout(next), layout);
    await page.evaluate(() => touchFixture.context(2));
    await expect(overlay).toHaveAttribute('data-context', 'game');
    await expect.poll(async () => (await snapshot()).corners).toEqual([true, true]);
}
