import { readFile } from 'node:fs/promises';

export function summarizeStartup(trace) {
    const first = trace.frames.find((frame) => frame.gfx > 0);
    const firstAt = first?.at ?? trace.main;
    const buckets = Array.from({ length: 6 }, (_, index) => {
        const from = firstAt + index * 2000;
        const frames = trace.frames.filter((frame) => frame.at >= from && frame.at < from + 2000);
        const calls = trace.calls.filter((call) => call.at >= from && call.at < from + 2000);
        const gaps = frames.slice(1).map((frame, i) => frame.at - frames[i].at);
        return {
            seconds: index * 2,
            gfx: frames.length ? frames.at(-1).gfx - frames[0].gfx : 0,
            worstRAF: Math.max(0, ...gaps),
            glMS: calls.reduce((sum, call) => sum + call.duration, 0),
            roomFrames: frames.filter((frame) => frame.room).length,
        };
    });
    const hitches = trace.logs.filter(({ line }) => /^ge: hitch:/.test(line));
    return {
        mainToFirstFrame: firstAt - trace.main,
        buckets,
        largestGL: [...trace.calls]
            .sort((a, b) => b.duration - a.duration)
            .slice(0, 12)
            .map((call) => ({ ...call, afterMain: call.at - trace.main })),
        hitchCount: hitches.length,
        hitches: hitches.slice(0, 5),
    };
}

if (import.meta.main || process.argv[1]?.endsWith('startup-report.mjs')) {
    for (const path of process.argv.slice(2))
        console.log(
            JSON.stringify(
                { path, ...summarizeStartup(JSON.parse(await readFile(path, 'utf8'))) },
                null,
                2,
            ),
        );
}
