"""Render benchmark results as Markdown. Pure: results in, text out."""
from bench_metrics import change

# Repeats of one build and scenario draw the same frames; the render thread reads
# the game tick a frame early or late at the window's edges, which moves the mean
# draw count by far less than this.
WORKLOAD_TOLERANCE = 0.005

# (results key, column title, digits, higher is better)
TABLE = (
    ('avg_fps', 'Average fps', 0, True),
    ('low_1pct_fps', '1% low fps', 0, True),
    ('frame_ms_p50', 'Median frame ms', 2, False),
    ('frame_ms_p99', '99th pct ms', 2, False),
    ('render_ms', 'Render ms', 2, False),
    ('sim_ms', 'Game ms', 2, False),
    ('draws', 'Draws', 0, None),
    ('load_ms', 'Load ms', 0, False),
)


def cell(summary, key, digits):
    value = summary[key]
    return f"{value['median']:.{digits}f}"


def spread(summary, key):
    """Slowest-to-fastest range of a metric, as a share of its median."""
    value = summary[key]
    if value['median'] == 0:
        return 0.0
    return (value['max']-value['min'])/value['median']


def versus(base, other, key, higher_is_better):
    """'+12%' style change against the base build; bold when it beats the noise."""
    maybe_ratio = change(base[key]['median'], other[key]['median'])
    if maybe_ratio is None or higher_is_better is None:
        return ''
    text = f'{maybe_ratio:+.0%}'
    if min(base['runs'], other['runs']) < 2:
        return f' ({text})'  # one run has no range to judge the change against
    noise = max(spread(base, key), spread(other, key))
    return f' (**{text}**)' if abs(maybe_ratio) > noise else f' ({text})'


def scenario_table(scenario, by_build, builds, base_label):
    lines = [f'### {scenario}', '',
             '| Build | '+' | '.join(title for _, title, _, _ in TABLE)+' | Runs |',
             '|---|'+'---:|'*(len(TABLE)+1)]
    maybe_base = by_build.get(base_label)
    for label in builds:
        if label not in by_build:
            continue
        summary = by_build[label]
        cells = []
        for key, _, digits, higher_is_better in TABLE:
            text = cell(summary, key, digits)
            if maybe_base is not None and label != base_label:
                text += versus(maybe_base, summary, key, higher_is_better)
            cells.append(text)
        lines.append(f'| {label} | '+' | '.join(cells)+f" | {summary['runs']} |")
    spreads = ', '.join(f"{label} {spread(by_build[label], 'avg_fps'):.0%}"
                        for label in builds if label in by_build)
    lines += ['', f'Run-to-run range of average fps: {spreads}.']
    uneven = [label for label in builds if label in by_build
              and spread(by_build[label], 'draws') > WORKLOAD_TOLERANCE]
    if uneven:
        lines.append(f"The repeats of {', '.join(uneven)} did not draw the same amount, so part of that "
                     'range is a difference in work.')
    lines.append('')
    return lines


def disturbance_lines(runs):
    """What else the machine was doing while the runs were timed."""
    lines = []
    overlapped = sum(1 for run in runs if run.get('disturbed'))
    if overlapped:
        lines.append(f'- **{overlapped} of {len(runs)} runs overlapped another game process** even after '
                     'repeats; their timings are suspect')
    busy = sorted(run['cpu_busy'] for run in runs if run.get('cpu_busy') is not None)
    if busy:
        lines.append(f'- Processor use across the whole machine during runs: median {busy[len(busy)//2]:.0%}, '
                     f'highest {busy[-1]:.0%} (the game alone keeps two to three cores busy)')
    return lines


def window_state(results):
    if results.get('target') == 'browser':
        return 'headed browser' if results['visible'] else 'headless browser'
    return 'visible' if results['visible'] else 'hidden'


def render(results, base_label):
    """Markdown report: one table per preset and scenario, medians of the repeats."""
    host = results['host']
    builds = [build['label'] for build in results['builds']]
    lines = ['# Benchmark results', '',
             f"- Recorded: {results['created']}",
             f"- Machine: {host.get('cpu', 'unknown')}; {host.get('gpu', 'unknown')}; {host.get('os', 'unknown')}",
             f"- Window: {results['resolution']}, {window_state(results)}",
             f"- Mode: fixed tick, unpaced; each run draws the same game ticks, one frame per tick",
             f"- Compared against: {base_label}"]
    lines += disturbance_lines(results.get('runs', []))
    lines += [f'- {note}' for note in results.get('notes', [])]+['']
    for build in results['builds']:
        digest = f" (sha256 {build['sha256'][:12]})" if build['sha256'] else ''
        lines.append(f"- `{build['label']}`: {build['identity']}{digest}")
    lines += ['', 'Each cell is the median of the repeats. A change in bold is larger than the',
              "runs' own slowest-to-fastest range; one in plain text is within the noise.", '']
    for preset, scenarios in results['summary'].items():
        lines += [f'## Settings preset: {preset}', '']
        for scenario, by_build in scenarios.items():
            lines += scenario_table(scenario, by_build, builds, base_label)
    return '\n'.join(lines)
