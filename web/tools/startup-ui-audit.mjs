// ROM-free loading feedback while real download and Wasm compilation are held.
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

const origin = process.argv[2] || 'http://127.0.0.1:4178/Iron-Sights/';
const output = resolve(process.env.MIDAS_AUDIT_OUTPUT || '../build/web-startup/ui');
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
let maybePage;
try {
    const context = await browser.newContext({ viewport: { width: 1440, height: 900 } });
    const page = await context.newPage();
    maybePage = page;
    const errors = [];
    page.on('pageerror', (error) => {
        errors.push(String(error));
        console.log('PAGE ERROR: ' + String(error));
    });
    page.on('response', (response) => {
        if (response.status() >= 400)
            console.log('HTTP ' + response.status() + ' ' + response.url());
    });
    await page.addInitScript(() => {
        const compile = WebAssembly.compile;
        WebAssembly.compile = async (...args) => {
            await new Promise((resolve) => {
                globalThis.releaseStartupCompile = resolve;
            });
            return compile(...args);
        };
    });
    let releaseDownload;
    const held = new Promise((resolve) => {
        releaseDownload = resolve;
    });
    await page.route('**/ProjectMidas.wasm', async (route) => {
        const response = await route.fetch();
        await held;
        await route.fulfill({ response });
    });
    await page.goto(origin);
    const loading = page.locator('.startup-screen');
    const crt = page.locator('.room-canvas');
    await expect(crt).toHaveAttribute('data-room-loading-label', 'Downloading game', {
        timeout: 60_000,
    });
    await expect(crt).toHaveAttribute('data-room-loading-percent', '0');
    await expect(loading).toHaveCount(0);
    await page.screenshot({ path: resolve(output, 'download.png') });
    await page.keyboard.press('F2');
    await expect(loading).toHaveCount(0);
    await page.keyboard.press('F2');
    await expect(crt).toBeVisible();
    releaseDownload();
    await expect(crt).toHaveAttribute('data-room-loading-label', 'Compiling WebAssembly', {
        timeout: 60_000,
    });
    assert.equal(
        await crt.getAttribute('data-room-loading-percent'),
        '',
        'Compilation invented a percentage',
    );
    await page.screenshot({ path: resolve(output, 'compiling.png') });
    await page.evaluate(() => globalThis.releaseStartupCompile());
    await expect(loading).toHaveCount(0, { timeout: 60_000 });
    await expect(crt).toHaveAttribute('data-room-loading-label', '', { timeout: 60_000 });
    await expect(page.locator('.room-canvas')).toHaveAttribute('data-room-state', 'ready', {
        timeout: 60_000,
    });
    const second = await context.newPage();
    second.on('pageerror', (error) => errors.push(String(error)));
    await second.goto(origin);
    const warning = second
        .getByRole('dialog')
        .filter({ hasText: 'Your game is open in another tab' });
    await warning.waitFor();
    const box = await warning.boundingBox();
    assert.ok(Math.abs(box.y + box.height / 2 - 450) < 2, 'Save-lock warning was not centered');
    assert.equal(await second.evaluate(() => scrollY), 0);
    await second.screenshot({ path: resolve(output, 'other-tab.png') });
    await page.close();
    await warning.getByRole('button', { name: 'Try again' }).click();
    await warning.waitFor({ state: 'detached' });
    await second.waitForFunction(
        () =>
            document.querySelector('.launch-bar strong')?.textContent ===
            'Choose your ROM to begin.',
        undefined,
        { timeout: 60_000 },
    );
    await expect(second.locator('.room-canvas')).toHaveAttribute('data-room-state', 'ready', {
        timeout: 60_000,
    });
    assert.deepEqual(errors, []);
    console.log(
        'Startup UI: download percentage, indeterminate compilation, F2 recovery and room handoff passed',
    );
} catch (error) {
    await maybePage
        ?.screenshot({ path: resolve(output, 'failure.png'), fullPage: true })
        .catch(() => {});
    await writeFile(
        resolve(output, 'failure.txt'),
        (await maybePage?.locator('body').innerText()) || '',
    );
    throw error;
} finally {
    await browser.close();
}
