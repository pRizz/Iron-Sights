// Private real-ROM audio continuity diagnostic; never bundled or uploaded.
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import assert from 'node:assert/strict';
import { launchAuditBrowser } from './browser-choice.mjs';
import { installGameProbe, navigateToDam, captureGameplay } from './native-game-audit.mjs';
import { audioContinuity } from './audio-quality.mjs';

const [
    rom,
    folder = '../build/web-audio/current',
    origin = 'http://127.0.0.1:4177/Iron-Sights/',
] = process.argv.slice(2);
if (!rom) throw new Error('Usage: audio-quality-audit.mjs private-rom private-output [origin]');
const output = resolve(folder);
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
await installGameProbe(page);
const workletSource = await readFile(
    new URL('./audio-quality-worklet.js', import.meta.url),
    'utf8',
);
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
    await navigateToDam(page);
    await captureGameplay(page);
    await page.waitForTimeout(2000);
    await page.evaluate(async (source) => {
        const game = midasAuditGame;
        const audio = game.SDL2.audio;
        const node = audio.scriptProcessorNode;
        const original = node.onaudioprocess;
        globalThis.audioQuality = {
            rate: game.SDL2.audioContext.sampleRate,
            size: node.bufferSize,
            blocks: [],
            queue: [],
        };
        const moduleURL = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
        try {
            await game.SDL2.audioContext.audioWorklet.addModule(moduleURL);
        } finally {
            URL.revokeObjectURL(moduleURL);
        }
        globalThis.captureRenderedAudio = (seconds) =>
            new Promise((resolve, reject) => {
                const timeout = setTimeout(
                    () => reject(new Error('Audio render thread did not complete capture')),
                    (seconds + 10) * 1000,
                );
                const probe = new AudioWorkletNode(game.SDL2.audioContext, 'audio-quality-probe', {
                    outputChannelCount: [2],
                    processorOptions: { seconds },
                });
                probe.port.onmessage = (event) => {
                    clearTimeout(timeout);
                    node.disconnect();
                    probe.disconnect();
                    node.connect(game.SDL2.audioContext.destination);
                    resolve(Array.from(event.data));
                };
                node.disconnect();
                node.connect(probe);
                probe.connect(game.SDL2.audioContext.destination);
            });
        globalThis.audioQualityRendered = captureRenderedAudio(12);
        node.onaudioprocess = (event) => {
            original(event);
            // Read before dispatch returns: output buffers are cleared/recycled
            // before the microtask used by the older presence-only observer.
            audioQuality.blocks.push(Array.from(event.outputBuffer.getChannelData(0)));
            audioQuality.queue.push([performance.now(), game._portWebGetAudioMetric(1)]);
        };
    }, workletSource);
    const result = await page.evaluate(async () => {
        const samples = await audioQualityRendered;
        const { blocks, ...rest } = audioQuality;
        return {
            ...rest,
            samples,
            metrics: Array.from({ length: 10 }, (_, i) => midasAuditGame._portWebGetAudioMetric(i)),
        };
    });
    const samples = result.samples;
    delete result.samples;
    Object.assign(result, audioContinuity(samples, result.rate));
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
    header.writeUInt32LE(result.rate, 24);
    header.writeUInt32LE(result.rate * 2, 28);
    header.writeUInt16LE(2, 32);
    header.writeUInt16LE(16, 34);
    header.write('data', 36);
    header.writeUInt32LE(pcm.length, 40);
    await writeFile(resolve(output, 'audio.wav'), Buffer.concat([header, pcm]));
    await page.keyboard.press('P', { delay: 120 });
    await page.waitForFunction(() => !midasAuditGame._portWebControlsLive());
    await page.waitForTimeout(500);
    result.watch = audioContinuity(await page.evaluate(() => captureRenderedAudio(3)), result.rate);
    await page.keyboard.press('P', { delay: 120 });
    await page.waitForFunction(() => midasAuditGame._portWebControlsLive());
    await page.waitForTimeout(500);
    await page.evaluate(() => midasAuditGame._portWebSetPaused(1));
    await page.waitForTimeout(500);
    result.paused = audioContinuity(
        await page.evaluate(() => captureRenderedAudio(2)),
        result.rate,
    );
    await page.evaluate(() => midasAuditGame._portWebSetPaused(0));
    await page.waitForTimeout(1000);
    result.resumed = audioContinuity(
        await page.evaluate(() => captureRenderedAudio(3)),
        result.rate,
    );
    await writeFile(resolve(output, 'report.json'), JSON.stringify(result, null, 2));
    await writeFile(resolve(output, 'native-log.txt'), await page.locator('pre').textContent());
    console.log(JSON.stringify({ ...result, queue: `${result.queue.length} observations` }));
    assert.ok(result.size >= 512, 'Browser audio period regressed below the pre-1.0.2b baseline');
    assert.ok(
        result.peak > 0.001 && result.jumps === 0,
        'Idle Dam audio has discontinuities; inspect private WAV/report',
    );
    assert.equal(result.paused.peak, 0, 'Paused output was not silent');
    assert.ok(result.watch.peak > 0.001, 'Watch audio was silent');
    assert.ok(result.resumed.peak > 0.001, 'Audio did not resume');
} catch (error) {
    await writeFile(resolve(output, 'failure.txt'), String(error));
    await writeFile(
        resolve(output, 'native-log.txt'),
        await page
            .locator('pre')
            .textContent()
            .catch(() => ''),
    );
    throw error;
} finally {
    await browser.close();
}
