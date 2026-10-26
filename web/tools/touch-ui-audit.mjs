// ROM-free actual pointer-capture/multi-contact checks; fake native boundary only.
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import assert from 'node:assert/strict';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

const assets = new Map(
    await Promise.all(
        [
            ['/', '../tests/touch.html', 'text/html'],
            ['/driver.js', '../../build/web-touch-fixture/driver.js', 'text/javascript'],
            ['/driver.css', '../../build/web-touch-fixture/driver.css', 'text/css'],
        ].map(async ([path, file, type]) => [
            path,
            { bytes: await readFile(new URL(file, import.meta.url)), type },
        ]),
    ),
);
const server = createServer((request, response) => {
    const asset = assets.get(request.url);
    response.writeHead(asset ? 200 : 404, { 'Content-Type': asset?.type || 'text/plain' });
    response.end(asset?.bytes || 'Not found');
});
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const { browser, kind } = await launchAuditBrowser();
const output = resolve(process.env.MIDAS_AUDIT_OUTPUT || '../build/web-touch/ui');
await mkdir(output, { recursive: true });
try {
    const context = await browser.newContext({
        viewport: { width: 844, height: 390 },
        isMobile: true,
        hasTouch: true,
    });
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', (error) => errors.push(String(error)));
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-mode', 'game');
    const aim = page.locator('[data-touch-action=aim]');
    await aim.tap();
    await expect(aim).toHaveAttribute('aria-pressed', 'true');
    assert.ok((await page.evaluate(() => touchFixture.snapshot())).held.includes(1));
    await aim.tap();
    await expect(aim).toHaveAttribute('aria-pressed', 'false');
    const snapshot = () => page.evaluate(() => touchFixture.snapshot());
    const weapon = page.getByRole('button', { name: 'Switch weapon', exact: true });
    await expect(weapon.locator('svg')).toBeVisible();
    // The primary icon uses the native next-weapon action, with independent
    // press/release ownership on every tap, without opening More.
    for (let tap = 0; tap < 2; tap++) {
        await weapon.hover();
        await page.mouse.down();
        assert.ok((await snapshot()).held.includes(9));
        await page.mouse.up();
        assert.ok(!(await snapshot()).held.includes(9));
    }
    await expect(page.locator('.touch-extra')).toHaveCount(0);
    if (kind === 'chromium') {
        const session = await context.newCDPSession(page);
        const fingers = new Map();
        const update = async (id, x, y, down) => {
            const exists = fingers.has(id);
            const ending = fingers.get(id);
            down ? fingers.set(id, { id, x, y }) : fingers.delete(id);
            await session.send('Input.dispatchTouchEvent', {
                type: down ? (exists ? 'touchMove' : 'touchStart') : 'touchEnd',
                touchPoints: down ? [...fingers.values()] : [ending],
            });
        };
        const stick = await page.locator('.touch-stick').boundingBox();
        const fire = await page.locator('[data-touch-action=fire]').boundingBox();
        const look = await page.locator('.touch-look').boundingBox();
        await update(1, stick.x + stick.width / 2, stick.y + 15, true);
        await update(2, fire.x + 24, fire.y + 24, true);
        await update(3, look.x + 40, look.y + 40, true);
        await update(3, look.x + 95, look.y + 40, true);
        let value = await snapshot();
        assert.ok(value.movement[1] > 0.5 && value.held.includes(0) && value.look[0] > 1);
        await update(2, 0, 0, false);
        value = await snapshot();
        assert.ok(
            !value.held.includes(0) && value.movement[1] > 0.5,
            'Finger release affected another control: ' + JSON.stringify(value),
        );
        await session.send('Input.dispatchTouchEvent', { type: 'touchCancel', touchPoints: [] });
        value = await snapshot();
        assert.deepEqual(value.movement, [0, 0]);
        assert.deepEqual(value.held, []);
        fingers.clear();
        await update(1, fire.x + 24, fire.y + 24, true);
        await page.evaluate(() => window.dispatchEvent(new Event('blur')));
        assert.deepEqual((await snapshot()).held, [], 'Focus loss left fire held');
        await session.send('Input.dispatchTouchEvent', { type: 'touchCancel', touchPoints: [] });
    }
    await page.evaluate(() => touchFixture.menu(true));
    await expect(page.locator('[data-touch-action=accept]')).toBeVisible();
    await expect(weapon).toHaveCount(0);
    await page.evaluate(() => touchFixture.menu(false));
    await expect(page.locator('[data-touch-action=fire]')).toBeVisible();
    for (const size of [
        { width: 320, height: 568 },
        { width: 390, height: 844 },
        { width: 1024, height: 768 },
    ]) {
        await page.setViewportSize(size);
        for (const selector of [
            '.touch-stick',
            '[data-touch-action=fire]',
            '[data-touch-action=watch]',
            '.touch-weapon button',
        ]) {
            const box = await page.locator(selector).boundingBox();
            assert.ok(
                box.x >= 0 &&
                    box.y >= 0 &&
                    box.x + box.width <= size.width + 1 &&
                    box.y + box.height <= size.height + 1,
            );
        }
        await page.getByRole('button', { name: 'More', exact: true }).tap();
        await expect(page.getByRole('button', { name: 'Weapon +', exact: true })).toBeVisible();
        await expect(page.getByRole('button', { name: 'Weapon −', exact: true })).toBeVisible();
        const icon = await weapon.boundingBox();
        const tray = await page.locator('.touch-extra').boundingBox();
        assert.ok(
            icon.x + icon.width <= tray.x ||
                icon.y + icon.height <= tray.y ||
                icon.y >= tray.y + tray.height,
            'Weapon icon overlapped the More controls',
        );
        await page.getByRole('button', { name: 'More', exact: true }).tap();
        await page.screenshot({ path: resolve(output, `layout-${size.width}.png`) });
    }
    await page.getByRole('button', { name: 'More', exact: true }).tap();
    await expect(page.locator('[data-touch-action=previousWeapon]')).toBeVisible();
    await page.getByRole('button', { name: 'Pause', exact: true }).tap();
    await expect(page.locator('.touch-controls')).toHaveCount(0);
    await page.evaluate(() => touchFixture.resume());
    await expect(page.locator('.touch-controls')).toBeVisible();
    await page.getByRole('button', { name: 'Options', exact: true }).tap();
    await expect(page.locator('.touch-controls')).toHaveCount(0);
    await page.evaluate(() => touchFixture.resume());
    await page.evaluate(() => touchFixture.enabled(false));
    await expect(page.locator('.touch-controls')).toHaveCount(0);
    assert.deepEqual((await snapshot()).held, []);
    assert.deepEqual(errors, []);
    console.log(
        'Touch UI: weapon icon/More controls, independent move/look/fire, pointer cancellation, focus reset, menu mode, portrait/tablet bounds and disable passed',
    );
} finally {
    await browser.close();
    await new Promise((resolve) => server.close(resolve));
}
