import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';

export async function auditN64Layout(page, output) {
    const setting = page.getByLabel('Touch layout', { exact: true });
    const overlay = page.locator('.touch-controls');
    await expect(setting).toHaveValue('n64');
    await expect(overlay).toHaveAttribute('data-layout', 'n64');
    await expect(overlay.getByRole('button', { name: /^(Pause|Options)$/ })).toHaveCount(0);
    const buttons = [
        ['a', 'accept', 5],
        ['b', 'back', 6],
        ['z', 'fire', 0],
        ['start', 'watch', 4],
    ];
    for (const [name, action, id] of buttons) {
        const button = page.locator(`[data-controller-button=${name}]`);
        await expect(button).toHaveAttribute('data-touch-action', action);
        await button.hover();
        await page.mouse.down();
        assert.ok((await page.evaluate(() => touchFixture.snapshot())).held.includes(id));
        await page.mouse.up();
        assert.deepEqual((await page.evaluate(() => touchFixture.snapshot())).held, []);
        const style = await button.evaluate((element) => {
            const css = getComputedStyle(element),
                box = element.getBoundingClientRect();
            return {
                color: css.backgroundColor,
                radius: css.borderRadius,
                width: box.width,
                height: box.height,
            };
        });
        const [red, green, blue] = style.color.match(/\d+/g).map(Number);
        if (name === 'a') assert.ok(blue > red && blue > green, 'A must be blue');
        if (name === 'b') assert.ok(green > red && green > blue, 'B must be green');
        if (name === 'start') assert.ok(red > green && red > blue, 'Start must be red');
        if (name === 'z') {
            assert.ok(
                Math.max(red, green, blue) - Math.min(red, green, blue) < 25,
                'Z must be gray',
            );
            assert.ok(
                style.height >= 160 && style.height > style.width * 1.8 && style.radius === '8px',
                'Z must be a large upright rectangle',
            );
        } else {
            assert.equal(style.width, style.height);
            assert.equal(style.radius, '50%', `${name} must be circular`);
        }
    }
    await expect(page.getByRole('button', { name: 'Fire', exact: true })).toHaveCount(0);
    await expect(page.getByRole('button', { name: 'More', exact: true })).toHaveCount(0);
    await expect(page.getByRole('button', { name: 'Aim', exact: true })).toBeVisible();
    await expect(page.getByRole('button', { name: 'Switch weapon', exact: true })).toBeVisible();
    for (const size of [
        { width: 320, height: 568 },
        { width: 390, height: 844 },
        { width: 844, height: 390 },
        { width: 568, height: 280 },
        { width: 1024, height: 768 },
    ]) {
        await page.setViewportSize(size);
        const stick = await page.locator('.touch-stick').boundingBox();
        const cluster = await page.locator('.touch-n64').boundingBox();
        assert.ok(stick.x + stick.width <= cluster.x, 'Stick overlaps N64 buttons');
        assert.ok(stick.x >= 27, 'Missing extra left margin');
        assert.ok(size.width - cluster.x - cluster.width >= 35, 'Missing extra right margin');
        assert.ok(size.height - cluster.y - cluster.height >= 39, 'Missing extra bottom margin');
        assert.ok(size.height - stick.y - stick.height >= 39, 'Stick needs the same bottom inset');
        const [start, b, a] = await Promise.all(
            ['start', 'b', 'a'].map((name) =>
                page.locator(`[data-controller-button=${name}]`).boundingBox(),
            ),
        );
        assert.ok(
            start.y + start.height <= b.y && b.y + b.height <= a.y,
            'Start, B, A must be ordered top to bottom',
        );
        assert.ok(
            Math.abs(start.x + start.width / 2 - a.x - a.width / 2) < 1 &&
                Math.abs(b.x + b.width / 2 - a.x - a.width / 2) < 1,
            'Start, B, A must share one vertical centerline',
        );
        for (const [name] of buttons) {
            const box = await page.locator(`[data-controller-button=${name}]`).boundingBox();
            assert.ok(box.x >= stick.x + stick.width && box.y >= 0);
            assert.ok(box.x + box.width <= size.width + 1 && box.y + box.height <= size.height + 1);
            if (name === 'z')
                for (const fraction of [0.1, 0.9]) {
                    await page.mouse.move(box.x + box.width * 0.8, box.y + box.height * fraction);
                    await page.mouse.down();
                    assert.ok(
                        (await page.evaluate(() => touchFixture.snapshot())).held.includes(0),
                        'Z edge does not fire',
                    );
                    await page.mouse.up();
                }
        }
        await page.screenshot({ path: resolve(output, `n64-layout-${size.width}.png`) });
    }
    await page.setViewportSize({ width: 844, height: 390 });
    await page.evaluate(() => touchFixture.menu(true));
    await expect(overlay).toHaveAttribute('data-mode', 'menu');
    await expect(page.getByRole('button', { name: 'N64 A', exact: true })).toBeVisible();
    await expect(page.getByRole('button', { name: 'Select', exact: true })).toHaveCount(0);
    await expect(page.getByRole('button', { name: 'Aim', exact: true })).toHaveCount(0);
    await page.setViewportSize({ width: 320, height: 568 });
    for (const direction of await page.locator('.touch-dpad button').all()) {
        const pad = await direction.boundingBox();
        for (const [name] of buttons) {
            const button = await page.locator(`[data-controller-button=${name}]`).boundingBox();
            assert.ok(
                pad.x + pad.width <= button.x ||
                    pad.x >= button.x + button.width ||
                    pad.y + pad.height <= button.y ||
                    pad.y >= button.y + button.height,
                'Menu directions overlap N64 buttons on a small phone',
            );
        }
    }
    await page.setViewportSize({ width: 844, height: 390 });
    await page.evaluate(() => touchFixture.fileMenu(true));
    await expect(page.locator('.touch-n64')).toHaveCount(0);
    await expect(page.locator('.touch-menu-surface')).toBeVisible();
    await expect(page.getByRole('button', { name: 'Import Save', exact: true })).toBeVisible();
    await page.evaluate(() => touchFixture.fileMenu(false));
    await expect(page.locator('.touch-n64')).toBeVisible();
    await page.evaluate(() => touchFixture.menu(false));
    await expect(overlay).toHaveAttribute('data-mode', 'game');
    await page.getByRole('button', { name: 'Aim', exact: true }).tap();
    await page.locator('[data-controller-button=z]').evaluate((button) => {
        button.dispatchEvent(
            new PointerEvent('pointerdown', {
                pointerId: 85,
                pointerType: 'touch',
                bubbles: true,
                cancelable: true,
            }),
        );
    });
    assert.ok((await page.evaluate(() => touchFixture.snapshot())).held.includes(0));
    await page.evaluate(() => touchFixture.fileMenu(true));
    await expect(page.locator('.touch-n64')).toHaveCount(0);
    assert.deepEqual((await page.evaluate(() => touchFixture.snapshot())).held, []);
    await page.evaluate(() => touchFixture.fileMenu(false));
    await expect(page.locator('.touch-n64')).toBeVisible();
    await expect(page.getByRole('button', { name: 'Aim', exact: true })).toHaveAttribute(
        'aria-pressed',
        'false',
    );
    await setting.selectOption('modern');
    await expect(overlay).toHaveAttribute('data-layout', 'modern');
    await expect(overlay.getByRole('button', { name: /^(Pause|Options)$/ })).toHaveCount(0);
    await page.evaluate(() => {
        touchFixture.menu(true);
        touchFixture.fileMenu(true);
    });
    await expect(page.locator('.touch-menu-actions')).toHaveCount(0);
    await expect(page.locator('.touch-menu-surface')).toBeVisible();
    await page.evaluate(() => touchFixture.fileMenu(false));
    await expect(page.locator('.touch-menu-actions')).toBeVisible();
    await page.evaluate(() => touchFixture.menu(false));
    assert.deepEqual(
        (await page.evaluate(() => touchFixture.snapshot())).held,
        [],
        'Layout change left input held',
    );
    await expect(page.getByRole('button', { name: 'Aim', exact: true })).toHaveAttribute(
        'aria-pressed',
        'false',
    );
    await page.reload();
    await expect(setting).toHaveValue('modern');
    await expect(overlay).toHaveAttribute('data-layout', 'modern');
    await setting.selectOption('n64');
    await page.reload();
    await expect(setting).toHaveValue('n64');
    await setting.selectOption('modern');
    await expect(overlay).toHaveAttribute('data-layout', 'modern');
}
