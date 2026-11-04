// Room frame-pacing benchmark on this host's real GPU. Results stay in ignored build/.
// Usage: room-benchmark.mjs [local-rom] [private-output] [origin]
// Without a ROM only the room scenarios run; the ROM adds the insert/TV-flight scenario.
// The first flight uses a fresh browser profile (cold GPU shader cache); later flights
// clear site storage but keep that cache, like a returning visitor's browser.
import { chromium, expect } from '@playwright/test';
import { mkdir, rm, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';

const [romPath, outputPath = '../build/web-room/benchmark', origin = 'http://127.0.0.1:4176'] =
    process.argv.slice(2);
const runs = Number(process.env.MIDAS_BENCH_RUNS || 3);
const idleMS = Number(process.env.MIDAS_BENCH_IDLE_MS || 6000);
const scale = Number(process.env.MIDAS_BENCH_DPR || 1.25);
const label = process.env.MIDAS_BENCH_LABEL || 'benchmark';
const viewport = { width: 1536, height: 864 };
const output = resolve(outputPath);
const profile = resolve(output, `profile-${label}`);
await rm(profile, { recursive: true, force: true });
await mkdir(output, { recursive: true });

// Headed with occlusion throttling disabled, so frames reflect the GPU, not window state.
const context = await chromium.launchPersistentContext(profile, {
    headless: false,
    channel: process.env.MIDAS_BROWSER || 'chrome',
    viewport,
    deviceScaleFactor: scale,
    args: [
        '--disable-backgrounding-occluded-windows',
        '--disable-renderer-backgrounding',
        '--disable-background-timer-throttling',
    ],
});

function observeFrames() {
    const samples = [];
    const loaf = [];
    const pending = [];
    let lastRoomFrames = '';
    let maybeFrame;
    let maybeGL;
    let maybeTimer;
    const getContext = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (type, ...rest) {
        const context = getContext.call(this, type, ...rest);
        if (type === 'webgl2' && context && this.classList.contains('room-canvas')) {
            maybeGL = context;
            maybeTimer = context.getExtension('EXT_disjoint_timer_query_webgl2');
        }
        return context;
    };
    // GPU time of the room context's work, where the browser exposes timer queries.
    const collect = () => {
        while (pending.length && maybeGL && !maybeGL.isContextLost()) {
            const { query, frame } = pending[0];
            if (!maybeGL.getQueryParameter(query, maybeGL.QUERY_RESULT_AVAILABLE)) break;
            if (!maybeGL.getParameter(maybeTimer.GPU_DISJOINT_EXT))
                frame.gpu =
                    (frame.gpu ?? 0) + maybeGL.getQueryParameter(query, maybeGL.QUERY_RESULT) / 1e6;
            maybeGL.deleteQuery(query);
            pending.shift();
        }
    };
    const raf = globalThis.requestAnimationFrame.bind(globalThis);
    // Sum every rAF callback's main-thread work per frame; note whether the room drew.
    globalThis.requestAnimationFrame = (callback) =>
        raf((time) => {
            if (maybeFrame?.time !== time) {
                maybeFrame = { time, work: 0, room: false, phase: '', stage: '' };
                samples.push(maybeFrame);
            }
            const frame = maybeFrame;
            const timed = maybeTimer && maybeGL && !maybeGL.isContextLost();
            let maybeQuery;
            if (timed) {
                collect();
                maybeQuery = maybeGL.createQuery();
                maybeGL.beginQuery(maybeTimer.TIME_ELAPSED_EXT, maybeQuery);
            }
            const start = performance.now();
            try {
                callback(time);
            } finally {
                frame.work += performance.now() - start;
                if (maybeQuery && !maybeGL.isContextLost()) {
                    maybeGL.endQuery(maybeTimer.TIME_ELAPSED_EXT);
                    pending.push({ query: maybeQuery, frame });
                }
                const canvas = document.querySelector('.room-canvas');
                const frames = canvas?.dataset.roomFrames ?? '';
                if (frames !== lastRoomFrames) {
                    lastRoomFrames = frames;
                    frame.room = true;
                }
                frame.phase = document.querySelector('.room-layer')?.dataset.phase ?? 'gone';
                frame.stage = canvas?.dataset.roomLoadingLabel ?? '';
            }
        });
    try {
        new PerformanceObserver((list) => {
            for (const entry of list.getEntries())
                loaf.push({ start: entry.startTime, blocking: entry.blockingDuration });
        }).observe({ type: 'long-animation-frame', buffered: true });
    } catch {}
    globalThis.roomBenchmark = {
        gpuTimers: () => !!maybeTimer,
        reset() {
            samples.length = 0;
            loaf.length = 0;
        },
        take() {
            collect();
            return { samples: samples.slice(), loaf: loaf.slice() };
        },
    };
}

const percentile = (values, p) => {
    if (!values.length) return 0;
    const sorted = [...values].sort((a, b) => a - b);
    return sorted[Math.min(sorted.length - 1, Math.floor((p / 100) * sorted.length))];
};
const round = (value) => Math.round(value * 100) / 100;
const mean = (values) => values.reduce((sum, value) => sum + value, 0) / Math.max(1, values.length);

function summarize(samples, loaf) {
    const drawn = samples.filter((sample) => sample.room);
    if (drawn.length < 2) return { frames: drawn.length };
    const intervals = drawn.slice(1).map((sample, index) => sample.time - drawn[index].time);
    const start = drawn[0].time;
    const end = drawn.at(-1).time;
    const blocking = loaf
        .filter((entry) => entry.start >= start && entry.start <= end)
        .reduce((sum, entry) => sum + entry.blocking, 0);
    const work = drawn.map((sample) => sample.work);
    const gpu = drawn.filter((sample) => sample.gpu !== undefined).map((sample) => sample.gpu);
    return {
        frames: drawn.length,
        fps: round(((drawn.length - 1) * 1000) / (end - start)),
        intervalP50: round(percentile(intervals, 50)),
        intervalP95: round(percentile(intervals, 95)),
        intervalP99: round(percentile(intervals, 99)),
        intervalMax: round(Math.max(...intervals)),
        hitches50: intervals.filter((interval) => interval > 50).length,
        hitches100: intervals.filter((interval) => interval > 100).length,
        workMean: round(mean(work)),
        workP95: round(percentile(work, 95)),
        ...(gpu.length ? { gpuMean: round(mean(gpu)), gpuP95: round(percentile(gpu, 95)) } : {}),
        blockingMSPerSecond: round((blocking * 1000) / Math.max(1, end - start)),
    };
}

async function openRoom() {
    const page = await context.newPage();
    const cdp = await context.newCDPSession(page);
    // Forget remembered ROMs and saves, but keep the GPU shader cache.
    await cdp.send('Storage.clearDataForOrigin', {
        origin: new URL(origin).origin,
        storageTypes:
            'cookies,file_systems,indexeddb,local_storage,cache_storage,service_workers,storage_buckets',
    });
    await page.addInitScript(observeFrames);
    await page.goto(origin);
    await expect(page.locator('.room-canvas')).toHaveAttribute('data-room-state', 'ready', {
        timeout: 60_000,
    });
    // The first frames include shader compilation and artwork upload: report them separately.
    await page.waitForFunction(
        () => Number(document.querySelector('.room-canvas').dataset.roomFrames) >= 1,
    );
    await page.waitForTimeout(2500);
    const { samples, loaf } = await page.evaluate(() => globalThis.roomBenchmark.take());
    const first = samples.find((sample) => sample.room);
    const startup = { firstFrameMS: round(first?.time ?? 0), ...summarize(samples, loaf) };
    return { page, startup };
}

async function measureIdle(pointer) {
    const { page, startup } = await openRoom();
    try {
        await page.evaluate(() => globalThis.roomBenchmark.reset());
        const until = Date.now() + idleMS;
        if (pointer) {
            // Sweep across the props so hover raycasts run each pointer event.
            for (let step = 0; Date.now() < until; step++) {
                const t = (step % 120) / 120;
                await page.mouse.move(
                    200 + Math.sin(t * Math.PI * 2) * 180 + t * 1000,
                    300 + Math.cos(t * Math.PI * 2) * 200,
                );
                await page.waitForTimeout(8);
            }
        } else await page.waitForTimeout(idleMS);
        const { samples, loaf } = await page.evaluate(() => globalThis.roomBenchmark.take());
        const trace = process.env.MIDAS_BENCH_TRACE === '1' ? { samples } : {};
        return { startup, steady: summarize(samples, loaf), ...trace };
    } finally {
        await page.close();
    }
}

async function measureFlight() {
    const { page } = await openRoom();
    try {
        await page.evaluate(() => globalThis.roomBenchmark.reset());
        await page.getByRole('button', { name: 'Insert GoldenEye cartridge' }).focus();
        await page.keyboard.press('Enter');
        await page.locator('.rom-dialog input[type=file]').setInputFiles(resolve(romPath));
        await expect(page.locator('.room-layer')).toHaveCount(0, { timeout: 120_000 });
        const { samples, loaf } = await page.evaluate(() => globalThis.roomBenchmark.take());
        const phases = {};
        for (const phase of ['inserting', 'powering', 'flying', 'screen', 'complete']) {
            const selected = samples.filter((sample) => sample.phase === phase);
            if (selected.length) phases[phase] = summarize(selected, loaf);
        }
        const cinematic = samples.filter((sample) =>
            ['inserting', 'powering', 'flying'].includes(sample.phase),
        );
        // Attribute frame gaps to the startup stage shown when each gap began.
        const stages = {};
        // The unmount marker after the reveal crossfade is not a drawn frame.
        const drawn = samples.filter((sample) => sample.room && sample.phase !== 'gone');
        for (let index = 1; index < drawn.length; index++) {
            const gap = drawn[index].time - drawn[index - 1].time;
            const stage = drawn[index - 1].stage || '(none)';
            stages[stage] ??= { frames: 0, gapMS: 0, maxGap: 0 };
            stages[stage].frames++;
            stages[stage].gapMS = round(stages[stage].gapMS + gap);
            stages[stage].maxGap = round(Math.max(stages[stage].maxGap, gap));
        }
        const handoff = drawn.length ? round(drawn.at(-1).time - drawn[0].time) : 0;
        // MIDAS_BENCH_TRACE keeps raw samples for locating individual stalls.
        const trace = process.env.MIDAS_BENCH_TRACE === '1' ? { samples, loaf } : {};
        return {
            selectToHandoffMS: handoff,
            cinematic: summarize(cinematic, loaf),
            phases,
            stages,
            ...trace,
        };
    } finally {
        await page.close();
    }
}

async function describeHost() {
    const page = await context.newPage();
    await page.goto(new URL('LICENSE.txt', origin + '/').href);
    const host = await page.evaluate(async () => {
        const gl = document.createElement('canvas').getContext('webgl2', {
            powerPreference: 'low-power',
        });
        const info = gl?.getExtension('WEBGL_debug_renderer_info');
        const renderer = info ? gl.getParameter(info.UNMASKED_RENDERER_WEBGL) : 'unknown';
        const timers = !!gl?.getExtension('EXT_disjoint_timer_query_webgl2');
        const times = [];
        await new Promise((done) => {
            const step = (time) => {
                times.push(time);
                if (times.length < 121) requestAnimationFrame(step);
                else done();
            };
            requestAnimationFrame(step);
        });
        const refreshHz = (120 * 1000) / (times.at(-1) - times[0]);
        return {
            renderer,
            timers,
            refreshHz: Math.round(refreshHz),
            userAgent: navigator.userAgent,
        };
    });
    await page.close();
    return { ...host, viewport, deviceScaleFactor: scale };
}

try {
    const report = { host: await describeHost(), runs, idleMS, idle: [], pointer: [], flight: [] };
    // Cold flight first, before any room page has warmed the profile's shader cache.
    if (romPath) report.flight.push({ cache: 'cold', ...(await measureFlight()) });
    for (let run = 0; run < runs; run++) {
        report.idle.push(await measureIdle(false));
        report.pointer.push(await measureIdle(true));
        if (romPath) report.flight.push({ cache: 'warm', ...(await measureFlight()) });
        console.log(`run ${run + 1}/${runs} complete`);
    }
    await writeFile(resolve(output, `${label}.json`), JSON.stringify(report, null, 2) + '\n');
    console.log(`Wrote ${resolve(output, `${label}.json`)}`);
} finally {
    await context.close();
    await rm(profile, { recursive: true, force: true });
}
