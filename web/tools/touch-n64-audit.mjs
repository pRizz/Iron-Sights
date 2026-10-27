import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';

export async function auditN64Layout(page, output) {
    const setting = page.getByLabel('Touch layout', { exact: true });
    const overlay = page.locator('.touch-controls');
    await expect(setting).toHaveValue('n64');
    await expect(overlay).toHaveAttribute('data-layout', 'n64');
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
                style.width > style.height && style.radius === '8px',
                'Z must be rectangular',
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
        { width: 1024, height: 768 },
    ]) {
        await page.setViewportSize(size);
        const stick = await page.locator('.touch-stick').boundingBox();
        const cluster = await page.locator('.touch-n64').boundingBox();
        assert.ok(stick.x + stick.width <= cluster.x, 'Stick overlaps N64 buttons');
        for (const [name] of buttons) {
            const box = await page.locator(`[data-controller-button=${name}]`).boundingBox();
            assert.ok(box.x >= size.width / 2 && box.y >= 0);
            assert.ok(box.x + box.width <= size.width + 1 && box.y + box.height <= size.height + 1);
        }
        await page.screenshot({ path: resolve(output, `n64-layout-${size.width}.png`) });
    }
    await page.setViewportSize({ width: 844, height: 390 });
    await page.evaluate(() => touchFixture.menu(true));
    await expect(overlay).toHaveAttribute('data-mode', 'menu');
    await expect(page.getByRole('button', { name: 'N64 A', exact: true })).toBeVisible();
    await expect(page.getByRole('button', { name: 'Select', exact: true })).toHaveCount(0);
    await expect(page.getByRole('button', { name: 'Aim', exact: true })).toHaveCount(0);
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
    await setting.selectOption('modern');
    await expect(overlay).toHaveAttribute('data-layout', 'modern');
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
