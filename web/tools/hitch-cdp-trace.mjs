// Browser-wide, private Chrome trace capture for the hitch audit.
import { createReadStream, createWriteStream } from 'node:fs';
import { writeFile } from 'node:fs/promises';
import { createGunzip, createGzip } from 'node:zlib';
import { pipeline } from 'node:stream/promises';

const categories = [
    'devtools.timeline', 'disabled-by-default-devtools.timeline',
    'disabled-by-default-v8.cpu_profiler', 'toplevel', 'blink.user_timing',
    'cc', 'viz', 'gpu',
];

export async function startBrowserTrace(browser, path, bufferKb = 65536) {
    const session = await browser.newBrowserCDPSession();
    let dataLoss = false;
    session.on('Tracing.dataLossOccurred', () => { dataLoss = true; });
    await session.send('Tracing.start', {
        transferMode: 'ReturnAsStream',
        traceConfig: {
            recordMode: 'recordContinuously',
            traceBufferSizeInKb: bufferKb,
            includedCategories: categories,
        },
    });
    let stopped = false;
    return {
        async stop() {
            if (stopped) throw new Error('Chrome trace was already stopped');
            stopped = true;
            const complete = new Promise((resolve, reject) => {
                const timeout = setTimeout(() => reject(new Error('Chrome tracingComplete timeout')), 120_000);
                session.once('Tracing.tracingComplete', (event) => {
                    clearTimeout(timeout);
                    resolve(event);
                });
            });
            await session.send('Tracing.end');
            const { stream, dataLossOccurred } = await complete;
            if (!stream) throw new Error('Chrome trace stream was not returned');
            const gzip = createGzip();
            const writing = pipeline(gzip, createWriteStream(path));
            try {
                for (;;) {
                    const chunk = await session.send('IO.read', { handle: stream, size: 1 << 20 });
                    const bytes = chunk.base64Encoded
                        ? Buffer.from(chunk.data, 'base64') : Buffer.from(chunk.data);
                    if (bytes.length && !gzip.write(bytes)) await new Promise((resolve) => gzip.once('drain', resolve));
                    if (chunk.eof) break;
                }
                gzip.end();
                await writing;
            } finally {
                await session.send('IO.close', { handle: stream }).catch(() => {});
                await session.detach().catch(() => {});
            }
            return { path, dataLoss: dataLoss || !!dataLossOccurred, bufferKb };
        },
    };
}

export async function readBrowserTrace(path) {
    const chunks = [];
    for await (const chunk of createReadStream(path).pipe(createGunzip())) chunks.push(chunk);
    return JSON.parse(Buffer.concat(chunks).toString('utf8'));
}

export function analyzeBrowserTrace(trace, phase, gap) {
    const events = trace.traceEvents || [];
    const names = new Map();
    const threads = new Map();
    for (const event of events) {
        if (event.ph === 'M' && event.name === 'thread_name')
            threads.set(`${event.pid}:${event.tid}`, event.args?.name || 'unknown');
        if (event.ph === 'M' && event.name === 'process_name')
            names.set(event.pid, event.args?.name || 'unknown');
    }
    const anchor = events.find((event) => event.name === `midas-hitch:${phase}:start`);
    const relevantTracks = [...new Set(events.flatMap((event) =>
        ['cc', 'viz', 'gpu', 'devtools.timeline', 'disabled-by-default-v8.cpu_profiler']
            .filter((category) => event.cat?.split(',').includes(category))))];
    const result = {
        events: events.length,
        tracks: relevantTracks,
        processes: [...new Set([...names.values()])],
        anchorFound: !!anchor,
        gap: gap || null,
        topEvents: [],
        threadActivity: [],
        cpuSamples: 0,
        cpuTopFunctions: [],
    };
    if (!anchor || !gap) return result;
    const startUs = anchor.ts + (gap.at - gap.gap - gap.anchorAt) * 1000;
    const endUs = anchor.ts + (gap.at - gap.anchorAt) * 1000;
    const overlapping = events.filter((event) => event.ph === 'X' && event.dur &&
        event.ts <= endUs && event.ts + event.dur >= startUs);
    const activity = new Map();
    for (const event of overlapping) {
        const process = names.get(event.pid) || String(event.pid);
        const thread = threads.get(`${event.pid}:${event.tid}`) || String(event.tid);
        const key = `${process}/${thread}`;
        const value = activity.get(key) || { process, thread, events: 0, longestMs: 0 };
        value.events++;
        value.longestMs = Math.max(value.longestMs, event.dur / 1000);
        activity.set(key, value);
    }
    result.threadActivity = [...activity.values()].sort((a, b) => b.events - a.events);
    const profileChunks = events.filter((event) => event.name === 'ProfileChunk' &&
        event.pid === anchor.pid);
    const functions = new Map();
    for (const event of profileChunks)
        for (const node of event.args?.data?.cpuProfile?.nodes || [])
            functions.set(node.id, node.callFrame);
    const samples = new Map();
    for (const event of profileChunks) {
        if (event.ts < startUs || event.ts > endUs) continue;
        for (const id of event.args?.data?.cpuProfile?.samples || [])
            samples.set(id, (samples.get(id) || 0) + 1);
    }
    result.cpuSamples = [...samples.values()].reduce((sum, count) => sum + count, 0);
    result.cpuTopFunctions = [...samples].sort((a, b) => b[1] - a[1]).slice(0, 10)
        .map(([id, count]) => ({ function: functions.get(id)?.functionName || '(unknown)', samples: count }));
    result.topEvents = overlapping
        .sort((a, b) => b.dur - a.dur).slice(0, 25)
        .map((event) => ({
            name: event.name, durationMs: event.dur / 1000,
            process: names.get(event.pid) || String(event.pid),
            thread: threads.get(`${event.pid}:${event.tid}`) || String(event.tid),
            category: event.cat,
        }));
    return result;
}

export async function reportBrowserTrace(path, phase, gap, metadata) {
    const trace = await readBrowserTrace(path);
    const report = { ...metadata, analysis: analyzeBrowserTrace(trace, phase, gap) };
    await writeFile(path.replace(/\.json\.gz$/, '.report.json'), JSON.stringify(report, null, 2));
    return report;
}
