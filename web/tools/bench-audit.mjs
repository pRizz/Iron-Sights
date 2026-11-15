// Private real-ROM benchmark runner for the browser build (FORK-085). Plays the
// runs planned by scripts/port/bench/bench_web.py: each one reloads the page
// with its own environment, midas.ini and input route, waits for the game's
// GE_BENCH frame log, and saves it beside the plan. The numbers are worked out
// by `bench_web.py collect`; this tool only launches and collects.
// One browser profile: a first launch prepares the weapon icons, so no run
// waits on that stage. Never upload the output; it is timing data only.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

const [rom, folder, origin = 'http://127.0.0.1:4191/Iron-Sights/'] = process.argv.slice(2);
if (!rom || !folder) throw new Error('Usage: bench-audit.mjs private-rom session-folder [origin]');
const output = resolve(folder);
const plan = JSON.parse(await readFile(resolve(output, 'plan.json'), 'utf8'));
const RUN_TIMEOUT_MS = 10 * 60_000;
const DONE = 'ge: bench: wrote';

// What the next game launch takes; the route below reads it at each page load.
let current = { env: {}, ini: null, files: {} };
const { browser, kind } = await launchAuditBrowser({
    angleBackend: process.env.MIDAS_ANGLE || undefined,
});
const page = await browser.newPage({ viewport: { width: 1280, height: 960 } });
const pageErrors = [];
page.on('pageerror', (error) => pageErrors.push(String(error)));
let wasmDigest = '';
await page.route('**/runtime/*/ProjectMidas.wasm', async (route) => {
    const response = await route.fetch();
    wasmDigest ||= createHash('sha256')
        .update(await response.body())
        .digest('hex');
    await route.fulfill({ response });
});
await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
    const response = await route.fetch();
    const source = await response.text();
    assert.ok(source.includes('export default createMidas;'));
    await route.fulfill({
        response,
        body: source.replace(
            'export default createMidas;',
            `export default async function(options) {
    globalThis.benchAuditLog = [];
    for (const key of ['print', 'printErr']) {
        const originalPrint = options[key];
        options[key] = function(line) { benchAuditLog.push(line); originalPrint(line); };
    }
    const previous = options.preRun;
    options.preRun = (module) => {
        previous?.(module);
        Object.assign(module.ENV, ${JSON.stringify(current.env)});
    };
    const game = await createMidas(options);
    globalThis.benchAuditGame = game;
    const original = game.callMain;
    game.callMain = function(args) {
        const ini = ${JSON.stringify(current.ini)};
        if (ini !== null) game.FS.writeFile('/user/midas.ini', ini);
        for (const [path, text] of Object.entries(${JSON.stringify(current.files)}))
            game.FS.writeFile(path, text);
        return original(args);
    };
    return game;
}`,
        ),
    });
});

// Clear the startup screen and any paused overlay so the game runs.
async function keepRunning(deadlineMs) {
    const end = Date.now() + deadlineMs;
    while (Date.now() < end) {
        const startup = await page.locator('section.startup-screen').count();
        for (const label of ['Resume startup', 'Resume game']) {
            const button = page.getByRole('button', { name: label });
            if (await button.isVisible().catch(() => false))
                await button.click({ timeout: 2000 }).catch(() => {});
        }
        const close = page.locator('.controls-close');
        if (await close.isVisible().catch(() => false))
            await close.click({ timeout: 2000 }).catch(() => {});
        if (!startup) return true;
        await page.waitForTimeout(500);
    }
    return false;
}

const gameLog = () => page.evaluate(() => globalThis.benchAuditLog ?? []);
const started = () =>
    page.waitForFunction(() => globalThis.benchAuditGame?._portWebGetGfxTasks() > 3, undefined, {
        timeout: 120_000,
    });

async function firstLaunch() {
    current = { env: {}, ini: '[display]\nmaxfps=60\nmusic_volume=0\nsfx_volume=0\n', files: {} };
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    const play = page.getByRole('button', { name: 'Play GoldenEye' });
    await expect(play).toBeEnabled({ timeout: 90_000 });
    await play.click();
    await started();
    // The launcher drops its startup screen after 90 s even if the icon stage
    // (paused with the page) has not finished; wait for the stage itself.
    const iconsEnd = Date.now() + 300_000;
    while (Date.now() < iconsEnd) {
        await keepRunning(5000);
        if (!(await page.evaluate(() => benchAuditGame._portWebIconStageBusy()))) break;
    }
    assert.ok(await keepRunning(60_000), 'The first launch never left its startup screen');
    assert.equal(await page.evaluate(() => benchAuditGame._portWebIconStageBusy()), 0);
    await page.waitForTimeout(3000); // the icon cache reaches IndexedDB
}

async function measure(run) {
    current = { env: run.env, ini: run.ini, files: { [run.route_path]: run.route } };
    await page.goto(origin);
    await started();
    const end = Date.now() + RUN_TIMEOUT_MS;
    while (Date.now() < end) {
        await keepRunning(2000);
        if ((await gameLog()).some((line) => line.startsWith(DONE))) break;
        await page.waitForTimeout(1000);
    }
    const log = await gameLog();
    assert.ok(
        log.some((line) => line.startsWith(DONE)),
        `${run.log_name}: no frame log after ${RUN_TIMEOUT_MS / 60_000} minutes`,
    );
    const frames = await page.evaluate(
        (path) => benchAuditGame.FS.readFile(path, { encoding: 'utf8' }),
        run.frame_log,
    );
    await writeFile(resolve(output, run.log_name), frames);
    return log;
}

async function hostFacts(log) {
    const facts = await page.evaluate(() => {
        const gl = document.createElement('canvas').getContext('webgl2');
        const debug = gl?.getExtension('WEBGL_debug_renderer_info');
        const canvas = document.querySelector('#canvas');
        return {
            renderer: debug ? gl.getParameter(debug.UNMASKED_RENDERER_WEBGL) : 'masked',
            canvas: canvas ? `${canvas.width}x${canvas.height}` : 'unknown',
            platform: navigator.platform,
            threads: navigator.hardwareConcurrency,
        };
    });
    const manifest = await fetch(new URL('runtime-manifest.json', origin))
        .then((response) => response.json())
        .catch(() => ({}));
    return {
        ...facts,
        browser: `${kind} ${browser.version()}`,
        headed: process.env.MIDAS_HEADED === '1',
        runtime: manifest.version ?? 'unknown',
        wasm_sha256: wasmDigest,
        build: log.find((line) => line.startsWith('ge: sys: build ')) ?? 'unknown',
    };
}

let failure;
let lastLog = [];
try {
    await firstLaunch();
    for (const [index, run] of plan.runs.entries()) {
        const begun = Date.now();
        lastLog = await measure(run);
        console.log(
            `${index + 1}/${plan.runs.length} ${run.log_name} ${((Date.now() - begun) / 1000).toFixed(0)} s`,
        );
    }
    await writeFile(
        resolve(output, 'host.json'),
        JSON.stringify(await hostFacts(lastLog), null, 2) + '\n',
    );
} catch (error) {
    failure = error;
    await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
    await writeFile(
        resolve(output, 'failure-log.txt'),
        (await gameLog().catch(() => lastLog)).join('\n'),
    );
} finally {
    await browser.close();
}
if (failure) throw failure;
assert.deepEqual(pageErrors, [], 'Page errors: ' + pageErrors.join('; '));
