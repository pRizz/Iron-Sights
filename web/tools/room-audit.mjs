// ROM-free immersive-entrypoint checks. Screenshots stay in ignored build/.
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { checkPresentation } from './presentation-check.mjs';
import { checkRoomFraming } from './room-framing-check.mjs';

const origin = process.argv[2] || 'http://127.0.0.1:4176';
const output = resolve(process.env.MIDAS_AUDIT_OUTPUT || '../build/web-room/audit');
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
try {
    const context = await browser.newContext({ viewport: { width: 1440, height: 900 } });
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', (error) => errors.push(String(error)));
    await page.goto(origin);
    await expect(page.locator('.room-canvas')).toBeVisible();
    await expect(page.locator('.room-canvas')).toHaveAttribute('data-room-state', 'ready', {
        timeout: 60_000,
    });
    await expect(page.locator('.room-canvas')).toHaveAttribute(
        'data-room-artwork',
        /^(avif|png)$/,
        { timeout: 60_000 },
    );
    const imageSize = await page.evaluate(async () => {
        const canvas = document.querySelector('.room-canvas');
        const format = canvas.dataset.roomArtwork;
        const response = await fetch(new URL('room/cartridge-art.' + format, document.baseURI));
        if (!response.ok) throw new Error('Cartridge artwork did not load');
        const image = new Image();
        const url = URL.createObjectURL(await response.blob());
        image.src = url;
        await image.decode();
        const size = [image.naturalWidth, image.naturalHeight];
        URL.revokeObjectURL(url);
        return size;
    });
    assert.deepEqual(imageSize, [1000, 1000]);
    await page.waitForTimeout(2000);
    await page.waitForFunction(
        () => Number(document.querySelector('.room-canvas').dataset.roomFrames) >= 12,
        undefined,
        { timeout: 60_000 },
    );
    const firstFrame = await page.locator('.room-canvas').evaluate((element) => ({
        frames: Number(element.dataset.roomFrames),
        at: performance.now(),
    }));
    await page.waitForTimeout(2000);
    const measured = await page.locator('.room-canvas').evaluate((element) => ({
        frames: Number(element.dataset.roomFrames),
        submissionMS: Number(element.dataset.roomSubmitMs),
        drawCalls: Number(element.dataset.roomDrawCalls),
        triangles: Number(element.dataset.roomTriangles),
        scale: Number(element.dataset.roomScale),
        at: performance.now(),
    }));
    const idleFPS = ((measured.frames - firstFrame.frames) * 1000) / (measured.at - firstFrame.at);
    await writeFile(
        resolve(output, 'metrics.json'),
        JSON.stringify({ ...measured, idleFPS }, null, 2),
    );
    await expect(page.locator('.site-header')).toBeHidden();
    const canvas = await page.locator('#canvas').elementHandle();
    assert.ok(canvas);
    const bounds = await page.locator('.room-canvas').boundingBox();
    assert.equal(bounds.width, 1440);
    assert.equal(bounds.height, 900);
    await page.screenshot({ path: resolve(output, 'room.png') });
    await checkPresentation(page);
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await page.waitForTimeout(100);
    const initialPitch = Number(await page.locator('.room-canvas').getAttribute('data-room-pitch'));
    const initialYaw = Number(await page.locator('.room-canvas').getAttribute('data-room-yaw'));
    await page.mouse.move(100, 700);
    await page.mouse.down();
    // End over the cartridge: releasing a background drag must not choose a ROM.
    await page.mouse.move(550, 460, { steps: 10 });
    await expect(page.locator('.room-canvas')).toHaveCSS('cursor', 'grabbing');
    await page.mouse.up();
    await page.waitForTimeout(100);
    assert.ok(
        Number(await page.locator('.room-canvas').getAttribute('data-room-pitch')) <
            initialPitch - 0.1,
    );
    assert.ok(
        Number(await page.locator('.room-canvas').getAttribute('data-room-yaw')) > initialYaw + 1,
    );
    await expect(page.locator('.rom-dialog')).toBeHidden();
    await page.screenshot({ path: resolve(output, 'pivot.png') });
    await page.mouse.move(1100, 100);
    await page.mouse.down();
    await page.mouse.move(650, 340, { steps: 10 });
    await page.mouse.up();
    await page.waitForTimeout(100);
    assert.ok(
        Math.abs(
            Number(await page.locator('.room-canvas').getAttribute('data-room-pitch')) -
                initialPitch,
        ) < 0.001,
    );
    await page.mouse.move(550, 460);
    await expect(page.locator('.room-canvas')).toHaveCSS('cursor', 'pointer');
    await page.mouse.click(550, 460);
    await expect(page.getByRole('dialog', { name: 'Insert your GoldenEye ROM' })).toBeVisible();
    await page.getByRole('button', { name: 'Back to the room' }).click();
    await page.emulateMedia({ reducedMotion: 'no-preference' });
    const cartridge = page.getByRole('button', { name: 'Insert GoldenEye cartridge' });
    await cartridge.focus();
    await page.keyboard.press('Enter');
    await expect(page.getByRole('dialog', { name: 'Insert your GoldenEye ROM' })).toBeVisible();
    const choosingRom = page.waitForEvent('filechooser');
    await page.getByRole('dialog').getByRole('button', { name: 'Choose ROM', exact: true }).click();
    const chooser = await choosingRom;
    assert.equal(
        await chooser.element().getAttribute('accept'),
        null,
        'Room ROM picker must allow unfamiliar iCloud file types',
    );
    await chooser.setFiles({
        name: 'invalid.z64',
        mimeType: 'application/octet-stream',
        buffer: Buffer.from('invalid'),
    });
    await expect(page.locator('.rom-dialog [role=alert]')).toContainText('different size');
    await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'empty');
    // Cached-ROM errors may open this modal before the first F2 gesture.
    // F2 must still reveal the full controls and their Forget-ROM action.
    await page.keyboard.press('F2');
    await expect(page.getByRole('dialog', { name: 'Insert your GoldenEye ROM' })).toBeHidden();
    await expect(page.locator('.site-header')).toBeVisible();
    await expect(page.locator('.room-layer')).toBeHidden();
    const beforePause = await page.locator('.room-canvas').getAttribute('data-room-frames');
    await page.waitForTimeout(250);
    assert.equal(await page.locator('.room-canvas').getAttribute('data-room-frames'), beforePause);
    await expect(page.getByRole('button', { name: 'Download save backup' })).toBeEnabled({
        timeout: 60_000,
    });
    assert.ok(await canvas.evaluate((element) => element === document.querySelector('#canvas')));
    await page.keyboard.press('F2');
    await expect(page.locator('.room-canvas')).toBeVisible();
    await page.evaluate(() => {
        globalThis.restoreFullscreenRequest = () => {
            Element.prototype.requestFullscreen = original;
        };
        const original = Element.prototype.requestFullscreen;
        Element.prototype.requestFullscreen = () =>
            Promise.reject(new DOMException('Synthetic refusal', 'NotAllowedError'));
    });
    await page.keyboard.press('Alt+Enter');
    await expect(page.locator('.experience-hint')).toContainText('Fullscreen was refused');
    await page.evaluate(() => globalThis.restoreFullscreenRequest());
    await page.keyboard.press('Alt+Enter');
    await page.waitForFunction(() => !!document.fullscreenElement);
    await checkPresentation(page);
    await page.keyboard.press('Alt+Enter');
    await page.waitForFunction(() => !document.fullscreenElement);
    await page.setViewportSize({ width: 800, height: 1000 });
    await page.waitForTimeout(500);
    await checkPresentation(page);
    await page.screenshot({ path: resolve(output, 'portrait-room.png') });
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await page.waitForTimeout(500);
    await page.screenshot({ path: resolve(output, 'reduced-motion.png') });
    await page.locator('.room-canvas').dispatchEvent('webglcontextlost');
    await expect(page.locator('.site-header')).toBeVisible();
    assert.ok(await canvas.evaluate((element) => element === document.querySelector('#canvas')));
    assert.deepEqual(errors, []);
    console.log(
        'Room: viewport, keyboard ROM dialog, invalid ROM, stable canvas/F2, fullscreen, portrait, reduced motion and context-loss fallback passed',
    );
    await context.close();
    const fallbackContext = await browser.newContext();
    const fallback = await fallbackContext.newPage();
    await fallback.route('**/assets/scene-*.js', (route) => route.abort());
    await fallback.goto(origin);
    await expect(fallback.locator('.site-header')).toBeVisible({ timeout: 60_000 });
    await expect(fallback.locator('.controls-close')).toBeDisabled();
    await expect(
        fallback.getByRole('status').filter({ hasText: '3D room is unavailable' }),
    ).toBeVisible();
    await fallbackContext.close();
    const imageFallbackContext = await browser.newContext();
    const imageFallback = await imageFallbackContext.newPage();
    await imageFallback.route('**/room/cartridge-art.avif', (route) => route.abort());
    await imageFallback.goto(origin);
    await expect(imageFallback.locator('.room-canvas')).toHaveAttribute(
        'data-room-artwork',
        'png',
        { timeout: 60_000 },
    );
    await imageFallbackContext.close();
    const modelFallbackContext = await browser.newContext();
    const modelFallback = await modelFallbackContext.newPage();
    await modelFallback.route('**/room/controller.glb', (route) => route.abort());
    await modelFallback.goto(origin);
    await expect(modelFallback.locator('.site-header')).toBeVisible({ timeout: 60_000 });
    await expect(modelFallback.locator('.controls-close')).toBeDisabled();
    await modelFallbackContext.close();
    await checkRoomFraming(browser, origin, output);
} finally {
    await browser.close();
}
