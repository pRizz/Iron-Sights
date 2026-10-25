// Private representative mission/effect smoke. No mission completion is implied.
import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { launchAuditBrowser } from './browser-choice.mjs';
import { observeAudio, readAudio } from './audio-observer.mjs';

const [rom, directory, origin = 'http://127.0.0.1:4174', selected] = process.argv.slice(2);
if (!rom || !directory)
    throw new Error(
        'Usage: mission-audit.mjs local-rom output-directory [prototype-url] [comma-separated-stages]',
    );
const stages = {
    dam: 33,
    facility: 34,
    surface2: 43,
    jungle: 37,
    frigate: 26,
    caverns: 39,
    cradle: 41,
    egypt: 32,
};
const cases = selected ? selected.split(',') : Object.keys(stages);
if (cases.some((name) => !(name in stages))) throw new Error('Unknown mission audit stage');
const output = resolve(directory);
await mkdir(output, { recursive: true });
const build = JSON.parse(
    await readFile(
        new URL('../../build/web/video/generated/build-info.json', import.meta.url),
        'utf8',
    ),
);
const reports = [];
for (const stage of cases) {
    const { browser, kind } = await launchAuditBrowser();
    const context = await browser.newContext({ viewport: { width: 1100, height: 1000 } });
    const page = await context.newPage();
    await page.addInitScript(observeAudio);
    const messages = [];
    page.on('console', (message) => messages.push(`${message.type()}: ${message.text()}`));
    page.on('pageerror', (error) => messages.push('PAGE ERROR: ' + String(error)));
    const begin = Date.now();
    const snapshot = () =>
        page.evaluate(() => {
            const game = globalThis.midasPrototype;
            return {
                frames: game._portWebGetFrames(),
                tasks: game._portWebGetGfxTasks(),
                memory: game._portWebGetMemoryBytes(),
                stage: game._portWebGetStage(),
                position: [0, 1, 2].map((axis) => game._portWebGetPlayerPosition(axis)),
                locked: !!document.pointerLockElement,
            };
        });
    try {
        const url = new URL(origin);
        url.searchParams.set('audit', '1');
        url.searchParams.set('stage', stage);
        await page.goto(url.href);
        await page.locator('#rom').setInputFiles(resolve(rom));
        await page.locator('#play').click();
        await page.waitForFunction(
            (expected) => {
                const game = globalThis.midasPrototype;
                const live =
                    game && game._portWebGetStage() === expected && game._portWebControlsLive();
                globalThis.midasStableAt = live ? globalThis.midasStableAt || performance.now() : 0;
                return live && performance.now() - globalThis.midasStableAt > 1000;
            },
            stages[stage],
            { timeout: 90_000 },
        );
        await page.locator('#resume').click();
        await page.waitForFunction(() => !!document.pointerLockElement);
        const readyMs = Date.now() - begin;
        await page.waitForTimeout(1000);
        const initial = await snapshot();
        const measuredAt = Date.now();
        await page.screenshot({ path: resolve(output, `${stage}-scene.png`) });
        await page.keyboard.down('w');
        await page.waitForTimeout(500);
        await page.keyboard.up('w');
        await page.mouse.down({ button: 'right' });
        await page.waitForTimeout(700);
        await page.screenshot({ path: resolve(output, `${stage}-ads.png`) });
        await page.mouse.up({ button: 'right' });
        for (let shot = 0; shot < 4; shot++) {
            await page.mouse.down();
            await page.waitForTimeout(120);
            await page.mouse.up();
            await page.waitForTimeout(180);
        }
        await page.screenshot({ path: resolve(output, `${stage}-fire.png`) });
        await page.waitForTimeout(3000);
        const final = await snapshot();
        const elapsedMs = Date.now() - measuredAt;
        const audio = await page.evaluate(readAudio);
        const graphics = await page.evaluate(() => {
            const gl = document.querySelector('canvas').getContext('webgl2');
            const debug = gl.getExtension('WEBGL_debug_renderer_info');
            return {
                browser: navigator.userAgent,
                renderer: debug
                    ? gl.getParameter(debug.UNMASKED_RENDERER_WEBGL)
                    : gl.getParameter(gl.RENDERER),
            };
        });
        const errors = messages.filter((line) =>
            /PAGE ERROR|GL_INVALID|WebGL: INVALID|shader.*ERROR/i.test(line),
        );
        const report = {
            stage,
            kind,
            ...graphics,
            readyMs,
            initial,
            final,
            elapsedMs,
            pumpsPerSecond: ((final.frames - initial.frames) * 1000) / elapsedMs,
            graphicsPerSecond: ((final.tasks - initial.tasks) * 1000) / elapsedMs,
            audioPeak: Math.max(
                0,
                ...audio.map((sample) => Math.max(sample.peak, sample.outputPeak)),
            ),
            errors,
            messages,
            log: await page.locator('#log').textContent(),
        };
        reports.push(report);
        await writeFile(resolve(output, `${stage}.json`), JSON.stringify(report, null, 2));
        assert.equal(final.stage, stages[stage]);
        assert.ok(final.tasks > initial.tasks + 60, 'The mission stopped rendering');
        assert.ok(final.locked, 'The mission lost mouse capture');
        assert.ok(report.audioPeak > 0.00001, 'No mission audio reached Web Audio');
        assert.deepEqual(errors, []);
        console.log(
            JSON.stringify({
                stage,
                readyMs,
                fps: report.graphicsPerSecond,
                memory: final.memory,
                audioPeak: report.audioPeak,
            }),
        );
    } catch (error) {
        await page.screenshot({ path: resolve(output, `${stage}-failure.png`) }).catch(() => {});
        await writeFile(
            resolve(output, `${stage}-failure.log`),
            messages.join('\n') +
                '\n' +
                (await page
                    .locator('#log')
                    .textContent()
                    .catch(() => '')),
        );
        throw error;
    } finally {
        await browser.close();
    }
}
await writeFile(
    resolve(output, 'report.json'),
    JSON.stringify(
        {
            build,
            scope: 'Spawn, short movement, ADS, firing, audio and rendering; no mission completion',
            reports,
        },
        null,
        2,
    ),
);
