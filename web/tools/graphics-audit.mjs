// Private real-ROM check: FORK-082 Graphics (Tone Mapping, Looks, Auto Exposure,
// Baked Lighting) in the browser build. One browser profile: a first launch
// prepares the weapon icons (so no case waits on that stage), then each case
// reloads the remembered cartridge with its own midas.ini and GE_STAGE, waits
// for control (and the bake), saves a canvas capture and measures frame rate.
// The hotkeys case presses F3 (checking the saved setting) and F8 (Compare). MIDAS_ANGLE=d3d11 picks the GPU on Windows.
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

const [
    rom,
    folder = '../build/graphics-audit/browser',
    origin = 'http://127.0.0.1:4179/Iron-Sights/',
    selected,
] = process.argv.slice(2);
if (!rom) throw new Error('Usage: graphics-audit.mjs private-rom private-output [origin] [cases]');
const output = resolve(folder);
await mkdir(output, { recursive: true });

const levels = { dam: 33, facility: 34, surface: 36 };
const stock = { tone_mapping: 0, baked_lighting: 0 };
const vibrant = { tone_mapping: 1, graphics_look: 2, baked_lighting: 1 };
const cases = {
    'stock-dam': ['dam', stock],
    'vibrant-dam': ['dam', vibrant],
    'stock-surface': ['surface', stock],
    'vibrant-surface': ['surface', vibrant],
    'stock-facility': ['facility', stock],
    'vibrant-facility': ['facility', vibrant],
    'noir-dam': ['dam', { tone_mapping: 1, graphics_look: 4, baked_lighting: 1 }],
    'defaults-dam': ['dam', null], // no Graphics keys: this build's defaults
    hotkeys: ['dam', { tone_mapping: 1, graphics_look: 2, baked_lighting: 0 }],
};
const names = selected ? selected.split(',') : Object.keys(cases);
if (names.some((name) => !(name in cases))) throw new Error('Unknown graphics audit case');
const notable =
    /tone mapping|auto exposure|^bake:|Baked Lighting|GL_INVALID|WebGL|shader.*(error|fail)/i;

function iniFor(settings) {
    const lines = [
        '[display]',
        'maxfps=60',
        'msaa=2',
        'setting_help=0',
        'music_volume=0',
        'sfx_volume=0',
    ];
    for (const [key, value] of Object.entries(settings ?? {})) lines.push(`${key}=${value}`);
    return lines.join('\n') + '\n';
}

// The case the next game launch takes; the route reads it at each page load.
let current = { env: {}, ini: null };
const { browser, kind } = await launchAuditBrowser({
    angleBackend: process.env.MIDAS_ANGLE || undefined,
});
const page = await browser.newPage({ viewport: { width: 1280, height: 960 } });
const logs = [];
page.on('console', (message) => {
    if (logs.length < 16384) logs.push(`${message.type()}: ${message.text()}`);
});
page.on('pageerror', (error) => logs.push(`pageerror: ${error}`));
await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
    const response = await route.fetch();
    const source = await response.text();
    assert.ok(source.includes('export default createMidas;'));
    await route.fulfill({
        response,
        body: source.replace(
            'export default createMidas;',
            `export default async function(options) {
    globalThis.graphicsAuditLog = [];
    for (const key of ['print', 'printErr']) {
        const originalPrint = options[key];
        options[key] = function(line) { graphicsAuditLog.push(line); originalPrint(line); };
    }
    const previous = options.preRun;
    options.preRun = (module) => {
        previous?.(module);
        Object.assign(module.ENV, ${JSON.stringify(current.env)});
    };
    const game = await createMidas(options);
    globalThis.graphicsAuditGame = game;
    const original = game.callMain;
    game.callMain = function(args) {
        const ini = ${JSON.stringify(current.ini)};
        if (ini !== null) game.FS.writeFile('/user/midas.ini', ini);
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
        if (
            await page
                .locator('.controls-close')
                .isVisible()
                .catch(() => false)
        )
            await page
                .locator('.controls-close')
                .click({ timeout: 2000 })
                .catch(() => {});
        if (!startup) return true;
        await page.waitForTimeout(500);
    }
    return false;
}

const gameLog = () => page.evaluate(() => globalThis.graphicsAuditLog ?? []);
const reports = [];
let failure;
try {
    // First launch: the weapon icon stage, behind the startup screen.
    current = { env: {}, ini: iniFor(stock) };
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 90000,
    });
    await page.getByRole('button', { name: 'Play GoldenEye' }).click();
    await page.waitForFunction(
        () => globalThis.graphicsAuditGame?._portWebGetGfxTasks() > 3,
        undefined,
        {
            timeout: 120000,
        },
    );
    // The launcher drops its startup screen after 90 s even if the icon stage
    // (paused with the page) has not finished; wait for the stage itself.
    const iconsEnd = Date.now() + 300000;
    while (Date.now() < iconsEnd) {
        await keepRunning(5000);
        if (!(await page.evaluate(() => graphicsAuditGame._portWebIconStageBusy()))) break;
    }
    assert.ok(await keepRunning(60000), 'The first launch never left its startup screen');
    assert.equal(await page.evaluate(() => graphicsAuditGame._portWebIconStageBusy()), 0);
    await page.waitForTimeout(3000); // the icon cache reaches IndexedDB
    const icons = (await gameLog()).filter((line) => /icons: (prepared|ready|cache)/.test(line));
    console.log(JSON.stringify({ firstLaunch: icons.at(-1) ?? 'no icon lines' }));

    for (const name of names) {
        const [stage, settings] = cases[name];
        current = {
            env: {
                GE_STAGE: stage,
                GE_DIFF: '0',
                GE_STAGE_AFTER: '60',
                GE_CHEATS: 'invuln,invis',
                GE_SEED: '1',
                GE_SKIPINTRO: '1',
            },
            ini: iniFor(settings),
        };
        const report = { name, stage, settings, kind };
        reports.push(report);
        try {
            await page.goto(origin);
            await page.waitForFunction(
                () => globalThis.graphicsAuditGame?._portWebGetGfxTasks() > 3,
                undefined,
                { timeout: 120000 },
            );
            await keepRunning(60000);
            await page.waitForFunction(
                (level) => {
                    const game = globalThis.graphicsAuditGame;
                    const live = game._portWebControlsLive() && game._portWebGetStage() === level;
                    globalThis.graphicsLiveAt = live
                        ? globalThis.graphicsLiveAt || performance.now()
                        : 0;
                    return live && performance.now() - globalThis.graphicsLiveAt > 1000;
                },
                levels[stage],
                { timeout: 120000 },
            );
            await keepRunning(5000);
            const liveAt = Date.now();
            const baking = settings === null || settings.baked_lighting === 1;
            if (baking) {
                const ready = await page
                    .waitForFunction(
                        () => graphicsAuditLog.some((line) => line.startsWith('bake:')),
                        undefined,
                        { timeout: 180000 },
                    )
                    .then(() => true)
                    .catch(() => false);
                report.bakeReadyMs = ready ? Date.now() - liveAt : null;
            }
            await keepRunning(5000);
            await page.waitForTimeout(2500); // Auto Exposure settles in about 2 s
            await page.locator('#canvas').screenshot({ path: resolve(output, `${name}.png`) });
            const sample = () =>
                page.evaluate(() => [graphicsAuditGame._portWebGetGfxTasks(), performance.now()]);
            const before = await sample();
            await page.waitForTimeout(5000);
            const after = await sample();
            report.fps = ((after[0] - before[0]) * 1000) / (after[1] - before[1]);
            const readIni = () =>
                page.evaluate(() =>
                    graphicsAuditGame.FS.readFile('/user/midas.ini', { encoding: 'utf8' }),
                );
            if (name === 'hotkeys') {
                // Keys reach the game once the page has focus and the mouse is
                // captured; the game samples held keys once a frame, so hold them.
                await page.bringToFront();
                await page.locator('#canvas').click();
                await page.waitForTimeout(500);
                await page.keyboard.press('F3', { delay: 150 });
                await expect.poll(async () => /^tone_mapping=0$/m.test(await readIni())).toBe(true);
                await page.waitForTimeout(500);
                await page
                    .locator('#canvas')
                    .screenshot({ path: resolve(output, 'hotkeys-f3-off.png') });
                await page.keyboard.press('F3', { delay: 150 });
                await expect.poll(async () => /^tone_mapping=1$/m.test(await readIni())).toBe(true);
                // Compare is F8 in the browser, which keeps F11 for fullscreen.
                await page.keyboard.press('F8', { delay: 150 });
                await page.waitForTimeout(800);
                await page
                    .locator('#canvas')
                    .screenshot({ path: resolve(output, 'hotkeys-f8-compare.png') });
                await page.keyboard.press('F8', { delay: 150 });
            }
            const log = await gameLog();
            report.notable = log.filter((line) => notable.test(line)).slice(0, 40);
            report.hitches = log.filter((line) => line.includes('ge: hitch:')).length;
            // the game's own per-second stats: gl = GL submit ms per frame
            report.stats = log.filter((line) => line.startsWith('ge: stats:')).slice(-3);
            report.ini = (await readIni())
                .split('\n')
                .filter((line) =>
                    /^(tone_|graphics_|baked_|vibrance|auto_exposure|adapt_speed|msaa|window_)/.test(
                        line,
                    ),
                );
        } catch (error) {
            report.failure = String(error);
            await page.screenshot({ path: resolve(output, `${name}-failure.png`) }).catch(() => {});
            report.log = await gameLog().catch(() => []);
        }
        console.log(
            JSON.stringify({
                name,
                fps: report.fps && Math.round(report.fps * 10) / 10,
                bakeReadyMs: report.bakeReadyMs,
                hitches: report.hitches,
                failure: report.failure?.slice(0, 300),
            }),
        );
    }
} catch (error) {
    failure = error;
    await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
} finally {
    const renderer = await page
        .evaluate(() => {
            const gl = document.createElement('canvas').getContext('webgl2');
            const debug = gl.getExtension('WEBGL_debug_renderer_info');
            return {
                renderer: debug ? gl.getParameter(debug.UNMASKED_RENDERER_WEBGL) : 'masked',
                floatTargets: !!gl.getExtension('EXT_color_buffer_float'),
            };
        })
        .catch(() => ({}));
    const errors = logs.filter((line) =>
        /pageerror|GL_INVALID|WebGL: INVALID|shader.*ERROR/i.test(line),
    );
    await writeFile(
        resolve(output, 'report.json'),
        JSON.stringify({ renderer, errors, reports }, null, 2),
    );
    await writeFile(resolve(output, 'console.txt'), logs.join('\n'));
    await browser.close();
    if (failure) throw failure;
    assert.deepEqual(errors, [], 'WebGL or page errors; see console.txt');
    assert.ok(
        reports.every((report) => !report.failure),
        'A graphics audit case failed',
    );
}
