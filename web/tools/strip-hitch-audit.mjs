// Private actual-ROM measurement of the first weapon-strip appearance in a mission.
// Never upload these logs. Starts a fresh profile (so the first launch prepares
// the icons), enters Dam through the menus and records, from the moment the
// player gets control, the browser's longest frame gaps and the game's own
// 'ge: hitch:' lines, which break a slow frame into shader compiles and uploads.
// A CPU throttle (default 4x) stands in for a phone's slower main thread.
import { launchAuditBrowser } from './browser-choice.mjs';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { installGameProbe, navigateToDam } from './native-game-audit.mjs';

const [romPath, outputPath = '../build/web-ui/strip-hitch', origin = 'http://127.0.0.1:4176', throttle = '4'] =
    process.argv.slice(2);
if (!romPath) throw new Error('Usage: strip-hitch-audit.mjs local-rom output-directory [origin] [cpu-throttle]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const { browser, kind } = await launchAuditBrowser();
const context = await browser.newContext({ viewport: { width: 1280, height: 900 } });
const page = await context.newPage();
await installGameProbe(page);
await page.addInitScript(() => {
    globalThis.midasLines = [];
    const watch = () => {
        const pre = document.querySelector('pre');
        if (!pre) return setTimeout(watch, 50);
        let seen = '';
        new MutationObserver(() => {
            const text = pre.textContent;
            // The log drops its oldest lines; carry on after the last text already seen.
            const tail = seen.slice(-400);
            const at = tail ? text.lastIndexOf(tail) : -1;
            const fresh = text.startsWith(seen) ? text.slice(seen.length) : at >= 0 ? text.slice(at + tail.length) : text;
            seen = text;
            for (const line of fresh.split('\n')) if (line) globalThis.midasLines.push(line);
        }).observe(pre, { childList: true, subtree: true, characterData: true });
    };
    watch();
    // Frame gaps, recorded once asked to.
    globalThis.midasGaps = [];
    let last = 0;
    const frame = (now) => {
        if (globalThis.midasGapsOn && last) globalThis.midasGaps.push([now, now - last]);
        last = now;
        requestAnimationFrame(frame);
    };
    requestAnimationFrame(frame);
});
try {
    await page.goto(origin);
    await page.keyboard.press('F2');
    await page.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });
    await page.locator('#rom-file').setInputFiles(resolve(romPath));
    await page.getByText('✓ Remembered for next time').waitFor({ timeout: 60_000 });
    await page.getByRole('button', { name: 'Play', exact: true }).click();
    await page.waitForFunction(
        () => {
            const game = globalThis.midasAuditGame;
            return game && game._portWebGetGfxTasks() > 60 && !game._portWebIconStageBusy();
        },
        undefined,
        { timeout: 180_000, polling: 200 },
    );
    let cdp = null;
    if (kind === 'chromium' || kind === 'chrome' || kind === 'msedge') {
        cdp = await context.newCDPSession(page);
        await cdp.send('Emulation.setCPUThrottlingRate', { rate: Number(throttle) });
        if (process.env.MIDAS_PROFILE) {
            // MIDAS_PROFILE=1: a CPU profile of the mission start, to name what runs in a long gap.
            await cdp.send('Profiler.enable');
            await cdp.send('Profiler.setSamplingInterval', { interval: 500 });
            await cdp.send('Profiler.start');
        }
    }
    const mark = await page.evaluate(() => {
        globalThis.midasGaps = [];
        globalThis.midasGapsOn = true;
        return globalThis.midasLines.length;
    });
    await navigateToDam(page);
    await page.waitForFunction(() => globalThis.midasAuditGame._portWebControlsLive(), undefined, {
        timeout: 120_000,
        polling: 'raf',
    });
    const live = await page.evaluate(() => performance.now());
    if (cdp && process.env.MIDAS_TRACE) {
        // MIDAS_TRACE=1: a browser trace of the first seconds of control, GPU process included.
        await cdp.send('Tracing.start', {
            transferMode: 'ReturnAsStream',
            traceConfig: {
                includedCategories: ['toplevel', 'gpu', 'gpu.angle', 'viz', 'v8', 'devtools.timeline',
                    'disabled-by-default-gpu.service', 'disabled-by-default-gpu.decoder', 'disabled-by-default-devtools.timeline'],
            },
        });
    }
    await page.waitForTimeout(6000);
    if (cdp && process.env.MIDAS_TRACE) {
        const done = new Promise((resolveDone) => cdp.once('Tracing.tracingComplete', resolveDone));
        await cdp.send('Tracing.end');
        const { stream } = await done;
        const chunks = [];
        for (;;) {
            const { data, eof, base64Encoded } = await cdp.send('IO.read', { handle: stream, size: 1 << 20 });
            chunks.push(base64Encoded ? Buffer.from(data, 'base64') : Buffer.from(data));
            if (eof) break;
        }
        await cdp.send('IO.close', { handle: stream });
        await writeFile(resolve(output, 'mission-start.trace.json'), Buffer.concat(chunks));
    }
    const result = await page.evaluate(([from, live]) => {
        globalThis.midasGapsOn = false;
        // From half a second before the player got control: the stage load before it is not the strip.
        const gaps = globalThis.midasGaps.filter(([at]) => at > live - 500).map(([, gap]) => gap).sort((a, b) => b - a);
        return {
            frames: globalThis.midasGaps.length,
            worstGaps: gaps.slice(0, 5).map((gap) => Math.round(gap)),
            // [ms after the player got control at the gap's end, gap ms], over the whole run
            // every slow frame from just before control to a second after: the strip's first frames
            aroundLive: globalThis.midasGaps
                .filter(([at, gap]) => gap > 25 && at > live - 500 && at < live + 1000)
                .map(([at, gap]) => [Math.round(at - live), Math.round(gap)]),
            longGaps: globalThis.midasGaps.filter(([, gap]) => gap > 150).map(([at, gap]) => [Math.round(at - live), Math.round(gap)]),
            hitches: globalThis.midasLines.slice(from).filter((line) => line.includes('ge: hitch:')),
            iconsLatched: globalThis.midasLines.some((line) => line.includes('weapon strip uses icons')),
        };
    }, [mark, live]);
    if (cdp && process.env.MIDAS_PROFILE) {
        const { profile } = await cdp.send('Profiler.stop');
        await writeFile(resolve(output, 'mission-start.cpuprofile'), JSON.stringify(profile));
    }
    await page.screenshot({ path: resolve(output, 'strip.png') });
    await writeFile(resolve(output, 'result.json'), JSON.stringify(result, null, 2));
    console.log(`Throttle ${throttle}x: worst frame gaps ${result.worstGaps.join(', ')} ms over ${result.frames} frames`);
    for (const line of result.hitches) console.log(line);
    assert.ok(result.iconsLatched, 'The mission did not use the icons');
} finally {
    await browser.close();
}
