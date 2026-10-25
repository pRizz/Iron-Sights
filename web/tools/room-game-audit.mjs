// Private actual-ROM room transition audit. Never upload its screenshots or saves.
import { expect } from '@playwright/test';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { launchAuditBrowser } from './browser-choice.mjs';
import { observeAudio, readAudio } from './audio-observer.mjs';
import { checkRoomGameplay } from './room-gameplay-check.mjs';
import { checkPresentation } from './presentation-check.mjs';

const [romPath, outputPath = '../build/web-room/game', origin = 'http://127.0.0.1:4176'] =
    process.argv.slice(2);
if (!romPath) throw new Error('Usage: room-game-audit.mjs local-rom [private-output] [origin]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
const reduced = process.env.MIDAS_REDUCED_MOTION === '1';
const gameplay = process.env.MIDAS_ROOM_GAMEPLAY === '1';
const context = await browser.newContext({
    viewport: { width: 1440, height: 900 },
    reducedMotion: reduced ? 'reduce' : 'no-preference',
});
const page = await context.newPage();
await page.addInitScript(observeAudio);
await page.addInitScript(() => {
    const observer = new MutationObserver((records) => {
        for (const record of records) {
            const value = record.target.dataset?.roomScreenCorners;
            if (value) globalThis.roomFinalCorners = JSON.parse(value);
        }
    });
    observer.observe(document, {
        subtree: true,
        attributes: true,
        attributeFilter: ['data-room-screen-corners'],
    });
});
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
    await expect(page.locator('.room-canvas')).toHaveAttribute('data-room-state', 'ready', {
        timeout: 60_000,
    });
    const roomCanvas = await page.locator('.room-canvas').elementHandle();
    const canvas = await page.locator('#canvas').elementHandle();
    await page.getByRole('button', { name: 'Insert GoldenEye cartridge' }).focus();
    await page.keyboard.press('Enter');
    await page.locator('.rom-dialog input[type=file]').setInputFiles(resolve(romPath));
    if (!reduced) {
        await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'inserting');
        await page.waitForTimeout(600);
        await page.screenshot({ path: resolve(output, 'inserting.png') });
        await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'flying', {
            timeout: 60_000,
        });
        await page.waitForTimeout(500);
        await page.screenshot({ path: resolve(output, 'flying.png') });
    }
    await waitForGame();
    await checkPresentation(page);
    await page.keyboard.press('F2');
    for (const viewport of [
        { width: 1920, height: 800 },
        { width: 800, height: 1000 },
        { width: 1440, height: 900 },
    ]) {
        await page.setViewportSize(viewport);
        await checkPresentation(page);
    }
    await page.screenshot({ path: resolve(output, 'letterboxed-game.png') });
    await page.keyboard.press('Alt+Enter');
    await page.waitForFunction(() => !!document.fullscreenElement);
    await checkPresentation(page);
    await page.keyboard.press('Alt+Enter');
    await page.waitForFunction(() => !document.fullscreenElement);
    await page.keyboard.press('F2');
    if (!reduced) {
        const corners = await page.evaluate(() => globalThis.roomFinalCorners);
        assert.equal(corners?.length, 4, 'TV endpoint was not observed');
        const edges = corners.flatMap(([x, y]) => [Math.abs(x), Math.abs(y)]);
        assert.ok(
            edges.every((edge) => edge <= 1.00001),
            'TV screen cropped at the flight endpoint',
        );
        assert.ok(Math.abs(Math.max(...edges) - 1) < 0.00001, 'TV does not meet a canvas edge');
    }
    assert.ok(
        await roomCanvas.evaluate((element) => element.getContext('webgl2').isContextLost()),
        'Room GPU context survived handoff',
    );
    assert.ok(await canvas.evaluate((element) => element === document.querySelector('#canvas')));
    await page.getByRole('button', { name: 'Resume game' }).click();
    // A native menu gesture also unlocks browser audio after the cinematic.
    await page.keyboard.press('Enter', { delay: 150 });
    await page.waitForTimeout(1500);
    const audio = gameplay
        ? await checkRoomGameplay(page, context, output)
        : await page.evaluate(readAudio);
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
                reduced,
                gameplay,
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
