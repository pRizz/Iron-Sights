// Private actual-ROM long-run diagnostic. Only timing/counters/logs are saved.
import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { navigateToDam, captureGameplay } from './native-game-audit.mjs';
import { observeHitches, installHitchAudio } from './hitch-observer.mjs';

const [rom, folder = '../build/web-hitches/current', origin = 'http://127.0.0.1:4177/Iron-Sights/'] =
    process.argv.slice(2);
if (!rom) throw new Error('Usage: hitch-audit.mjs private-rom private-output [origin]');
const output = resolve(folder);
await mkdir(output, { recursive: true });
const menuSeconds = Number(process.env.MIDAS_HITCH_MENU_SECONDS || 60);
const gameSeconds = Number(process.env.MIDAS_HITCH_GAME_SECONDS || 120);
const invulnerable = process.env.MIDAS_HITCH_INVULN === '1';
const audioEnabled = process.env.MIDAS_HITCH_AUDIO !== '0';
const graphicsEnabled = process.env.MIDAS_HITCH_WEBGL !== '0';
if (![menuSeconds, gameSeconds].every((seconds) => Number.isInteger(seconds) && seconds >= 1 && seconds <= 600))
    throw new Error('Hitch phase durations must be whole seconds from 1 to 600');

const { browser } = await launchAuditBrowser();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
await page.addInitScript(observeHitches, { graphics: graphicsEnabled, invulnerable });
await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
    const response = await route.fetch();
    const source = await response.text();
    const marker = 'export default createMidas;';
    assert.ok(source.includes(marker), 'Runtime probe marker was not found');
    await route.fulfill({
        response,
        body: source.replace(marker, `
export default async function(options) {
    globalThis.midasHitchBeforeCreate(options);
    return globalThis.midasHitchAttach(await createMidas(options));
}`),
    });
});

const messages = [];
page.on('console', (message) => {
    if (messages.length < 4096) messages.push({ at: Date.now(), type: message.type(), text: message.text() });
});
page.on('pageerror', (error) => messages.push({ at: Date.now(), type: 'pageerror', text: String(error) }));

async function mark(name) {
    await page.evaluate((label) => globalThis.midasHitchTrace.markers.push({ at: performance.now(), label }), name);
}

function phaseStats(data, name) {
    const start = data.markers.find((marker) => marker.label === `${name} start`)?.at;
    const end = data.markers.find((marker) => marker.label === `${name} end`)?.at;
    if (start === undefined || end === undefined) return null;
    const frames = data.frames.filter((frame) => frame.at >= start && frame.at <= end);
    return {
        seconds: (end - start) / 1000,
        over100ms: frames.filter((frame) => frame.gap >= 100).length,
        over250ms: frames.filter((frame) => frame.gap >= 250).length,
        worstMs: Math.max(0, ...frames.map((frame) => frame.gap)),
        damSamples: data.samples.filter((sample) =>
            sample.at >= start && sample.at <= end && sample.stage === 33).length,
    };
}

async function driveGameplay(seconds) {
    const until = Date.now() + seconds * 1000;
    let pass = 0;
    while (Date.now() < until) {
        await mark(`gameplay pass ${++pass}`);
        await page.keyboard.down('w');
        await page.waitForTimeout(650);
        await page.keyboard.up('w');
        await page.mouse.move(600 + (pass % 3) * 35, 390, { steps: 4 });
        await page.mouse.down({ button: 'right' });
        await page.waitForTimeout(650);
        await page.mouse.down({ button: 'left' });
        await page.waitForTimeout(120);
        await page.mouse.up({ button: 'left' });
        await page.mouse.up({ button: 'right' });
        await page.waitForTimeout(2200);
    }
}

let error;
try {
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({ timeout: 90_000 });
    await page.getByRole('button', { name: 'Play GoldenEye' }).click();
    await page.waitForFunction(() => globalThis.midasAuditGame?._portWebGetGfxTasks() >= 240, undefined, {
        timeout: 120_000,
    });
    if (await page.locator('.controls-close').count()) await page.locator('.controls-close').click();
    if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
        await page.getByRole('button', { name: 'Resume game' }).click();
    const audioProbe = audioEnabled
        ? await page.evaluate(installHitchAudio,
            await readFile(new URL('./hitch-audio-worklet.js', import.meta.url), 'utf8'))
        : 'disabled';
    await mark('menu start');
    await page.waitForTimeout(menuSeconds * 1000);
    await mark('menu end');
    await navigateToDam(page);
    await captureGameplay(page);
    await mark('gameplay start');
    await driveGameplay(gameSeconds);
    await mark('gameplay end');
    console.log(`Hitch audit targeted ${menuSeconds}s menu and at least ${gameSeconds}s scripted gameplay; audio ${audioProbe}`);
} catch (caught) {
    error = caught;
} finally {
    const trace = await page.evaluate(() => {
        const game = globalThis.midasAuditGame;
        const context = game?.SDL2?.audioContext;
        const canvas = document.querySelector('#canvas');
        const gl = canvas?.getContext('webgl2');
        const info = gl?.getExtension('WEBGL_debug_renderer_info');
        return {
            capturedAt: new Date().toISOString(),
            browser: navigator.userAgent,
            renderer: gl && (info ? gl.getParameter(info.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER)),
            canvas: canvas && { width: canvas.width, height: canvas.height },
            audioState: context?.state,
            audioClock: globalThis.midasHitchAudioClock?.(),
            trace: globalThis.midasHitchTrace,
        };
    }).catch(() => null);
    await writeFile(resolve(output, 'trace.json'), JSON.stringify({
        menuSeconds, gameSeconds, invulnerable, audioEnabled, graphicsEnabled,
        trace, messages, error: error ? String(error) : null,
    }, null, 2));
    if (trace?.trace) {
        const data = trace.trace;
        const worst = [...data.frames].sort((a, b) => b.gap - a.gap).slice(0, 20);
        const summary = {
            browser: trace.browser, renderer: trace.renderer, canvas: trace.canvas,
            invulnerable, audioEnabled, graphicsEnabled,
            audioState: trace.audioState, audioClock: trace.audioClock,
            markers: data.markers, worstFrames: worst,
            phases: { menu: phaseStats(data, 'menu'), gameplay: phaseStats(data, 'gameplay') },
            frameGapsOver100ms: data.frames.filter((frame) => frame.gap >= 100).length,
            frameGapsOver250ms: data.frames.filter((frame) => frame.gap >= 250).length,
            longTasks: data.longTasks.length, audioReports: data.audio.length,
            syncs: data.syncs.length, error: error ? String(error) : null,
        };
        await writeFile(resolve(output, 'summary.json'), JSON.stringify(summary, null, 2));
        console.log(JSON.stringify({ output, renderer: summary.renderer,
            phases: summary.phases, worstFrames: worst.slice(0, 5), error: summary.error }));
    }
    await browser.close();
}
if (error) throw error;
