import assert from 'node:assert/strict';
import { expect } from '@playwright/test';

export async function auditTouchSurface(page) {
    for (const layout of ['n64', 'modern']) {
        await page.evaluate((value) => touchFixture.layout(value), layout);
        for (const size of [
            { width: 844, height: 390 },
            { width: 320, height: 568 },
        ]) {
            await page.setViewportSize(size);
            for (const mode of ['game', 'menu', 'files']) {
                await page.evaluate((value) => {
                    touchFixture.menu(value !== 'game');
                    touchFixture.fileMenu(value === 'files');
                }, mode);
                await expect(page.locator('.touch-controls')).toHaveAttribute(
                    'data-mode',
                    mode === 'game' ? 'game' : 'menu',
                );
                const blocked = await page.evaluate((value) => {
                    const expected = value === 'game' ? '.touch-look' : '.touch-menu-surface';
                    const missed = [];
                    for (let row = 0; row < 12; row++)
                        for (let column = 0; column < 16; column++) {
                            const x = ((column + 0.5) * innerWidth) / 16;
                            const y = ((row + 0.5) * innerHeight) / 12;
                            const element = document.elementFromPoint(x, y);
                            if (element?.closest('button, .touch-stick, [data-fixture-settings]'))
                                continue;
                            if (!element?.closest(expected))
                                missed.push({ x, y, target: element?.className });
                        }
                    return missed;
                }, mode);
                assert.equal(
                    blocked.length,
                    0,
                    `Dead touch space in ${layout}/${mode}/${size.width}: ${JSON.stringify(blocked.slice(0, 8))}`,
                );
            }
        }
    }
    await page.setViewportSize({ width: 844, height: 390 });
    await page.evaluate(() => {
        touchFixture.fileMenu(false);
        touchFixture.menu(false);
        touchFixture.layout('n64');
    });
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-mode', 'game');
}

export async function auditTouchGestures(page, context, kind) {
    const snapshot = () => page.evaluate(() => touchFixture.snapshot());
    // A second, non-primary pointer must keep looking even if capture was
    // refused and moves land on another real control. Synthetic pointer IDs
    // deliberately cannot acquire browser pointer capture in either engine.
    const before = await snapshot();
    await page.evaluate(() => {
        const stick = document.querySelector('.touch-stick');
        const box = stick.getBoundingClientRect();
        stick.dispatchEvent(
            new PointerEvent('pointerdown', {
                bubbles: true,
                cancelable: true,
                pointerId: 401,
                pointerType: 'touch',
                isPrimary: true,
                clientX: box.x + box.width / 2,
                clientY: box.y + 15,
            }),
        );
        document.querySelector('.touch-look').dispatchEvent(
            new PointerEvent('pointerdown', {
                bubbles: true,
                cancelable: true,
                pointerId: 402,
                pointerType: 'touch',
                isPrimary: false,
                clientX: innerWidth * 0.6,
                clientY: innerHeight - 12,
            }),
        );
        const start = document.querySelector('[data-controller-button=start]');
        const target = start.getBoundingClientRect();
        start.dispatchEvent(
            new PointerEvent('pointermove', {
                bubbles: true,
                pointerId: 402,
                pointerType: 'touch',
                isPrimary: false,
                clientX: target.x + target.width / 2,
                clientY: target.y + target.height / 2,
            }),
        );
    });
    let value = await snapshot();
    assert.ok(
        value.movement[1] > 0.5 && value.look[0] > before.look[0] + 1,
        'Non-primary uncaptured look was lost while walking',
    );
    assert.deepEqual(value.held, [], 'Dragging across Start pressed it');
    await page.evaluate(() => {
        for (const pointerId of [402, 401])
            document.dispatchEvent(new PointerEvent('pointerup', { pointerId }));
    });
    assert.deepEqual((await snapshot()).movement, [0, 0]);

    if (kind === 'chromium') {
        const session = await context.newCDPSession(page);
        const fingers = new Map();
        const update = async (id, x, y, down) => {
            const existed = fingers.has(id),
                ending = fingers.get(id);
            down ? fingers.set(id, { id, x, y }) : fingers.delete(id);
            await session.send('Input.dispatchTouchEvent', {
                type: down ? (existed ? 'touchMove' : 'touchStart') : 'touchEnd',
                touchPoints: down ? [...fingers.values()] : [ending],
            });
        };
        const stick = await page.locator('.touch-stick').boundingBox();
        const start = await page.locator('[data-controller-button=start]').boundingBox();
        const z = await page.locator('[data-controller-button=z]').boundingBox();
        await update(1, stick.x + stick.width / 2, stick.y + 15, true);
        for (const [x, y] of [
            [540, 24],
            [540, 378],
            [(start.x + start.width + z.x) / 2, 190],
        ]) {
            const previous = (await snapshot()).look[0];
            await update(2, x, y, true);
            await update(2, x + 3, y + 2, true);
            await update(2, 0, 0, false);
            value = await snapshot();
            assert.ok(
                value.look[0] > previous + 0.1 && value.movement[1] > 0.5,
                `Right-thumb drag lost at ${x},${y} with left thumb still down`,
            );
        }
        await update(1, 0, 0, false);
        assert.deepEqual((await snapshot()).movement, [0, 0]);
        await session.detach();
    }

    await page.evaluate(() => touchFixture.menu(true));
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-mode', 'menu');
    const accepts = (await snapshot()).accepts;
    const cursor = await page.evaluate(() => {
        document.querySelector('.touch-menu-surface').dispatchEvent(
            new PointerEvent('pointerdown', {
                bubbles: true,
                cancelable: true,
                pointerId: 403,
                pointerType: 'touch',
                clientX: 400,
                clientY: 24,
            }),
        );
        const target = document.querySelector('[data-controller-button=start]');
        const box = target.getBoundingClientRect();
        const point = [box.x + box.width / 2, box.y + box.height / 2];
        target.dispatchEvent(
            new PointerEvent('pointermove', {
                bubbles: true,
                pointerId: 403,
                clientX: point[0],
                clientY: point[1],
            }),
        );
        document.dispatchEvent(
            new PointerEvent('pointerup', {
                pointerId: 403,
                clientX: point[0],
                clientY: point[1],
            }),
        );
        return point;
    });
    assert.deepEqual((await snapshot()).cursor, cursor, 'Uncaptured menu pan stopped at a button');
    assert.equal((await snapshot()).accepts, accepts, 'Panning across a button selected');
    await page.evaluate(() => touchFixture.menu(false));
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-mode', 'game');
}

export async function auditTouchHints(page) {
    await page.evaluate(() => touchFixture.pause());
    await expect(page.locator('.touch-controls')).toHaveCount(0);
    await page.evaluate(() => touchFixture.resume());
    const game = page.locator('.touch-game-hint');
    const menu = page.locator('.touch-menu-hint');
    await expect(game).toBeVisible();
    assert.ok((await game.boundingBox()).y <= 10, 'Game hint is not near the screen top');
    await page.clock.fastForward(4000);
    await expect(game).toBeVisible();
    // The real capture state emits fresh playing snapshots during a mission.
    // Refreshing that snapshot must not restart the five-second hint deadline.
    await page.evaluate(() => touchFixture.resume());
    await page.clock.fastForward(1000);
    await expect(game).toHaveClass(/helper-text-fading/);
    await expect(game).toHaveCount(1);
    await expect(game).toHaveCSS('transition-property', 'opacity');
    await expect(game).toHaveCSS('transition-duration', '0.35s');
    await page.clock.fastForward(360);
    await expect(game).toHaveCount(0);
    await expect(page.locator('.touch-look span')).toHaveCount(0);
    await page.evaluate(() => touchFixture.menu(true));
    await expect(menu).toBeVisible();
    assert.ok((await menu.boundingBox()).y <= 10, 'Menu hint is not near the screen top');
    await page.clock.fastForward(2000);
    await page.evaluate(() => touchFixture.menu(false));
    await expect(game).toBeVisible();
    await page.clock.fastForward(1200);
    await expect(game).toBeVisible(); // The previous menu's timer must have been cancelled.
    await page.clock.fastForward(4000);
    await expect(game).toHaveCount(0);
    await page.evaluate(() => touchFixture.menu(true));
    await expect(menu).toBeVisible();
    await page.clock.fastForward(4000);
    await expect(menu).toBeVisible();
    await page.clock.fastForward(1000);
    await expect(menu).toHaveClass(/helper-text-fading/);
    await page.clock.fastForward(360);
    await expect(menu).toHaveCount(0);
    await page.evaluate(() => touchFixture.fileMenu(true));
    await expect(menu).toBeVisible();
    await page.clock.fastForward(5000);
    await expect(menu).toHaveClass(/helper-text-fading/);
    await page.clock.fastForward(360);
    await expect(menu).toHaveCount(0);
    await page.evaluate(() => {
        touchFixture.fileMenu(false);
        touchFixture.menu(false);
        touchFixture.message(true);
    });
    await expect(page.locator('.touch-look')).toBeVisible();
    const message = await page.locator('.experience-hint').boundingBox();
    assert.equal(
        await page.evaluate(
            ({ x, y }) => !!document.elementFromPoint(x, y)?.closest('.touch-look'),
            { x: message.x + message.width / 2, y: message.y + message.height / 2 },
        ),
        true,
        'Status hint intercepted the gesture surface',
    );
    await page.evaluate(() => touchFixture.message(false));
    await expect(page.locator('.experience-hint')).toHaveClass(/helper-text-fading/);
    await page.clock.fastForward(150);
    await page.evaluate(() => touchFixture.message(true));
    await expect(page.locator('.experience-hint')).not.toHaveClass(/helper-text-fading/);
    await page.clock.fastForward(360);
    await expect(page.locator('.experience-hint')).toBeVisible();
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await expect(page.locator('.experience-hint')).toHaveCSS('transition-duration', '0.08s');
    await page.evaluate(() => touchFixture.message(false));
    await page.clock.fastForward(360);
    await expect(page.locator('.experience-hint')).toHaveCount(0);
    await page.emulateMedia({ reducedMotion: 'no-preference' });
}
