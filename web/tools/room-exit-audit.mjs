// Actual native EXIT GAME confirmation, room retreat and same-page power resume.
// Uses a local ROM; all screenshots/backups remain in ignored build/.
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { observeAudio, readAudio } from './audio-observer.mjs';

const [romPath, outputPath = '../build/web-room/exit', origin = 'http://127.0.0.1:4176'] =
    process.argv.slice(2);
if (!romPath) throw new Error('Usage: room-exit-audit.mjs local-rom [private-output] [origin]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
const context = await browser.newContext({
    viewport: { width: 1440, height: 900 },
    reducedMotion: process.env.MIDAS_REDUCED_MOTION === '1' ? 'reduce' : 'no-preference',
});
const page = await context.newPage();
await page.addInitScript(observeAudio);
const errors = [];
page.on('pageerror', (error) => errors.push(String(error)));
async function menuClick(x, y) {
    const rect = await page.locator('#canvas').boundingBox();
    const band = Math.min(rect.width, (rect.height * 4) / 3);
    const clientX = rect.x + (rect.width - band) / 2 + (x / 440) * band;
    const clientY = rect.y + (y / 330) * rect.height;
    // Browsers cannot warp the OS pointer. Native menu handoff therefore uses
    // deltas after reseating; traverse both bounds to align its virtual cursor.
    await page.mouse.move(clientX - 8, clientY - 5);
    await page.waitForTimeout(120);
    await page.mouse.move(rect.x + rect.width - 1, rect.y + rect.height - 1);
    await page.waitForTimeout(120);
    await page.mouse.move(rect.x + 1, rect.y + 1);
    await page.waitForTimeout(120);
    await page.mouse.move(clientX, clientY, { steps: 20 });
    await page.waitForTimeout(120);
    await page.mouse.click(clientX, clientY, { delay: 120 });
    await page.waitForTimeout(500);
}
async function backup() {
    const pending = page.waitForEvent('download');
    await page.getByRole('button', { name: 'Download save backup' }).click();
    const download = await pending;
    const chunks = [];
    for await (const chunk of await download.createReadStream()) chunks.push(chunk);
    return JSON.parse(Buffer.concat(chunks).toString());
}
async function confirmExit() {
    await menuClick(390, 298);
    await page.screenshot({ path: resolve(output, 'exit-confirm.png') });
    await menuClick(185, 170);
    await expect(page.locator('.room-layer')).toHaveAttribute('data-returning', 'true', {
        timeout: 60_000,
    });
    await page.waitForTimeout(600);
    await page.screenshot({ path: resolve(output, 'exit-retreat.png') });
    await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'inserted', {
        timeout: 60_000,
    });
    await page.screenshot({ path: resolve(output, 'room-returned.png') });
    const paused = await page.locator('pre').textContent();
    await page.waitForTimeout(1500);
    assert.equal(
        await page.locator('pre').textContent(),
        paused,
        'Native game advanced in the returned room',
    );
    assert.equal(await page.evaluate(() => document.pointerLockElement === null), true);
    const audio = await page.evaluate(readAudio);
    assert.ok(
        audio.length > 0 &&
            audio.every((sample) => sample.state === 'suspended' || sample.peak < 0.00001),
        'Game audio was not silent in the room',
    );
    return paused;
}
try {
    await page.goto(origin);
    await page.getByRole('button', { name: 'Insert GoldenEye cartridge' }).focus();
    await page.keyboard.press('Enter');
    await page.locator('.rom-dialog input[type=file]').setInputFiles(resolve(romPath));
    await expect(page.locator('.room-layer')).toHaveCount(0, { timeout: 90_000 });
    await page.waitForFunction(
        () => document.querySelector('pre').textContent.includes('frame 240'),
        undefined,
        { timeout: 90_000 },
    );
    if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
        await page.getByRole('button', { name: 'Resume game' }).click();
    for (let step = 0; step < 4; step++) {
        await page.keyboard.press('Enter', { delay: 120 });
        await page.waitForTimeout(1800);
        await page.screenshot({ path: resolve(output, `menu-${step}.png`) });
    }
    // Native No leaves the Files screen running; Yes must return to the room.
    await menuClick(390, 298);
    await menuClick(255, 170);
    await expect(page.locator('.room-layer')).toHaveCount(0);
    const canvas = await page.locator('#canvas').elementHandle();
    const frozen = await confirmExit();
    await page.keyboard.press('F2');
    await page.getByText('Saved on this device', { exact: true }).waitFor();
    const before = await backup();
    await page.keyboard.press('F2');
    // Use the physical switch raycast rather than the accessible button.
    await page.mouse.move(488, 610);
    await expect(page.locator('.room-canvas')).toHaveCSS('cursor', 'pointer');
    await page.mouse.click(488, 610);
    await expect(page.locator('.room-layer')).toHaveCount(0, { timeout: 90_000 });
    assert.ok(await canvas.evaluate((element) => element === document.querySelector('#canvas')));
    await page.waitForFunction((old) => document.querySelector('pre').textContent !== old, frozen);
    await confirmExit();
    await page.keyboard.press('F2');
    await page.getByText('Saved on this device', { exact: true }).waitFor();
    assert.deepEqual((await backup()).files, before.files, 'Power resume changed saved files');
    // A supported, explicit management restart must hold cached auto-start.
    await Promise.all([
        page.waitForURL((url) => url.searchParams.has('controls')),
        page.getByRole('button', { name: 'Restart to manage saves' }).click(),
    ]);
    await expect(page.getByRole('button', { name: 'Restore save backup' })).toBeEnabled({
        timeout: 60_000,
    });
    assert.deepEqual((await backup()).files, before.files);
    await page.keyboard.press('F2');
    await page.waitForFunction(
        () => document.querySelector('pre').textContent.includes('frame 240'),
        undefined,
        { timeout: 90_000 },
    );
    await expect(page.locator('.room-layer')).toHaveCount(0);
    assert.deepEqual(errors, []);
    await writeFile(
        resolve(output, 'report.json'),
        JSON.stringify(
            {
                checks: 'native Exit Game No/Yes, retreat, paused/audio/input release, physical warm power resume, stable canvas/saves, management restart and cached auto-start',
                errors,
            },
            null,
            2,
        ),
    );
    console.log(
        'Native Exit Game: reverse room transition, warm power resume and save-management restart passed',
    );
} catch (error) {
    await page.screenshot({ path: resolve(output, 'failure.png'), fullPage: true }).catch(() => {});
    await writeFile(
        resolve(output, 'failure-log.txt'),
        await page
            .locator('pre')
            .textContent()
            .catch(() => ''),
    );
    throw error;
} finally {
    await browser.close();
}
