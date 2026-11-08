// Private actual-ROM growth soak in the browser. Never upload these logs.
// Sets GE_SOAK before Play (port/src/port_soak.c reads Module.ENV live), lets the
// game cycle the stages, and records its "ge: memstats:" lines: the in-use heap
// (dlmalloc), the Wasm memory size (which only grows) and renderer counts.
import { launchAuditBrowser } from './browser-choice.mjs';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { installGameProbe } from './native-game-audit.mjs';

const [romPath, outputPath = '../build/web-ui/soak', origin = 'http://127.0.0.1:4176', spec = 'all:300:4'] =
    process.argv.slice(2);
if (!romPath) throw new Error('Usage: soak-audit.mjs local-rom output-directory [origin] [stages:frames:laps]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
const context = await browser.newContext({ viewport: { width: 1440, height: 1100 } });
const page = await context.newPage();
await installGameProbe(page);
const errors = [];
page.on('pageerror', (error) => errors.push(String(error)));
// The page's log keeps only recent lines; collect every line from the start.
await page.addInitScript(() => {
    globalThis.midasLines = [];
    const watch = () => {
        const pre = document.querySelector('pre');
        if (!pre) return setTimeout(watch, 50);
        let seen = '';
        new MutationObserver(() => {
            const text = pre.textContent;
            const fresh = text.startsWith(seen) ? text.slice(seen.length) : text;
            seen = text;
            for (const line of fresh.split('\n')) if (line) globalThis.midasLines.push(line);
        }).observe(pre, { childList: true, subtree: true, characterData: true });
    };
    watch();
});
try {
    await page.goto(origin);
    await page.keyboard.press('F2');
    await page.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });
    await page.locator('#rom-file').setInputFiles(resolve(romPath));
    await page.getByText('✓ Remembered for next time').waitFor({ timeout: 60_000 });
    await page.evaluate((value) => {
        globalThis.midasAuditGame.ENV.GE_SOAK = value;
    }, spec);
    const started = Date.now();
    await page.getByRole('button', { name: 'Play', exact: true }).click();
    await page.waitForFunction(
        () => globalThis.midasLines.some((line) => line.includes('ge: soak: complete')),
        undefined,
        { timeout: 3 * 3600_000, polling: 1000 },
    );
    const lines = await page.evaluate(() => globalThis.midasLines);
    await writeFile(resolve(output, 'soak-log.txt'), lines.join('\n'));
    const samples = lines.filter((line) => line.includes('ge: memstats:'));
    await writeFile(resolve(output, 'memstats.txt'), samples.join('\n'));
    console.log(`Soak ${spec}: ${samples.length} samples in ${((Date.now() - started) / 60000).toFixed(1)} min`);
    assert.ok(samples.length > 0, 'No memstats lines were logged');
    assert.deepEqual(errors, [], 'Page errors: ' + errors.join('; '));
} finally {
    await browser.close();
}
