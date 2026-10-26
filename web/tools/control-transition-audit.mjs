// Actual normal-menu Dam cutscene/control/watch/focus regression; private evidence only.
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { installStartupProbe, readStartup } from './startup-observer.mjs';

const [
    romPath,
    outputPath = '../build/web-startup/control',
    origin = 'http://127.0.0.1:4178/Iron-Sights/',
] = process.argv.slice(2);
if (!romPath)
    throw new Error('Usage: control-transition-audit.mjs local-rom private-output [origin]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
const context = await browser.newContext({
    viewport: { width: 1440, height: 900 },
    reducedMotion: process.env.MIDAS_REDUCED_MOTION === '1' ? 'reduce' : 'no-preference',
});
const page = await context.newPage();
await installStartupProbe(page);
const errors = [];
page.on('pageerror', (error) => errors.push(String(error)));
try {
    await page.goto(origin);
    await expect(page.locator('.startup-screen')).toHaveCount(0, { timeout: 60_000 });
    await page.getByRole('button', { name: 'Insert GoldenEye cartridge' }).focus();
    await page.keyboard.press('Enter');
    await page.locator('.rom-dialog input[type=file]').setInputFiles(resolve(romPath));
    if (process.env.MIDAS_PAUSE_STARTUP === '1') {
        await page
            .getByRole('heading', { name: 'Preparing graphics', exact: true })
            .waitFor({ timeout: 60_000 });
        await page.keyboard.press('F2');
        await page.keyboard.press('F2');
        await expect(page.getByRole('heading', { name: 'Startup paused' })).toBeVisible();
        await page.getByRole('button', { name: 'Resume startup' }).click();
    }
    await expect(page.locator('.room-layer')).toHaveCount(0, { timeout: 120_000 });
    await page.waitForFunction(() => globalThis.midasStartupGame?._portWebGetGfxTasks() >= 240);
    for (let step = 0; step < 12; step++) {
        if (await page.evaluate(() => globalThis.midasStartupGame._portWebGetStage() === 33)) break;
        await page.keyboard.press('Enter', { delay: 120 });
        await page.waitForTimeout(1800);
    }
    assert.equal(
        await page.evaluate(() => globalThis.midasStartupGame._portWebGetStage()),
        33,
        'Normal menus did not reach Dam',
    );
    // No key/click is sent while the complete intro hands control to Bond.
    await page.waitForFunction(
        () => globalThis.midasStartupGame._portWebControlsLive(),
        undefined,
        { timeout: 90_000 },
    );
    const before = await page.evaluate(() => globalThis.midasStartupGame._portWebGetGfxTasks());
    await page.waitForTimeout(3000);
    await expect(page.getByRole('button', { name: 'Resume game' })).toBeHidden();
    assert.ok(
        await page.evaluate(
            (value) => globalThis.midasStartupGame._portWebGetGfxTasks() > value + 15,
            before,
        ),
        'Game stopped on the cutscene-to-control transition',
    );
    await page.screenshot({ path: resolve(output, 'bond-in-control.png') });
    if (!(await page.evaluate(() => !!document.pointerLockElement)))
        await page.locator('#canvas').click();
    await page.waitForFunction(() => !!document.pointerLockElement);
    await page.keyboard.press('p', { delay: 120 });
    await page.waitForFunction(() => !document.pointerLockElement);
    await page.waitForTimeout(1000);
    await expect(page.getByRole('button', { name: 'Resume game' })).toBeHidden();
    await page.keyboard.press('p', { delay: 120 });
    await page.waitForFunction(() => globalThis.midasStartupGame._portWebControlsLive());
    await page.waitForTimeout(1200);
    await expect(page.getByRole('button', { name: 'Resume game' })).toBeHidden();
    if (!(await page.evaluate(() => !!document.pointerLockElement)))
        await page.locator('#canvas').click();
    await page.waitForFunction(() => !!document.pointerLockElement);
    const other = await context.newPage();
    await other.goto('about:blank');
    await other.bringToFront();
    await page.getByRole('button', { name: 'Resume game' }).waitFor();
    await page.waitForTimeout(500);
    const frozen = await page.evaluate(() => globalThis.midasStartupGame._portWebGetGfxTasks());
    await page.waitForTimeout(1000);
    assert.equal(
        await page.evaluate(() => globalThis.midasStartupGame._portWebGetGfxTasks()),
        frozen,
    );
    await page.bringToFront();
    await page.getByRole('button', { name: 'Resume game' }).click();
    await page.waitForFunction(() => !!document.pointerLockElement);
    await other.close();
    await page.evaluate(() => document.exitPointerLock());
    await page.getByRole('button', { name: 'Resume game' }).waitFor();
    assert.deepEqual(errors, []);
    await writeFile(
        resolve(output, 'trace.json'),
        JSON.stringify(await page.evaluate(readStartup), null, 2),
    );
    await writeFile(
        resolve(output, 'report.json'),
        JSON.stringify(
            {
                checks: 'uninterrupted Dam cutscene-to-control, watch open/close without false pause, gesture capture, real focus/Escape pause and resume',
                errors,
            },
            null,
            2,
        ),
    );
    console.log(
        'Control transition: cutscene/watch stay live; real focus/capture loss still pauses',
    );
} catch (error) {
    await page.screenshot({ path: resolve(output, 'failure.png'), fullPage: true }).catch(() => {});
    await writeFile(
        resolve(output, 'trace.json'),
        JSON.stringify(await page.evaluate(readStartup), null, 2),
    );
    throw error;
} finally {
    await browser.close();
}
