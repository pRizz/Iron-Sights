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
        await page.locator('[data-touch-action=fire]').evaluate((button) =>
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
