import assert from 'node:assert/strict';
import { expect } from '@playwright/test';

export async function auditTouchContext(page) {
    const overlay = page.locator('.touch-controls');
    for (const layout of ['n64', 'modern']) {
        await page.evaluate((value) => touchFixture.layout(value), layout);
        for (const [native, name] of [
            [0, 'navigation'],
            [1, 'cinematic'],
        ]) {
            await page.evaluate((value) => touchFixture.context(value), native);
            await expect(overlay).toHaveAttribute('data-context', name);
            await expect(overlay.locator('button')).toHaveCount(0);
            await expect(page.locator('.touch-stick')).toHaveCount(0);
            await expect(page.locator('.touch-menu-surface')).toBeVisible();
            if (name === 'cinematic') {
                await expect(page.locator('.touch-menu-hint')).toHaveCount(0);
                await expect(page.locator('.touch-game-hint')).toHaveCount(0);
            }
        }
        // An open slider edit: -/+ step it, a sideways drag scrubs it, a tap
        // keeps it, and Done/Cancel send A/B. The cursor surface stays live.
        await page.evaluate(() => touchFixture.context(4));
        await expect(overlay).toHaveAttribute('data-context', 'edit');
        await expect(page.locator('.touch-stick')).toHaveCount(0);
        await expect(page.locator('.touch-menu-hint')).toContainText('Drag sideways');
        const before = await page.evaluate(() => touchFixture.snapshot());
        await page.locator('[data-touch-step="1"]').tap();
        await page.locator('[data-touch-step="1"]').tap();
        await page.locator('[data-touch-step="-1"]').tap();
        assert.equal((await page.evaluate(() => touchFixture.snapshot())).steps, before.steps + 1);
        const surface = page.locator('.touch-menu-surface');
        const send = (type, clientX, target = surface) =>
            target.evaluate(
                (element, [eventType, x]) =>
                    element.dispatchEvent(
                        new PointerEvent(eventType, {
                            pointerId: 903,
                            pointerType: 'touch',
                            bubbles: true,
                            cancelable: true,
                            clientX: x,
                            clientY: 200,
                        }),
                    ),
                [type, clientX],
            );
        await send('pointerdown', 300);
        await send('pointermove', 400);
        await send('pointerup', 400);
        const dragged = await page.evaluate(() => touchFixture.snapshot());
        assert.equal(dragged.steps, before.steps + 1 + 5);
        assert.equal(dragged.accepts, before.accepts, 'a scrub must not keep the edit');
        await send('pointerdown', 300);
        await send('pointerup', 302);
        assert.equal(
            (await page.evaluate(() => touchFixture.snapshot())).accepts,
            before.accepts + 1,
        );
        await page.locator('[data-touch-action=accept]').tap();
        await page.locator('[data-touch-action=back]').tap();
        const ended = await page.evaluate(() => touchFixture.snapshot());
        assert.equal(ended.accepts, before.accepts + 2);
        assert.ok(ended.presses.slice(before.presses.length).includes(6), 'Cancel sends B');
        await page.evaluate(() => touchFixture.context(0));
        await expect(page.locator('[data-touch-step]')).toHaveCount(0);

        // An open pick list: ▲/▼ queue three rows each, taps still pick by the
        // cursor, and Cancel sends B to close it.
        await page.evaluate(() => touchFixture.context(5));
        await expect(overlay).toHaveAttribute('data-context', 'list');
        await expect(page.locator('.touch-menu-surface')).toBeVisible();
        await expect(page.locator('.touch-menu-hint')).toContainText('▲ ▼ scroll');
        const listed = await page.evaluate(() => touchFixture.snapshot());
        await page.locator('[data-touch-step="3"]').tap();
        await page.locator('[data-touch-step="3"]').tap();
        await page.locator('[data-touch-step="-3"]').tap();
        await send('pointerdown', 300);
        await send('pointerup', 301);
        await page.locator('[data-touch-action=back]').tap();
        const picked = await page.evaluate(() => touchFixture.snapshot());
        assert.equal(picked.steps, listed.steps + 3);
        assert.equal(picked.accepts, listed.accepts + 1, 'a tap picks the hovered entry');
        assert.ok(picked.presses.slice(listed.presses.length).includes(6), 'Cancel sends B');

        // A bind capture and an Input Device wait show only their buttons: no
        // cursor surface, whose tap would land on the row being bound.
        for (const [native, name, unbind] of [
            [6, 'capture', true],
            [7, 'assign', false],
        ]) {
            await page.evaluate((value) => touchFixture.context(value), native);
            await expect(overlay).toHaveAttribute('data-context', name);
            await expect(page.locator('.touch-menu-surface')).toHaveCount(0);
            await expect(page.locator('[data-touch-step]')).toHaveCount(0);
            await expect(page.locator('[data-touch-request=unbind]')).toHaveCount(unbind ? 1 : 0);
            const waiting = await page.evaluate(() => touchFixture.snapshot());
            if (unbind) await page.locator('[data-touch-request=unbind]').tap();
            await page.locator('[data-touch-action=back]').tap();
            const answered = await page.evaluate(() => touchFixture.snapshot());
            assert.equal(answered.unbinds, waiting.unbinds + (unbind ? 1 : 0));
            assert.equal(answered.accepts, waiting.accepts, 'waiting never confirms a row');
            assert.ok(answered.presses.slice(waiting.presses.length).includes(6), 'Cancel sends B');
        }
        await page.evaluate(() => touchFixture.context(0));
        await expect(page.locator('[data-touch-request]')).toHaveCount(0);

        // A hold-to-set screen (the 007 sliders): a resting finger holds A after
        // the cursor arrives and releases it on lift; a quick tap is one pulse.
        await page.evaluate(() => touchFixture.context(8));
        await expect(overlay).toHaveAttribute('data-context', 'hold');
        await expect(page.locator('.touch-menu-hint')).toContainText('drag along it');
        const resting = await page.evaluate(() => touchFixture.snapshot());
        await send('pointerdown', 300);
        assert.ok(!(await page.evaluate(() => touchFixture.snapshot())).held.includes(5));
        await page.waitForTimeout(200);
        assert.ok((await page.evaluate(() => touchFixture.snapshot())).held.includes(5));
        await send('pointermove', 420);
        await send('pointerup', 420);
        const lifted = await page.evaluate(() => touchFixture.snapshot());
        assert.ok(!lifted.held.includes(5), 'lifting releases A');
        assert.equal(lifted.accepts, resting.accepts + 1, 'one held press, no extra tap');
        assert.equal(lifted.cursor[0], 420, 'the cursor follows the drag');
        await send('pointerdown', 300);
        await send('pointerup', 301);
        await page.waitForTimeout(200);
        const tapped = await page.evaluate(() => touchFixture.snapshot());
        assert.equal(tapped.accepts, lifted.accepts + 1);
        assert.ok(!tapped.held.includes(5), 'a quick tap never starts a hold');

        // A pad-only screen (multiplayer Player Options, Health): no cursor
        // surface, a D-pad plus A, B and Start, each its own native button.
        await page.evaluate(() => touchFixture.context(9));
        await expect(overlay).toHaveAttribute('data-context', 'pad');
        await expect(page.locator('.touch-menu-surface')).toHaveCount(0);
        await expect(page.locator('.touch-dpad button')).toHaveCount(4);
        const padBefore = await page.evaluate(() => touchFixture.snapshot());
        for (const action of ['up', 'right', 'accept', 'back', 'watch'])
            await page.locator(`[data-touch-action=${action}]`).tap();
        const padAfter = await page.evaluate(() => touchFixture.snapshot());
        assert.deepEqual(padAfter.presses.slice(padBefore.presses.length), [15, 18, 5, 6, 4]);
        await page.evaluate(() => touchFixture.context(0));
        await expect(page.locator('.touch-dpad')).toHaveCount(0);

        // A name keyboard: the letter grid stays tappable, and Phone keyboard
        // opens a docked field that rewrites the whole name, keeps typing away
        // from the game's key bindings, and answers Done or Cancel.
        await page.evaluate(() => touchFixture.context(10));
        await expect(overlay).toHaveAttribute('data-context', 'text');
        await expect(page.locator('.touch-menu-surface')).toBeVisible();
        const launch = page.getByRole('button', { name: 'Phone keyboard' });
        const textField = page.locator('#touch-text-field');
        await expect(page.locator('form.touch-text')).toBeHidden();
        await launch.tap();
        await expect(page.locator('form.touch-text')).toBeVisible();
        await expect(page.locator('.touch-text-title')).toHaveText('SAVE SET AS');
        await expect(textField).toBeFocused();
        await expect(launch).toHaveCount(0);
        const typedFrom = await page.evaluate(() => touchFixture.snapshot());
        await textField.fill(''); // a later layout pass opens on the name saved earlier
        await textField.pressSequentially('Gold|en');
        await expect(textField).toHaveValue('Golden');
        await textField.fill('Golden Gun');
        const typed = await page.evaluate(() => touchFixture.snapshot());
        assert.equal(typed.nameText, 'Golden Gun', 'the whole name is rewritten');
        assert.equal(typed.windowKeys, typedFrom.windowKeys, 'typing never reaches the game');
        await page.locator('form.touch-text button[type=submit]').tap();
        assert.equal((await page.evaluate(() => touchFixture.snapshot())).nameFinish, 1);
        await expect(page.locator('form.touch-text')).toBeHidden();
        await launch.tap();
        await expect(textField).toHaveValue('Golden Gun');
        await page.locator('form.touch-text button[type=button]').tap();
        assert.equal((await page.evaluate(() => touchFixture.snapshot())).nameFinish, 0);
        await launch.tap();
        await textField.press('Enter');
        assert.equal((await page.evaluate(() => touchFixture.snapshot())).nameFinish, 1);
        await page.evaluate(() => touchFixture.context(0));
        await expect(launch).toHaveCount(0);
        await expect(page.locator('form.touch-text')).toHaveCount(0);

        // A menu gesture held over a camera transition must not select/skip
        // on release in the new context.
        await page.evaluate(() => touchFixture.context(0));
        await expect(overlay).toHaveAttribute('data-context', 'navigation');
        const accepts = (await page.evaluate(() => touchFixture.snapshot())).accepts;
        await page.locator('.touch-menu-surface').evaluate((surface) =>
            surface.dispatchEvent(
                new PointerEvent('pointerdown', {
                    pointerId: 901,
                    pointerType: 'touch',
                    bubbles: true,
                    cancelable: true,
                    clientX: 400,
                    clientY: 150,
                }),
            ),
        );
        await page.evaluate(() => touchFixture.context(1));
        await expect(overlay).toHaveAttribute('data-context', 'cinematic');
        await page.evaluate(() =>
            document.dispatchEvent(
                new PointerEvent('pointerup', {
                    pointerId: 901,
                    clientX: 400,
                    clientY: 150,
                }),
            ),
        );
        assert.equal((await page.evaluate(() => touchFixture.snapshot())).accepts, accepts);

        await page.evaluate(() => touchFixture.context(2));
        await expect(overlay).toHaveAttribute('data-context', 'game');
        await expect(page.locator('.touch-stick')).toBeVisible();
        const group = page.locator(layout === 'n64' ? '.touch-n64' : '.touch-actions');
        await expect(group).toHaveCSS('animation-name', 'touch-hud-enter');
        await expect(group).toHaveCSS('opacity', '1');
        await page.locator('[data-touch-action=aim]').tap();
        await page
            .locator('[data-touch-action=fire]:not([data-touch-side=left])')
            .evaluate((button) =>
                button.dispatchEvent(
                    new PointerEvent('pointerdown', {
                        pointerId: 902,
                        pointerType: 'touch',
                        bubbles: true,
                        cancelable: true,
                    }),
                ),
            );
        await page.evaluate(() => touchFixture.context(1));
        await expect(overlay.locator('button')).toHaveCount(0);
        assert.deepEqual((await page.evaluate(() => touchFixture.snapshot())).held, []);
        await page.evaluate(() => touchFixture.context(3));
        await expect(overlay).toHaveAttribute('data-context', 'watch');
        await expect(page.locator('[data-touch-action=watch]')).toBeVisible();
        await expect(page.locator('.touch-dpad')).toBeVisible();
    }
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await page.evaluate(() => touchFixture.context(2));
    await expect(page.locator('.touch-actions')).toHaveCSS('animation-name', 'none');
    await expect(page.locator('.touch-actions')).toHaveCSS('opacity', '1');
    await page.emulateMedia({ reducedMotion: 'no-preference' });
    await page.evaluate(() => touchFixture.layout('n64'));
}
