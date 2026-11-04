// Markdown before/after table from two room-benchmark.mjs reports (medians across runs).
// Usage: room-benchmark-compare.mjs before.json after.json
import { readFile } from 'node:fs/promises';

const [beforePath, afterPath] = process.argv.slice(2);
if (!beforePath || !afterPath)
    throw new Error('Usage: room-benchmark-compare.mjs before.json after.json');
const [before, after] = await Promise.all(
    [beforePath, afterPath].map(async (path) => JSON.parse(await readFile(path, 'utf8'))),
);

const median = (values) => {
    const sorted = values.filter((value) => Number.isFinite(value)).sort((a, b) => a - b);
    if (!sorted.length) return NaN;
    const middle = Math.floor(sorted.length / 2);
    return sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
};
const scenarios = [
    ['Idle room', (report) => report.idle.map((run) => run.steady)],
    ['Idle room, first 2.5 s', (report) => report.idle.map((run) => run.startup)],
    ['Pointer over props', (report) => report.pointer.map((run) => run.steady)],
    ...['inserting', 'powering', 'flying'].flatMap((phase) => [
        [
            `Warm-cache ${phase}`,
            (report) =>
                report.flight.filter((run) => run.cache === 'warm').map((run) => run.phases[phase]),
        ],
        [
            `Cold-cache ${phase}`,
            (report) =>
                report.flight.filter((run) => run.cache === 'cold').map((run) => run.phases[phase]),
        ],
    ]),
];
const metrics = [
    ['fps', 'FPS'],
    ['intervalP95', 'p95 frame ms'],
    ['intervalMax', 'worst frame ms'],
    ['gpuMean', 'GPU ms/frame'],
];
const format = (value) => (Number.isFinite(value) ? value.toFixed(1) : '-');
const lines = [
    `| Scenario | ${metrics.map(([, title]) => `${title} before → after`).join(' | ')} |`,
    `| --- | ${metrics.map(() => '---').join(' | ')} |`,
];
for (const [name, select] of scenarios) {
    const pair = [before, after].map((report) => select(report).filter(Boolean));
    if (!pair[0].length || !pair[1].length) continue;
    const cells = metrics.map(([key]) =>
        pair.map((runs) => format(median(runs.map((run) => run[key])))).join(' → '),
    );
    lines.push(`| ${name} | ${cells.join(' | ')} |`);
}
console.log(
    `Host: ${after.host.renderer}, ${after.host.viewport.width}x${after.host.viewport.height} @${after.host.deviceScaleFactor}x\n`,
);
console.log(lines.join('\n'));
