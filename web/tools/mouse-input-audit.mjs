// Private real-ROM mouse conservation/capture diagnostic; no hardware-spike claim.
import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { installGameProbe, navigateToDam, captureGameplay } from './native-game-audit.mjs';

const [
    rom,
    folder = '../build/web-mouse/game',
    origin = 'http://127.0.0.1:4177/Iron-Sights/',
] = process.argv.slice(2);
if (!rom) throw new Error('Usage: mouse-input-audit.mjs private-rom private-output [origin]');
const output = resolve(folder);
await mkdir(output, { recursive: true });
const { browser, kind } = await launchAuditBrowser();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
await installGameProbe(page);
await page.addInitScript((forceUnsupported) => {
    globalThis.mouseRequests = [];
    const request = HTMLCanvasElement.prototype.requestPointerLock;
    HTMLCanvasElement.prototype.requestPointerLock = function (options) {
        mouseRequests.push(options || null);
        if (forceUnsupported && options?.unadjustedMovement) {
            queueMicrotask(() => document.dispatchEvent(new Event('pointerlockerror')));
            return Promise.reject(
                new DOMException('Synthetic unsupported raw input', 'NotSupportedError'),
            );
        }
        return request.call(this, options);
    };
}, process.env.MIDAS_MOUSE_FALLBACK === '1');
const errors = [];
page.on('pageerror', (error) => errors.push(String(error)));
const heading = () => page.evaluate(() => midasAuditGame._portWebGetPlayerHeading());
const turn = (a, b) => ((b - a + 540) % 360) - 180;
try {
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 90000,
    });
    await page.getByRole('button', { name: 'Play GoldenEye' }).click();
    await page.waitForFunction(
        () => globalThis.midasAuditGame?._portWebGetGfxTasks() >= 240,
        undefined,
        { timeout: 120000 },
    );
    await page.locator('.controls-close').click();
    if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
        await page.getByRole('button', { name: 'Resume game' }).click();
    await navigateToDam(page);
    await captureGameplay(page);
    await page.mouse.move(640, 400);
    await page.waitForTimeout(300);
    await page.keyboard.down('w');
    const before = await heading();
    for (let i = 1; i <= 20; i++) {
        await page.mouse.move(640 + i * 2, 400);
        await page.waitForTimeout(16);
    }
    await page.waitForTimeout(300);
    const normal = turn(before, await heading());
    await page.mouse.move(640, 400);
    await page.waitForTimeout(300);
    const beforeHitch = await heading();
    if (kind === 'chromium') {
        const session = await page.context().newCDPSession(page);
        const hitch = page.evaluate(() => {
            const end = performance.now() + 250;
            while (performance.now() < end) {}
        });
        await Promise.all(
            Array.from({ length: 20 }, (_, i) =>
                session.send('Input.dispatchMouseEvent', {
                    type: 'mouseMoved',
                    x: 642 + i * 2,
                    y: 400,
                }),
            ),
        );
        await hitch;
    } else {
        await page.evaluate(() => {
            const end = performance.now() + 250;
            while (performance.now() < end) {}
        });
        await page.mouse.move(680, 400);
    }
    await page.waitForTimeout(500);
    const afterHitch = await heading(),
        hitchTurn = turn(beforeHitch, afterHitch);
    assert.ok(Math.abs(normal) > 0.1, 'Mouse did not turn Bond');
    assert.ok(Math.abs(normal - hitchTurn) < 0.15, 'Hitch amplified or lost the known mouse delta');
    await page.waitForTimeout(500);
    assert.ok(
        Math.abs(turn(afterHitch, await heading())) < 0.01,
        'Mouse motion replayed after the input stopped',
    );
    await page.keyboard.up('w');
    await page.keyboard.press('P', { delay: 120 });
    await page.waitForFunction(() => !midasAuditGame._portWebControlsLive());
    await page.waitForTimeout(500);
    await page.keyboard.press('P', { delay: 120 });
    await captureGameplay(page);
    await expect(page.getByRole('button', { name: 'Resume game' })).toBeHidden();
    await page.keyboard.press('F2');
    const downloadEvent = page.waitForEvent('download');
    await page.getByRole('button', { name: 'Download mouse diagnostics' }).click();
    const download = await downloadEvent;
    const file = resolve(output, 'mouse-diagnostics.json');
    await download.saveAs(file);
    const report = JSON.parse(await readFile(file, 'utf8'));
    assert.ok(report.motion.length > 0 && report.motion.length <= 1024);
    assert.ok(report.frames.length > 0 && report.frames.length <= 512);
    assert.ok(
        report.frames.some((frame) => frame.gap > 150),
        'Diagnostic missed the deliberate main-thread hitch',
    );
    assert.ok(report.frames.some((frame) => frame.heading !== undefined));
    assert.ok(report.runtime?.build?.commit, 'Diagnostic omitted runtime identity');
    const requests = await page.evaluate(() => mouseRequests);
    assert.equal(requests[0]?.unadjustedMovement, true);
    assert.equal(
        report.requestMode,
        process.env.MIDAS_MOUSE_FALLBACK === '1' ? 'compatibility' : 'unadjusted',
    );
    await page.setViewportSize({ width: 390, height: 844 });
    const button = await page
        .getByRole('button', { name: 'Download mouse diagnostics' })
        .boundingBox();
    assert.ok(
        button.x >= 0 && button.x + button.width <= 391,
        'Narrow toolbar clipped diagnostics',
    );
    assert.deepEqual(errors, []);
    const summary = {
        normal,
        hitchTurn,
        requestMode: report.requestMode,
        requests,
        browser: browser.version(),
        errors,
    };
    await writeFile(resolve(output, 'report.json'), JSON.stringify(summary, null, 2));
    console.log(JSON.stringify(summary));
} catch (error) {
    await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
    await writeFile(resolve(output, 'failure.txt'), String(error));
    throw error;
} finally {
    await browser.close();
}
