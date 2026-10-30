// ROM-free actual pointer-capture/multi-contact checks; fake native boundary only.
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import assert from 'node:assert/strict';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { auditN64Layout } from './touch-n64-audit.mjs';
import { auditTouchContext } from './touch-context-audit.mjs';
import { auditTouchSurface, auditTouchGestures, auditTouchHints } from './touch-surface-audit.mjs';

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
    await page.clock.install();
    const errors = [];
    page.on('pageerror', (error) => errors.push(String(error)));
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-mode', 'game');
    await auditTouchContext(page);
    await auditTouchSurface(page);
    await auditTouchGestures(page, context, kind);
    await auditTouchHints(page);
    await auditN64Layout(page, output);
    const aim = page.locator('[data-touch-action=aim]');
    await aim.tap();
    await expect(aim).toHaveAttribute('aria-pressed', 'true');
    assert.ok((await page.evaluate(() => touchFixture.snapshot())).held.includes(1));
    await aim.tap();
    await expect(aim).toHaveAttribute('aria-pressed', 'false');
    const snapshot = () => page.evaluate(() => touchFixture.snapshot());
    const scale = await page.evaluate(() => visualViewport.scale);
    assert.equal(
        await page.evaluate(() => {
            // Gaps near buttons must hit the guarded overlay, never SDL's canvas.
            return !!document.elementFromPoint(10, 150)?.closest('.touch-look');
        }),
        true,
        'A gap in the controls bypassed the touch gesture guard',
    );
    for (let tap = 0; tap < 8; tap++) await page.locator('[data-touch-action=fire]').tap();
    assert.equal(
        await page.evaluate(() => visualViewport.scale),
        scale,
        'Repeated fire zoomed page',
    );
    assert.deepEqual((await snapshot()).held, []);
    assert.equal(
        await page.locator('[data-touch-action=fire]').evaluate((surface) => {
            const event = new Event('touchend', { bubbles: true, cancelable: true });
            // Native touchend carries a TouchList; leave the document fallback inert.
            Object.defineProperty(event, 'touches', { value: [{}] });
            surface.dispatchEvent(event);
            return event.defaultPrevented;
        }),
        true,
        'Safari touchend zoom fallback was passive or missing',
    );
    const gadget = page.getByRole('button', { name: 'Switch gadget', exact: true });
    await expect(gadget.locator('svg')).toBeVisible();
    // The primary icon uses the native next-inventory action, with independent
    // press/release ownership on every tap, without opening More.
    for (let tap = 0; tap < 2; tap++) {
        await gadget.hover();
        await page.mouse.down();
        assert.ok((await snapshot()).held.includes(11));
        await page.mouse.up();
        assert.ok(!(await snapshot()).held.includes(11));
    }
    await expect(page.locator('.touch-extra')).toHaveCount(0);
    await gadget.evaluate((button) => {
        button.dispatchEvent(
            new PointerEvent('pointerdown', {
                bubbles: true,
                cancelable: true,
                pointerId: 771,
                pointerType: 'touch',
            }),
        );
        document.querySelector('[data-touch-action=fire]').dispatchEvent(
            new PointerEvent('pointerdown', {
                bubbles: true,
                cancelable: true,
                pointerId: 772,
                pointerType: 'touch',
            }),
        );
    });
    await page.evaluate(() => touchFixture.gadgets(false));
    await expect(gadget).toHaveCount(0);
    assert.deepEqual(
        (await snapshot()).held,
        [0],
        'Hiding an empty gadget control released Fire or kept gadget held',
    );
    await page.evaluate(() => {
        document.dispatchEvent(new PointerEvent('pointerup', { pointerId: 771 }));
        document.dispatchEvent(new PointerEvent('pointerup', { pointerId: 772 }));
        touchFixture.gadgets(true);
    });
    await expect(gadget).toBeVisible();
    assert.deepEqual((await snapshot()).held, []);
    // Exercise missing pointer terminals in both engines, including WebKit's
    // desktop driver, which cannot inject a native multi-contact touch stream.
    await page.locator('.touch-stick').evaluate((stick) => {
        const box = stick.getBoundingClientRect();
        stick.dispatchEvent(
            new PointerEvent('pointerdown', {
                bubbles: true,
                cancelable: true,
                pointerId: 77,
                pointerType: 'touch',
                clientX: box.x + box.width / 2,
                clientY: box.y + 15,
            }),
        );
        // Losing a descendant's implicit capture must not release its owner.
        stick.querySelector('i').dispatchEvent(
            new PointerEvent('lostpointercapture', {
                bubbles: true,
                pointerId: 77,
                pointerType: 'touch',
            }),
        );
    });
    assert.ok((await snapshot()).movement[1] > 0.5, 'Descendant capture loss released the stick');
    await page.evaluate(() => {
        const event = new Event('touchend');
        Object.defineProperty(event, 'touches', { value: [] });
        document.dispatchEvent(event);
    });
    assert.deepEqual(
        (await snapshot()).movement,
        [0, 0],
        'Missing pointer terminal left stick held',
    );
    await page.locator('.touch-stick').evaluate((stick) => {
        const box = stick.getBoundingClientRect();
        stick.dispatchEvent(
            new PointerEvent('pointerdown', {
                bubbles: true,
                cancelable: true,
                pointerId: 78,
                pointerType: 'touch',
                clientX: box.x + box.width / 2,
                clientY: box.y + 15,
            }),
        );
    });
    assert.ok((await snapshot()).movement[1] > 0.5, 'Recovered stick refused a fresh contact');
    await page.evaluate(() =>
        document.dispatchEvent(new PointerEvent('pointerup', { pointerId: 78 })),
    );
    assert.deepEqual((await snapshot()).movement, [0, 0], 'Document release left stick held');
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
        // A touch stream can end without pointerup/cancel reaching its owner.
        await update(1, stick.x + stick.width / 2, stick.y + 15, true);
        assert.ok((await snapshot()).movement[1] > 0.5);
        await page.evaluate(() =>
            document.dispatchEvent(new TouchEvent('touchend', { touches: [] })),
        );
        assert.deepEqual((await snapshot()).movement, [0, 0], 'Orphaned stick was not released');
        await session.send('Input.dispatchTouchEvent', { type: 'touchCancel', touchPoints: [] });
        fingers.clear();
        await update(1, stick.x + stick.width / 2, stick.y + 15, true);
        assert.ok((await snapshot()).movement[1] > 0.5, 'Stick could not be used after recovery');
        await update(1, 0, 0, false);
        assert.deepEqual((await snapshot()).movement, [0, 0]);
        await update(1, fire.x + 24, fire.y + 24, true);
        await page.evaluate(() => window.dispatchEvent(new Event('blur')));
        assert.deepEqual((await snapshot()).held, [], 'Focus loss left fire held');
        await session.send('Input.dispatchTouchEvent', { type: 'touchCancel', touchPoints: [] });
    }
    await page.evaluate(() => touchFixture.menu(true));
    await expect(page.locator('[data-touch-action=accept]')).toBeVisible();
    await expect(gadget).toHaveCount(0);
    const menuSurface = page.locator('.touch-menu-surface');
    const accepts = (await snapshot()).accepts;
    await page.mouse.move(400, 150);
    await page.mouse.down();
    assert.equal((await snapshot()).accepts, accepts, 'Menu selected on finger-down');
    await page.mouse.move(480, 160);
    assert.deepEqual((await snapshot()).cursor, [480, 160]);
    await page.mouse.move(400, 150);
    await page.mouse.up();
    assert.equal((await snapshot()).accepts, accepts, 'Menu drag selected on release');
    await menuSurface.tap({ position: { x: 400, y: 150 } });
    assert.equal(
        (await snapshot()).accepts,
        accepts + 1,
        'Stationary menu tap did not select once',
    );
    await menuSurface.evaluate((surface) => {
        surface.dispatchEvent(
            new PointerEvent('pointerdown', {
                pointerId: 99,
                pointerType: 'touch',
                clientX: 400,
                clientY: 150,
                bubbles: true,
                cancelable: true,
            }),
        );
        document.dispatchEvent(new PointerEvent('pointercancel', { pointerId: 99 }));
        document.dispatchEvent(new PointerEvent('pointerup', { pointerId: 99 }));
    });
    assert.equal((await snapshot()).accepts, accepts + 1, 'Cancelled menu contact selected');
    // Document capture releases input even if a target swallows the up event.
    await page.locator('[data-touch-action=up]').evaluate((button) => {
        button.addEventListener('pointerup', (event) => event.stopPropagation(), { once: true });
    });
    await page.locator('[data-touch-action=up]').tap();
    assert.deepEqual((await snapshot()).held, [], 'Stopped propagation left a menu direction held');
    await page.evaluate(() => touchFixture.menu(false));
    await expect(page.locator('[data-touch-action=fire]')).toBeVisible();
    for (const size of [
        { width: 320, height: 568 },
        { width: 390, height: 844 },
        { width: 844, height: 390 },
        { width: 568, height: 280 },
        { width: 1024, height: 768 },
    ]) {
        await page.setViewportSize(size);
        for (const selector of [
            '.touch-stick',
            '[data-touch-action=fire]',
            '[data-touch-action=watch]',
            '.touch-gadget button',
        ]) {
            const box = await page.locator(selector).boundingBox();
            assert.ok(
                box.x >= 27 &&
                    box.y >= 0 &&
                    box.x + box.width <= size.width - 35 &&
                    box.y + box.height <= size.height - 39,
                'Modern controls need the larger shared edge insets',
            );
        }
        const stick = await page.locator('.touch-stick').boundingBox();
        const actions = await page.locator('.touch-actions').boundingBox();
        assert.ok(stick.x + stick.width <= actions.x, 'Stick overlaps Modern buttons');
        await page.getByRole('button', { name: 'More', exact: true }).tap();
        await expect(page.getByRole('button', { name: 'Weapon +', exact: true })).toBeVisible();
        await expect(page.getByRole('button', { name: 'Weapon −', exact: true })).toBeVisible();
        const icon = await gadget.boundingBox();
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
    await expect(page.getByRole('button', { name: /^(Pause|Options)$/ })).toHaveCount(0);
    await page.evaluate(() => touchFixture.pause());
    await expect(page.locator('.touch-controls')).toHaveCount(0);
    await page.evaluate(() => touchFixture.resume());
    await expect(page.locator('.touch-controls')).toBeVisible();
    await page.evaluate(() => touchFixture.enabled(false));
    await expect(page.locator('.touch-controls')).toHaveCount(0);
    assert.deepEqual((await snapshot()).held, []);
    await page.evaluate(() => touchFixture.fileMenu(true));
    const importer = page.getByRole('button', { name: 'Import Save', exact: true });
    await expect(importer).toBeVisible();
    const [picker] = await Promise.all([page.waitForEvent('filechooser'), importer.tap()]);
    await picker.setFiles({
        name: 'touch-save.json',
        mimeType: 'application/json',
        buffer: Buffer.from('{}'),
    });
    assert.equal((await snapshot()).imported, 'touch-save.json');
    await page.getByRole('button', { name: 'Export Save', exact: true }).tap();
    assert.equal((await snapshot()).exported, 1);
    await page.evaluate(() => touchFixture.fileMenu(false));
    await expect(importer).toHaveCount(0);
    assert.deepEqual(errors, []);
    console.log(
        'Touch UI: free-surface hit coverage, uncaptured/non-primary gestures, hint deadlines, repeated fire/zoom prevention, menu tap vs drag, orphaned stick recovery, independent move/look/fire, cancellation, focus reset, layouts, disable and direct save-picker gesture passed',
    );
} finally {
    await browser.close();
    await new Promise((resolve) => server.close(resolve));
}
