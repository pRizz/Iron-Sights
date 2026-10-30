import assert from 'node:assert/strict';
import { expect } from '@playwright/test';

export async function auditTouchButtons(page) {
    const snapshot = () => page.evaluate(() => touchFixture.snapshot());
    const send = (locator, type, id, x, y) =>
        locator.dispatchEvent(type, {
            pointerId: id,
            pointerType: 'touch',
            bubbles: true,
            cancelable: true,
            clientX: x,
            clientY: y,
        });
    for (const layout of ['n64', 'modern']) {
        await page.evaluate((value) => touchFixture.layout(value), layout);
        await expect(page.locator('.touch-controls')).toHaveAttribute('data-layout', layout);
        const actions =
            layout === 'n64'
                ? ['accept', 'back', 'watch', 'aim', 'nextGadget']
                : ['aim', 'reload', 'use', 'crouch', 'watch', 'nextGadget'];
        for (const action of actions) {
            const button = page.locator(`[data-touch-action=${action}]`);
            const box = await button.boundingBox();
            const x = box.x + box.width / 2,
                y = box.y + box.height / 2;
            const before = await snapshot();
            await send(button, 'pointerdown', 880, x, y);
            assert.equal(
                (await snapshot()).presses.length,
                before.presses.length,
                'A button activated before tap-up',
            );
            // Deliberately lose capture and deliver movement on the document.
            await send(page.locator('body'), 'pointermove', 880, x + 45, y + 15);
            assert.ok((await snapshot()).look[0] > before.look[0] + 1, `${action} blocked looking`);
            await send(page.locator('body'), 'pointermove', 880, x, y);
            await send(page.locator('body'), 'pointerup', 880, x, y);
            assert.equal(
                (await snapshot()).presses.length,
                before.presses.length,
                `Swiping ${action} also activated it`,
            );
            await expect(page.locator('[data-touch-action=aim]')).toHaveAttribute(
                'aria-pressed',
                'false',
            );
        }
        const left = page.getByRole('button', { name: 'Left Z (fire)', exact: true });
        const right = page.locator('[data-touch-action=fire]:not([data-touch-side=left])');
        const l = await left.boundingBox(),
            r = await right.boundingBox();
        const before = await snapshot();
        await send(left, 'pointerdown', 881, l.x + 24, l.y + 24);
        await send(page.locator('body'), 'pointermove', 881, l.x + 50, l.y + 30);
        assert.deepEqual(
            (await snapshot()).look,
            before.look,
            'Left Z changed the right finger aim',
        );
        await send(right, 'pointerdown', 882, r.x + 24, r.y + 24);
        await send(page.locator('body'), 'pointermove', 882, r.x + 48, r.y + 24);
        assert.ok((await snapshot()).look[0] > before.look[0] + 1, 'Right Fire lost drag-look');
        await send(page.locator('body'), 'pointerup', 881, l.x + 50, l.y + 30);
        assert.ok((await snapshot()).held.includes(0), 'Releasing left Z released right Fire');
        await send(page.locator('body'), 'pointerup', 882, r.x + 48, r.y + 24);
        assert.deepEqual((await snapshot()).held, []);
        if (layout === 'modern') {
            const more = page.getByRole('button', { name: 'More', exact: true });
            const box = await more.boundingBox();
            await send(more, 'pointerdown', 883, box.x + 24, box.y + 24);
            await send(page.locator('body'), 'pointermove', 883, box.x - 30, box.y + 24);
            await send(page.locator('body'), 'pointerup', 883, box.x - 30, box.y + 24);
            await expect(page.locator('.touch-extra')).toHaveCount(0);
            await more.tap();
            const secondary = page.locator('[data-touch-action=nextWeapon]');
            const target = await secondary.boundingBox();
            const count = (await snapshot()).presses.length;
            await send(secondary, 'pointerdown', 884, target.x + 24, target.y + 24);
            await more.tap();
            await expect(secondary).toHaveCount(0);
            await send(page.locator('body'), 'pointerup', 884, target.x + 24, target.y + 24);
            assert.equal(
                (await snapshot()).presses.length,
                count,
                'A removed More action activated on release',
            );
        }
    }
    await page.evaluate(() => touchFixture.layout('modern'));
}
