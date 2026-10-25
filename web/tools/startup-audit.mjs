// Actual-ROM startup performance evidence. Run against a completed served build.
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import assert from 'node:assert/strict';
import { launchAuditBrowser } from './browser-choice.mjs';
import { installStartupProbe, readStartup } from './startup-observer.mjs';

const [
    romPath,
    outputPath = '../build/web-startup/audit',
    origin = 'http://127.0.0.1:4178/Project-Midas-ADS/',
] = process.argv.slice(2);
if (!romPath) throw new Error('Usage: startup-audit.mjs local-rom private-output [origin]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
const context = await browser.newContext({ viewport: { width: 1440, height: 900 } });
const page = await context.newPage();
await installStartupProbe(page);
// Make real cooperative preparation slow enough to exercise the fitted-screen
// wait. This only delays zero-ms native yields, never blocks the browser thread.
await page.addInitScript(() => {
    const schedule = window.setTimeout;
    window.setTimeout = (callback, delay, ...args) => {
        const trace = globalThis.midasStartupTrace;
        const preparing = trace?.progress?.at(-1)?.label === 'Preparing graphics';
        return schedule(callback, preparing && delay === 0 ? 80 : delay, ...args);
    };
});
async function record(name) {
    await page.waitForFunction(
        () => document.querySelector('pre')?.textContent.includes('frame 600'),
        undefined,
        { timeout: 120_000 },
    );
    const trace = await page.evaluate(readStartup);
    const settings = await page.evaluate(() =>
        new TextDecoder().decode(globalThis.midasStartupGame.FS.readFile('/user/midas.ini')),
    );
    assert.match(settings, /^look_updown=1$/m, 'Fresh browser defaults did not retain Upright');
    const completed = trace.progress?.find(
        (progress) => progress.total > 0 && progress.completed === progress.total,
    );
    const first = trace.frames.find((frame) => frame.gfx > 0);
    assert.ok(
        completed && first && completed.at <= first.at,
        'Shader preparation did not finish before graphics began',
    );
    assert.ok(
        trace.frames
            .filter((frame) => frame.at > trace.main && frame.gfx === 0)
            .every((frame) => frame.loading || (frame.room && frame.roomLabel)),
        'Initialization exposed a blank canvas without loading feedback',
    );
    await writeFile(resolve(output, name + '.json'), JSON.stringify(trace, null, 2));
    await page.screenshot({ path: resolve(output, name + '.png') });
}
try {
    await page.goto(origin);
    await expect(page.locator('.room-canvas')).toHaveAttribute('data-room-state', 'ready', {
        timeout: 60_000,
    });
    await page.getByRole('button', { name: 'Insert GoldenEye cartridge' }).focus();
    await page.keyboard.press('Enter');
    await page.locator('.rom-dialog input[type=file]').setInputFiles(resolve(romPath));
    const crt = page.locator('.room-canvas');
    await expect(crt).toHaveAttribute('data-room-loading-label', 'Preparing graphics', {
        timeout: 60_000,
    });
    await expect(page.locator('.startup-screen')).toHaveCount(0);
    await page.screenshot({ path: resolve(output, 'preparing.png') });
    await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'screen', {
        timeout: 60_000,
    });
    await expect(crt).toHaveAttribute('data-room-loading-label', 'Preparing graphics');
    await page.screenshot({ path: resolve(output, 'screen-loading.png') });
    const early = await page.evaluate(readStartup);
    assert.ok(
        early.frames.some(
            (frame) => frame.phase === 'inserting' && frame.roomLabel === 'Preparing graphics',
        ),
        'Native preparation did not overlap cartridge insertion',
    );
    const corners = JSON.parse(await crt.getAttribute('data-room-screen-corners'));
    assert.ok(
        corners.every(([x, y]) => Math.abs(x) <= 1.01 && Math.abs(y) <= 1.01),
        'Loading screen overshot the fitted TV',
    );
    // Opening controls during preparation must retain the room and offer an
    // explicit resume without skipping the cinematic or losing loading feedback.
    await page.keyboard.press('F2');
    await page.keyboard.press('F2');
    await expect(page.getByRole('button', { name: 'Resume startup' })).toBeVisible();
    await page.evaluate(() => {
        const game = globalThis.midasStartupGame;
        const pause = game._portWebSetPaused;
        let interrupted = false;
        game._portWebSetPaused = (value) => {
            const result = pause(value);
            if (
                !value &&
                !interrupted &&
                document.querySelector('.room-layer')?.dataset.phase === 'complete'
            ) {
                interrupted = true;
                queueMicrotask(() => window.dispatchEvent(new Event('blur')));
            }
            return result;
        };
    });
    await page.getByRole('button', { name: 'Resume startup' }).click();
    // A real interruption during handoff also needs to resume new graphics,
    // rather than reapplying the cinematic's earlier ready-frame hold.
    await expect(page.locator('.room-layer')).toHaveAttribute('data-phase', 'complete', {
        timeout: 90_000,
    });
    await expect(page.getByRole('button', { name: 'Resume startup' })).toBeVisible();
    await page.getByRole('button', { name: 'Resume startup' }).click();
    await expect(page.locator('.room-layer')).toHaveCount(0, { timeout: 90_000 });
    await record('first');
    await page.reload();
    await record('cached');
    console.log('Startup timing traces recorded: ' + output);
} finally {
    await browser.close();
}
