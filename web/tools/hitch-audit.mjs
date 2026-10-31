// Private actual-ROM long-run diagnostic. Only timing/counters/logs are saved.
import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { navigateToDam, captureGameplay } from './native-game-audit.mjs';
import { observeHitches, installHitchAudio } from './hitch-observer.mjs';
import { startBrowserTrace, reportBrowserTrace } from './hitch-cdp-trace.mjs';

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
const cdpEnabled = process.env.MIDAS_HITCH_CDP === '1';
const traceSeconds = Number(process.env.MIDAS_HITCH_TRACE_SECONDS || 60);
if (![menuSeconds, gameSeconds].every((seconds) => Number.isInteger(seconds) && seconds >= 1 && seconds <= 600))
    throw new Error('Hitch phase durations must be whole seconds from 1 to 600');
if (!Number.isInteger(traceSeconds) || traceSeconds < 1 || traceSeconds > 60)
    throw new Error('CDP search duration must be a whole number of seconds from 1 to 60');

const { browser, kind } = await launchAuditBrowser();
if (cdpEnabled && kind !== 'chromium') throw new Error('CDP tracing requires Chromium or Chrome');
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
    return page.evaluate((label) => {
        const at = performance.now();
        performance.mark(`midas-hitch:${label.replaceAll(' ', ':')}`);
        globalThis.midasHitchTrace.markers.push({ at, label });
        return at;
    }, name);
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

async function driveGameplay(seconds, stopped = () => false) {
    const until = Date.now() + seconds * 1000;
    let pass = 0;
    while (Date.now() < until && !stopped()) {
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

const browserReports = [];
let buildIdentity;
async function captureTracePhase(phase, seconds, retry = false) {
    const duration = Math.min(seconds, retry ? 30 : traceSeconds);
    const path = resolve(output, `${phase}${retry ? '-retry' : ''}.cdp.json.gz`);
    const controller = await startBrowserTrace(browser, path, retry ? 131072 : 65536);
    const anchorAt = await mark(`${phase} start`);
    const watch = page.evaluate(({ start, limit }) => new Promise((resolve) => {
        const deadline = performance.now() + limit * 1000;
        let found;
        const timer = setInterval(() => {
            if (!found) {
                found = globalThis.midasHitchTrace.frames.find((frame) =>
                    frame.at >= start && frame.gap >= 500);
                if (found) {
                    setTimeout(() => { clearInterval(timer); resolve(found); }, 2000);
                    clearInterval(timer);
                    return;
                }
            }
            if (performance.now() >= deadline) { clearInterval(timer); resolve(null); }
        }, 50);
    }), { start: anchorAt, limit: duration });
    let finished = false;
    const observed = watch.then((gap) => { finished = true; return gap; });
    let drivingError;
    const driving = phase === 'gameplay'
        ? driveGameplay(duration + 3, () => finished).catch((caught) => { drivingError = caught; })
        : null;
    let gap;
    let phaseError;
    let endAt;
    let recording;
    try {
        gap = await observed;
    } catch (caught) {
        phaseError = caught;
    } finally {
        endAt = await mark(`${phase} end`).catch(() => undefined);
        recording = await controller.stop();
    }
    if (driving) await driving;
    if (phaseError) throw phaseError;
    if (drivingError) throw drivingError;
    const state = await page.evaluate(({ start, end }) => {
        const canvas = document.querySelector('#canvas');
        const gl = canvas?.getContext('webgl2');
        const info = gl?.getExtension('WEBGL_debug_renderer_info');
        return {
            renderer: gl && (info ? gl.getParameter(info.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER)),
            canvas: canvas && { width: canvas.width, height: canvas.height },
            focused: document.hasFocus(), hidden: document.hidden,
            focusEvents: globalThis.midasHitchTrace.focus.filter((event) =>
                event.at >= start && event.at <= end),
            frameGapsOver500: globalThis.midasHitchTrace.frames.filter((frame) =>
                frame.at >= start && frame.at <= end && frame.gap >= 500).length,
            stage: globalThis.midasAuditGame?._portWebGetStage(),
        };
    }, { start: anchorAt, end: endAt });
    const hardwareComparisonEligible = !!state.renderer &&
        !/swiftshader|software|llvmpipe|microsoft basic render/i.test(state.renderer);
    const focusLost = !state.focused || state.hidden ||
        state.focusEvents.some((event) => event.name === 'blur' || event.hidden);
    const report = await reportBrowserTrace(path, phase, gap && { ...gap, anchorAt }, {
        phase, retry, searchCapSeconds: duration,
        capturedSeconds: (endAt - anchorAt) / 1000, incident: !!gap,
        build: buildIdentity, browser: await browser.version(), state, recording,
        hardwareComparisonEligible, focusLost,
    });
    report.accepted = !recording.dataLoss && report.analysis.anchorFound &&
        ['devtools.timeline', 'cc', 'viz', 'gpu'].every((track) =>
            report.analysis.tracks.includes(track)) &&
        !!buildIdentity?.version && hardwareComparisonEligible && !focusLost &&
        (phase === 'menu' ? state.stage === 90 : (gap?.stage || state.stage) === 33);
    await writeFile(path.replace(/\.json\.gz$/, '.report.json'), JSON.stringify(report, null, 2));
    browserReports.push(report);
    console.log(JSON.stringify({ phase, retry, incident: !!gap,
        gapMs: gap?.gap, dataLoss: recording.dataLoss,
        tracks: report.analysis.tracks, path }));
    if (recording.dataLoss && !retry) return captureTracePhase(phase, seconds, true);
    return report;
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
    buildIdentity = await page.evaluate(async () =>
        (await fetch('runtime-manifest.json', { cache: 'no-store' })).json());
    if (cdpEnabled) await captureTracePhase('menu', menuSeconds);
    else {
        await mark('menu start');
        await page.waitForTimeout(menuSeconds * 1000);
        await mark('menu end');
    }
    await navigateToDam(page);
    await captureGameplay(page);
    if (cdpEnabled) await captureTracePhase('gameplay', gameSeconds);
    else {
        await mark('gameplay start');
        await driveGameplay(gameSeconds);
        await mark('gameplay end');
    }
    console.log(cdpEnabled
        ? `CDP hitch audit captured menu and Dam with ${traceSeconds}s search caps; audio ${audioProbe}`
        : `Hitch audit targeted ${menuSeconds}s menu and at least ${gameSeconds}s scripted gameplay; audio ${audioProbe}`);
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
        cdpEnabled, buildIdentity, browserReports,
        trace, messages, error: error ? String(error) : null,
    }, null, 2));
    if (trace?.trace) {
        const data = trace.trace;
        const worst = [...data.frames].sort((a, b) => b.gap - a.gap).slice(0, 20);
        const summary = {
            browser: trace.browser, renderer: trace.renderer, canvas: trace.canvas,
            build: buildIdentity, browserVersion: await browser.version(), browserReports,
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
