// ROM-free immersive-entrypoint checks. Screenshots stay in ignored build/.
import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

const origin = process.argv[2] || 'http://127.0.0.1:4176';
const output = resolve('../build/web-room/audit');
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
try {
    const context = await browser.newContext({ viewport: { width: 1440, height: 900 } });
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', (error) => errors.push(String(error)));
    await page.goto(origin);
    await expect(page.locator('.room-canvas')).toBeVisible();
    const imageSize = await page.evaluate(async () => {
        const response = await fetch(new URL('room/cartridge-art.avif', document.baseURI));
        if (!response.ok) throw new Error('Cartridge artwork did not load');
        const image = await createImageBitmap(await response.blob());
        const size = [image.width, image.height];
        image.close();
        return size;
    });
    assert.deepEqual(imageSize, [1000, 1000]);
    await page.waitForTimeout(2000);
    await expect(page.locator('.site-header')).toBeHidden();
    const canvas = await page.locator('#canvas').elementHandle();
    assert.ok(canvas);
    const bounds = await page.locator('.room-canvas').boundingBox();
    assert.equal(bounds.width, 1440);
    assert.equal(bounds.height, 900);
    await page.screenshot({ path: resolve(output, 'room.png') });
    await page.mouse.move(550, 460);
    await expect(page.locator('.room-canvas')).toHaveCSS('cursor', 'pointer');
    await page.mouse.click(550, 460);
    await expect(page.getByRole('dialog', { name: 'Insert your GoldenEye ROM' })).toBeVisible();
    await page.getByRole('button', { name: 'Back to the room' }).click();
    const cartridge = page.getByRole('button', { name: 'Insert GoldenEye cartridge' });
    await cartridge.focus();
    await page.keyboard.press('Enter');
    await expect(page.getByRole('dialog', { name: 'Insert your GoldenEye ROM' })).toBeVisible();
    await page.locator('.rom-dialog input[type=file]').setInputFiles({
        name: 'invalid.z64',
        mimeType: 'application/octet-stream',
        buffer: Buffer.from('invalid'),
    });
    await expect(page.locator('.rom-dialog [role=alert]')).toContainText('different size');
    await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'empty');
    await page.getByRole('button', { name: 'Back to the room' }).click();
    await page.keyboard.press('F2');
    await expect(page.locator('.site-header')).toBeVisible();
    await expect(page.locator('.room-layer')).toBeHidden();
    await expect(page.getByRole('button', { name: 'Download save backup' })).toBeEnabled({
        timeout: 60_000,
    });
    assert.ok(await canvas.evaluate((element) => element === document.querySelector('#canvas')));
    await page.keyboard.press('F2');
    await expect(page.locator('.room-canvas')).toBeVisible();
    await page.keyboard.press('Alt+Enter');
    await page.waitForFunction(() => !!document.fullscreenElement);
    await page.keyboard.press('Alt+Enter');
    await page.waitForFunction(() => !document.fullscreenElement);
    await page.setViewportSize({ width: 800, height: 1000 });
    await page.waitForTimeout(500);
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
} finally {
    await browser.close();
}
