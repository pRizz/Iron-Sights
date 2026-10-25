// Private actual-ROM room transition audit. Never upload its screenshots or saves.
import { expect } from '@playwright/test';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { launchAuditBrowser } from './browser-choice.mjs';
import { observeAudio, readAudio } from './audio-observer.mjs';

const [romPath, outputPath = '../build/web-room/game', origin = 'http://127.0.0.1:4176'] =
    process.argv.slice(2);
if (!romPath) throw new Error('Usage: room-game-audit.mjs local-rom [private-output] [origin]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
const context = await browser.newContext({ viewport: { width: 1440, height: 900 } });
const page = await context.newPage();
await page.addInitScript(observeAudio);
const errors = [];
const requests = [];
page.on('pageerror', (error) => errors.push(String(error)));
page.on('request', (request) => requests.push(request.method()));
async function download() {
    const pending = page.waitForEvent('download');
    await page.getByRole('button', { name: 'Download save backup' }).click();
    const result = await pending;
    const chunks = [];
    for await (const chunk of await result.createReadStream()) chunks.push(chunk);
    return JSON.parse(Buffer.concat(chunks).toString());
}
async function waitForGame() {
    await expect(page.locator('.room-layer')).toHaveCount(0, { timeout: 90_000 });
    await page.waitForFunction(
        () => document.querySelector('pre').textContent.includes('frame 240'),
        undefined,
        { timeout: 90_000 },
    );
    await page.keyboard.press('F2');
}
try {
    await page.goto(origin);
    await expect(page.locator('.room-canvas')).toBeVisible();
    const canvas = await page.locator('#canvas').elementHandle();
    await page.getByRole('button', { name: 'Insert GoldenEye cartridge' }).focus();
    await page.keyboard.press('Enter');
    await page.locator('.rom-dialog input[type=file]').setInputFiles(resolve(romPath));
    await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'inserting');
    await page.waitForTimeout(600);
    await page.screenshot({ path: resolve(output, 'inserting.png') });
    await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'flying', {
        timeout: 60_000,
    });
    await page.waitForTimeout(500);
    await page.screenshot({ path: resolve(output, 'flying.png') });
    await waitForGame();
    assert.ok(await canvas.evaluate((element) => element === document.querySelector('#canvas')));
    await page.getByRole('button', { name: 'Resume game' }).click();
    // A native menu gesture also unlocks browser audio after the cinematic.
    await page.keyboard.press('Enter', { delay: 150 });
    await page.waitForTimeout(1500);
    const audio = await page.evaluate(readAudio);
    assert.ok(
        audio.some(
            (sample) =>
                sample.state === 'running' && Math.max(sample.peak, sample.outputPeak) > 0.00001,
        ),
        'No game audio after the room handoff',
    );
    await page.getByRole('button', { name: 'Pause', exact: true }).click();
    await page.getByText('Saved on this device', { exact: true }).waitFor();
    const before = await download();
    await page.reload();
    await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'inserted', {
        timeout: 60_000,
    });
    await page.waitForTimeout(700);
    await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'inserted');
    await page.screenshot({ path: resolve(output, 'returning.png') });
    await page.keyboard.press('F2');
    await page.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });
    assert.deepEqual((await download()).files, before.files, 'Room reload changed the saved files');
    await page.keyboard.press('F2');
    // Physical power-switch hit target, rather than the accessibility alternative.
    await page.mouse.move(488, 610);
    await expect(page.locator('.room-canvas')).toHaveCSS('cursor', 'pointer');
    await page.mouse.click(488, 610);
    await expect(page.locator('.room-layer')).not.toHaveAttribute('data-phase', 'inserted');
    await waitForGame();
    assert.deepEqual(errors, []);
    assert.ok(
        requests.every((method) => method === 'GET'),
        'Unexpected outgoing request',
    );
    await writeFile(
        resolve(output, 'report.json'),
        JSON.stringify(
            {
                checks: 'insertion, flight, stable canvas, actual game/audio, durable reload, physical returning power switch',
                audio,
                errors,
            },
            null,
            2,
        ),
    );
    console.log(
        'Room actual ROM: insertion/flight, game/audio, durable reload and returning power-switch startup passed',
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
