// Private real-ROM audit of audio across screen transitions and mission start.
// The rendered WAV and timing reports stay local; nothing is bundled or uploaded.
import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { navigateToDam } from './native-game-audit.mjs';
import { observeHitches } from './hitch-observer.mjs';
import { audioDropouts } from './audio-quality.mjs';

const [
    rom,
    folder = '../build/web-audio-transition/current',
    origin = 'http://127.0.0.1:4177/Iron-Sights/',
] = process.argv.slice(2);
if (!rom) throw new Error('Usage: audio-transition-audit.mjs private-rom private-output [origin]');
const output = resolve(folder);
await mkdir(output, { recursive: true });
const recordSeconds = Number(process.env.MIDAS_AUDIO_RECORD_SECONDS || 75);
const afterDamSeconds = Number(process.env.MIDAS_AUDIO_AFTER_DAM_SECONDS || 25);
const STALL_MS = 40;
const FORCED_STALLS_MS = [60, 150, 600];
// Hide AudioWorklet from the game, not the recorder, to exercise SDL's fallback.
const forceSdlOutput = process.env.MIDAS_AUDIO_FORCE_SDL === '1';

// Remember whichever node feeds the speakers, so the recorder works on any backend.
function trackOutputNode(hideWorklet) {
    globalThis.midasRecorderNodeType = AudioWorkletNode;
    if (hideWorklet) delete globalThis.AudioWorkletNode;
    const connect = AudioNode.prototype.connect;
    AudioNode.prototype.connect = function (destination, ...rest) {
        if (destination instanceof AudioDestinationNode) globalThis.midasOutputNode = this;
        return Reflect.apply(connect, this, [destination, ...rest]);
    };
}

async function installRecorder({ source, seconds }) {
    const node = globalThis.midasOutputNode;
    if (!node) throw new Error('No audio node is connected to the output');
    const context = node.context;
    const url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
    try {
        await context.audioWorklet.addModule(url);
    } finally {
        URL.revokeObjectURL(url);
    }
    const recorder = new globalThis.midasRecorderNodeType(context, 'audio-transition-recorder', {
        outputChannelCount: [2],
        processorOptions: { seconds },
    });
    node.disconnect();
    node.connect(recorder);
    recorder.connect(context.destination);
    globalThis.midasRecorder = recorder;
    globalThis.midasRecorderAnchor = {
        contextSeconds: context.currentTime,
        pageMs: performance.now(),
    };
    return context.sampleRate;
}

function finishRecorder() {
    return new Promise((resolve) => {
        globalThis.midasRecorder.port.onmessage = ({ data }) =>
            resolve({ ...data, samples: Array.from(data.samples) });
        globalThis.midasRecorder.port.postMessage('finish');
    });
}

function monoWav(samples, rate) {
    const pcm = Buffer.alloc(samples.length * 2);
    samples.forEach((value, i) =>
        pcm.writeInt16LE(Math.round(Math.max(-1, Math.min(1, value)) * 32767), i * 2),
    );
    const header = Buffer.alloc(44);
    header.write('RIFF');
    header.writeUInt32LE(pcm.length + 36, 4);
    header.write('WAVEfmt ', 8);
    header.writeUInt32LE(16, 16);
    header.writeUInt16LE(1, 20);
    header.writeUInt16LE(1, 22);
    header.writeUInt32LE(rate, 24);
    header.writeUInt32LE(rate * 2, 28);
    header.writeUInt16LE(2, 32);
    header.writeUInt16LE(16, 34);
    header.write('data', 36);
    header.writeUInt32LE(pcm.length, 40);
    return Buffer.concat([header, pcm]);
}

const { browser } = await launchAuditBrowser();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
await page.addInitScript(trackOutputNode, forceSdlOutput);
await page.addInitScript(observeHitches, { graphics: true });
await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
    const response = await route.fetch();
    const source = await response.text();
    await route.fulfill({
        response,
        body: source.replace(
            'export default createMidas;',
            `export default async function(options) {
    globalThis.midasHitchBeforeCreate(options);
    return globalThis.midasHitchAttach(await createMidas(options));
}`,
        ),
    });
});

let failure;
try {
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 90000,
    });
    await page.getByRole('button', { name: 'Play GoldenEye' }).click();
    await page.waitForFunction(
        () => globalThis.midasAuditGame?._portWebGetGfxTasks() >= 240,
        undefined,
        { timeout: 120000 },
    );
    if (await page.locator('.controls-close').count())
        await page.locator('.controls-close').click();
    if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
        await page.getByRole('button', { name: 'Resume game' }).click();
    await page.waitForFunction(() => globalThis.midasOutputNode, undefined, { timeout: 30000 });
    await page.evaluate(installRecorder, {
        source: await readFile(new URL('./audio-transition-worklet.js', import.meta.url), 'utf8'),
        seconds: recordSeconds,
    });
    const startedAt = Date.now();
    await navigateToDam(page);
    const damReachedMs = await page.evaluate(() => performance.now());
    await page.waitForTimeout(6000);
    // Deliberate stalls: one the deeper buffer should bridge, one it cannot.
    const forcedStalls = [];
    for (const milliseconds of FORCED_STALLS_MS) {
        forcedStalls.push({
            milliseconds,
            atPageMs: await page.evaluate((duration) => {
                const start = performance.now();
                while (performance.now() - start < duration);
                return start;
            }, milliseconds),
        });
        await page.waitForTimeout(3000);
    }
    const remaining = recordSeconds * 1000 - (Date.now() - startedAt) - 1000;
    await page.waitForTimeout(Math.max(0, Math.min(afterDamSeconds * 1000, remaining)));

    const recording = await page.evaluate(finishRecorder);
    const page_ = await page.evaluate(() => {
        const trace = globalThis.midasHitchTrace;
        return {
            anchor: globalThis.midasRecorderAnchor,
            latencySeconds: {
                base: globalThis.midasOutputNode.context.baseLatency,
                output: globalThis.midasOutputNode.context.outputLatency,
            },
            frames: trace.frames,
            longTasks: trace.longTasks,
            samples: trace.samples,
            logs: trace.logs.filter(({ line }) => line.includes('audio')),
            graphics: trace.graphics,
            metrics: Array.from({ length: 13 }, (_, i) =>
                globalThis.midasAuditGame._portWebGetAudioMetric(i),
            ),
        };
    });
    await writeFile(resolve(output, 'audio.wav'), monoWav(recording.samples, recording.rate));

    const recordedMs = (atSeconds) =>
        page_.anchor.pageMs +
        (recording.startedAt + atSeconds - page_.anchor.contextSeconds) * 1000;
    const dropouts = audioDropouts(recording.samples, recording.rate);
    const timeline = dropouts.map((event) => ({ ...event, atPageMs: recordedMs(event.atSeconds) }));
    const stalls = page_.frames
        .filter((frame) => frame.gap >= STALL_MS)
        .map(({ at, gap, stage }) => ({ endsAtPageMs: at, gapMs: gap, stage }));
    const faultsNearStall = timeline.filter((event) =>
        stalls.some(
            (stall) =>
                event.atPageMs >= stall.endsAtPageMs - stall.gapMs - 100 &&
                event.atPageMs <= stall.endsAtPageMs + 100,
        ),
    ).length;
    const report = {
        sampleRate: recording.rate,
        recordedSeconds: recording.samples.length / recording.rate,
        damReachedMs,
        forcedStalls: forcedStalls.map((stall) => ({
            ...stall,
            faults: timeline.filter(
                (event) =>
                    event.atPageMs >= stall.atPageMs - 50 &&
                    event.atPageMs <= stall.atPageMs + stall.milliseconds + 200,
            ),
        })),
        peak: recording.samples.reduce((peak, value) => Math.max(peak, Math.abs(value)), 0),
        counts: {
            silence: dropouts.filter((event) => event.kind === 'silence').length,
            hardSilence: dropouts.filter((event) => event.kind === 'silence' && event.hard).length,
            repeat: dropouts.filter((event) => event.kind === 'repeat').length,
            faultsNearStall,
            frameStalls: stalls.length,
            longTasks: page_.longTasks.length,
        },
        worstStallMs: Math.max(0, ...stalls.map((stall) => stall.gapMs)),
        worstFaultMs: Math.max(0, ...dropouts.map((event) => event.milliseconds)),
        metrics: page_.metrics,
        latencySeconds: page_.latencySeconds,
        timeline,
        stalls,
        longTasks: page_.longTasks,
        graphics: page_.graphics,
        queueSamples: page_.samples,
        audioLogs: page_.logs,
    };
    await writeFile(resolve(output, 'report.json'), JSON.stringify(report, null, 2));
    const {
        timeline: _t,
        stalls: _s,
        longTasks: _l,
        graphics: _g,
        queueSamples: _q,
        ...summary
    } = report;
    console.log(JSON.stringify(summary, null, 2));
    assert.ok(report.peak > 0.001, 'The recording was silent');
    assert.equal(page_.metrics[11], forceSdlOutput ? 0 : 1, 'Unexpected browser audio output');
    if (!forceSdlOutput) {
        assert.equal(report.counts.repeat, 0, 'Audio replayed a stale buffer; inspect private WAV');
        assert.equal(report.counts.hardSilence, 0, 'Audio stopped without a fade');
        assert.deepEqual(report.forcedStalls[0].faults, [], 'A 60 ms stall reached the output');
    }
} catch (error) {
    failure = error;
    await writeFile(resolve(output, 'failure.txt'), String(error));
} finally {
    await browser.close();
}
if (failure) throw failure;
