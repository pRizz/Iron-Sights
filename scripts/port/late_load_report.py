"""Turn late-load audit runs into findings. Pure: no files, no processes, no clock.

A run is one stage played by the room tour (port/src/port_load_tour.c) with
two logs: GE_LATELOG's late events (port/src/port_late_loads.c) and GE_BENCH's
per-frame log (port/src/port_bench.c). A stage's load ends with its first
frame shown (the log's 'S <stage> <tick>' line); the frames before it are the
load, the rest are play. An event is late when it happens in play. See
scripts/port/late_loads.md.
"""
from typing import NamedTuple

import bench_metrics

FORMAT = '# ge-latelog 1'
KINDS = ('rooms', 'models', 'tex', 'shaders', 'uploads', 'hd')
TOP_EVENTS = 25

Event = NamedTuple('Event', [('stage', int), ('tick', int), ('kind', str), ('us', int), ('what', str)])


def parse_latelog(text):
    """Return ((stage, tick its load ended) in order, late events)."""
    lines = text.splitlines()
    if not lines or not lines[0].startswith(FORMAT):
        raise ValueError('not a ge-latelog 1 file')
    loaded, events = [], []
    for number, line in enumerate(lines[1:], start=2):
        if line.startswith('S '):
            fields = line.split()
            loaded.append((int(fields[1]), int(fields[2]) if len(fields) > 2 else None))
        elif line.startswith('L '):
            fields = line.split(' ', 5)
            if len(fields) < 5:
                raise ValueError(f'line {number}: a late event needs stage, tick, kind and microseconds')
            what = fields[5] if len(fields) > 5 else ''
            events.append(Event(int(fields[1]), int(fields[2]), fields[3], int(fields[4]), what))
    return loaded, events


def stage_frames(frames, stage):
    """The stage's frames from its log, the load frame first."""
    return [frame for frame in frames if frame.stage == stage]


def frame_load_us(frame):
    """Timed load work in one frame, microseconds."""
    return sum(getattr(frame, kind) for kind in bench_metrics.EVENTS)


def split_load(frames, load_tick):
    """(load frames, play frames): play starts at the first frame of load_tick;
    with no tick, the first frame alone is the load."""
    if load_tick is None:
        return frames[:1], frames[1:]
    first = next((index for index, frame in enumerate(frames) if frame.tick >= load_tick), len(frames))
    return frames[:max(first, 1)], frames[max(first, 1):]


def summarize_run(name, stage, frames, events, budget_us, load_tick=None):
    """One stage's findings: its load, its late frames and late events."""
    mine = stage_frames(frames, stage)
    late = [event for event in events if event.stage == stage]
    loading, played = split_load(mine, load_tick)
    by_kind = {kind: [0, 0] for kind in KINDS}
    for event in late:
        counts = by_kind.setdefault(event.kind, [0, 0])
        counts[0] += 1
        counts[1] += event.us
    worst = max(played, key=frame_load_us, default=None)
    over = [frame for frame in played if frame_load_us(frame) >= budget_us]
    return {
        'name': name,
        'stage': stage,
        'frames': len(mine),
        'load_ms': sum(frame.interval for frame in loading)/1000 if loading else None,
        'worst_any_ms': max((frame.interval for frame in played), default=0)/1000,
        'late_count': len(late),
        'late_ms': sum(event.us for event in late if event.kind != 'hd')/1000,
        'by_kind': {kind: {'count': value[0], 'ms': value[1]/1000} for kind, value in by_kind.items()},
        'worst_frame_ms': worst.interval/1000 if worst else 0.0,
        'worst_load_ms': frame_load_us(worst)/1000 if worst else 0.0,
        'worst_tick': worst.tick if worst else None,
        'frames_over_budget': len(over),
        'top': sorted(late, key=lambda event: -event.us)[:TOP_EVENTS],
    }


def late_shaders(runs):
    """Shader ids compiled during play, in Data/shaders.txt's format, sorted."""
    found = set()
    for run in runs:
        for event in run['events']:
            if event.kind == 'shaders':
                # a room-cache twin is prewarmed with its combiner program, by the same ids
                found.add(event.what.strip().removeprefix('retained ').strip())
    return sorted(found)


def check(summaries, budget_ms):
    """Problems that fail --check: frames over the budget and any late shader compile."""
    problems = []
    for summary in summaries:
        if summary['frames'] == 0:
            problems.append(f"{summary['name']}: the run logged no frames of its stage")
            continue
        if summary['frames_over_budget']:
            problems.append(f"{summary['name']}: {summary['frames_over_budget']} frame(s) after the load spent "
                            f"{budget_ms:g} ms or more loading (worst {summary['worst_load_ms']:.1f} ms "
                            f"at tick {summary['worst_tick']})")
        shaders = summary['by_kind'].get('shaders', {'count': 0})
        if shaders['count']:
            problems.append(f"{summary['name']}: {shaders['count']} shader(s) compiled during play")
    return problems


def _kind_cell(summary, kind):
    value = summary['by_kind'].get(kind, {'count': 0, 'ms': 0.0})
    return f"{value['count']} / {value['ms']:.1f}" if value['count'] else '-'


def markdown(summaries, shaders, options):
    """The report: a table per stage, the costliest late events, late shaders."""
    lines = ['# Late-load audit', '',
             f"Settings: {options['settings']}. Room tour {options['dwell']} ticks a room. A frame over budget "
             f"spent {options['budget_ms']:g} ms or more of timed load work after the stage's first frame. "
             'Cells are count / ms.', '',
             '| Stage | Rooms toured | Load ms | Late events | Late ms | Rooms | Models | Tex | Shaders | Uploads | HD reads | '
             'Worst frame loading in play | Worst frame in play | Frames over budget |',
             '|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|']
    for summary in summaries:
        load = f"{summary['load_ms']:.0f}" if summary['load_ms'] is not None else '-'
        worst = (f"{summary['worst_frame_ms']:.1f} ms ({summary['worst_load_ms']:.1f} loading, tick "
                 f"{summary['worst_tick']})" if summary['worst_tick'] is not None else '-')
        cells = ' | '.join(_kind_cell(summary, kind) for kind in KINDS)
        lines.append(f"| {summary['name']} | {summary.get('rooms', '-')} | {load} | {summary['late_count']} | {summary['late_ms']:.1f} | {cells} | "
                     f"{worst} | {summary.get('worst_any_ms', 0.0):.1f} ms | {summary['frames_over_budget']} |")
    events = sorted((event for summary in summaries for event in summary['top']), key=lambda event: -event.us)
    names = {summary['stage']: summary['name'] for summary in summaries}
    lines += ['', '## Costliest late events', '', '| Stage | Tick | Kind | ms | What |', '|---|---:|---|---:|---|']
    for event in events[:TOP_EVENTS]:
        lines.append(f"| {names.get(event.stage, event.stage)} | {event.tick} | {event.kind} | {event.us/1000:.1f} | "
                     f"`{event.what}` |")
    lines += ['', '## Shaders compiled during play', '']
    if shaders:
        lines += ['Add these to `tools/shaders_base.txt` (or rerun `scripts/port/record_shaders.sh`) so the prewarm '
                  'compiles them before play:', '', '```'] + shaders + ['```']
    else:
        lines.append('None.')
    return '\n'.join(lines)+'\n'


def parse_used(text):
    """GE_HD_USED records: {stage: set of keys from other folders} (own=0 lines)."""
    found = {}
    for line in text.splitlines():
        fields = line.split()
        if len(fields) == 4 and fields[0] == 'U' and fields[3] == '0':
            found.setdefault(int(fields[1]), set()).add(fields[2].upper())
    return found


def merge_learned(doc, found):
    """hd_stage_prefetch.json with the stages found replaced, the rest kept, sorted."""
    stages = dict(doc.get('stages', {}))
    for stage, keys in found.items():
        stages[str(stage)] = sorted(keys)
    return {**doc, 'stages': {key: stages[key] for key in sorted(stages, key=int)}}
