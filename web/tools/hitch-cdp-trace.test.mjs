import assert from 'node:assert/strict';
import { test } from 'node:test';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from '@playwright/test';
import { startBrowserTrace, reportBrowserTrace } from './hitch-cdp-trace.mjs';

test('browser-level CDP trace streams, compresses and aligns a page mark', async () => {
    const folder = resolve('../build/web-hitches/rom-free');
    await mkdir(folder, { recursive: true });
    const browser = await chromium.launch({ headless: true, channel: 'chrome' });
    try {
        const page = await browser.newPage();
        await page.goto('data:text/html,<canvas></canvas>');
        const path = resolve(folder, 'menu.cdp.json.gz');
        const controller = await startBrowserTrace(browser, path, 32768);
        const anchorAt = await page.evaluate(() => {
            const at = performance.now();
            performance.mark('midas-hitch:menu:start');
            return at;
        });
        await page.evaluate(() => {
            const until = performance.now() + 80;
            while (performance.now() < until) { /* synthetic main-thread stall */ }
        });
        const recording = await controller.stop();
        const report = await reportBrowserTrace(path, 'menu',
            { at: anchorAt + 80, gap: 80, anchorAt }, { phase: 'menu', recording });
        assert.equal(recording.dataLoss, false);
        assert.ok(report.analysis.events > 0);
        assert.equal(report.analysis.anchorFound, true);
        assert.ok(report.analysis.topEvents.length > 0);
    } finally {
        await browser.close();
    }
});
