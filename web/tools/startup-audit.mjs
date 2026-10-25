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
async function record(name) {
    await page.waitForFunction(
        () => document.querySelector('pre')?.textContent.includes('frame 600'),
        undefined,
        { timeout: 120_000 },
    );
    const trace = await page.evaluate(readStartup);
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
            .every((frame) => frame.loading),
        'Initialization exposed a blank canvas without loading feedback',
    );
    await writeFile(resolve(output, name + '.json'), JSON.stringify(trace, null, 2));
    await page.screenshot({ path: resolve(output, name + '.png') });
}
try {
    await page.goto(origin);
    await page.getByRole('button', { name: 'Insert GoldenEye cartridge' }).focus();
    await page.keyboard.press('Enter');
    await page.locator('.rom-dialog input[type=file]').setInputFiles(resolve(romPath));
    await page
        .getByRole('heading', { name: 'Preparing graphics', exact: true })
        .waitFor({ timeout: 60_000 });
    await page.screenshot({ path: resolve(output, 'preparing.png') });
    await expect(page.locator('.room-layer')).toHaveCount(0, { timeout: 90_000 });
    await record('first');
    await page.reload();
    await record('cached');
    console.log('Startup timing traces recorded: ' + output);
} finally {
    await browser.close();
}
