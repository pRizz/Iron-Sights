// Private actual-ROM audit of FORK-019's in-process icon stage. Never upload these captures.
// A fresh browser profile's first Play renders the weapon strip icons behind the
// startup screen ('Preparing weapon icons'), persists the cache with the saves and
// returns to the title; a reload reuses the cache, and a mission uses the icons.
import { launchAuditBrowser } from './browser-choice.mjs';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { installGameProbe, navigateToDam, captureGameplay } from './native-game-audit.mjs';

const [romPath, outputPath = '../build/web-ui/icon-stage', origin = 'http://127.0.0.1:4176'] =
    process.argv.slice(2);
if (!romPath) throw new Error('Usage: icon-stage-audit.mjs local-rom output-directory [origin]');
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
const log = () => page.evaluate(() => globalThis.midasLines.join('\n'));

// Starts the game and records every startup label shown until the startup ends.
async function play(name) {
    await page.evaluate(() => {
        globalThis.midasLabels = [];
        const seen = () => {
            const text = document.body.innerText.toUpperCase();  /* labels may be styled upper case */
            for (const label of ['Preparing graphics', 'Preparing weapon icons', 'Starting GoldenEye'])
                if (text.includes(label.toUpperCase()) && globalThis.midasLabels.at(-1) !== label)
                    globalThis.midasLabels.push(label);
        };
        globalThis.midasLabelTimer = setInterval(seen, 50);
    });
    const started = Date.now();
    await page.getByRole('button', { name: 'Play', exact: true }).click();
    // Started: the boot decided about icons, no stage is running and the title is drawing.
    await page.waitForFunction(
        () => {
            const game = globalThis.midasAuditGame;
            const text = globalThis.midasLines.join('\n');
            return game && game._portWebGetGfxTasks() > 60 && !game._portWebIconStageBusy()
                && /in-process preparation (ready|failed)|reusing validated cache/.test(text);
        },
        undefined,
        { timeout: 180_000, polling: 100 },
    );
    const labels = await page.evaluate(() => {
        clearInterval(globalThis.midasLabelTimer);
        return globalThis.midasLabels;
    });
    await page.screenshot({ path: resolve(output, name + '-started.png') });
    return { labels, seconds: (Date.now() - started) / 1000 };
}

try {
    await page.goto(origin);
    await page.keyboard.press('F2');
    await page.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });
    await page.locator('#rom-file').setInputFiles(resolve(romPath));
    await page.getByText('✓ Remembered for next time').waitFor({ timeout: 60_000 });

    const first = await play('first');
    const firstLog = await log();
    await writeFile(resolve(output, 'first-log.txt'), firstLog);
    console.log(`First launch: ${first.seconds.toFixed(1)} s to frame 240; labels ${first.labels.join(' > ')}`);
    assert.ok(firstLog.includes('ge: icons: preparing in process before the title'), 'The stage was not armed');
    const ready = firstLog.match(/in-process preparation ready after (\d+) ms/);
    assert.ok(ready, 'The in-process stage did not finish');
    console.log(`Icon stage: ${ready[1]} ms`);
    assert.deepEqual(
        first.labels.slice(-2),
        ['Preparing weapon icons', 'Starting GoldenEye'],
        'The startup screen did not move from the icon step to the start: ' + first.labels.join(' > '),
    );
    assert.ok(!first.labels.slice(0, -2).includes('Starting GoldenEye'), 'The startup track stepped back');
    const cache = await page.evaluate(() => {
        const fs = globalThis.midasAuditGame.FS;
        const folder = '/user/Data/Cache';
        return fs.analyzePath(folder).exists
            ? fs.readdir(folder).filter((name) => name.endsWith('.cache')).map((name) => [name, fs.stat(folder + '/' + name).size])
            : [];
    });
    console.log('Cache files: ' + JSON.stringify(cache));
    assert.equal(cache.length, 1, 'The icon cache was not written to the user folder');
    assert.equal(cache[0][1], 12779712, 'The icon cache has the wrong size');
    // Let the write reach IndexedDB before the reload.
    await page.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });

    await page.reload();
    await page.keyboard.press('F2');
    await page.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });
    const second = await play('second');
    const secondLog = await log();
    await writeFile(resolve(output, 'second-log.txt'), secondLog);
    console.log(`Reload: ${second.seconds.toFixed(1)} s to frame 240; labels ${second.labels.join(' > ')}`);
    assert.ok(secondLog.includes('ge: icons: reusing validated cache'), 'The reload did not reuse the cache');
    assert.ok(!second.labels.includes('Preparing weapon icons'), 'The reload prepared the icons again');

    await navigateToDam(page);
    await captureGameplay(page);
    await page.waitForTimeout(1500);
    await page.mouse.wheel(0, 120);
    await page.waitForTimeout(400);
    await page.screenshot({ path: resolve(output, 'dam-strip.png') });
    const damLog = await log();
    await writeFile(resolve(output, 'dam-log.txt'), damLog);
    assert.ok(damLog.includes('ge: icons: the weapon strip uses icons from here on'), 'The mission did not take the icons');
    assert.deepEqual(errors, [], 'Page errors: ' + errors.join('; '));
    console.log('Icon stage audit passed');
} finally {
    await browser.close();
}
