"""Turn a GE_BENCH frame log into benchmark numbers. Pure: no files, no clock.

A log is the CSV port/src/port_bench.c writes: one row per presented frame,
times in microseconds. The runner (bench.py) picks the measured window by game
tick, so two runs of one scenario time the same frames however fast they ran.
"""
import math
import statistics
from typing import NamedTuple

FORMAT = 'ge-bench 1'
COLUMNS = ('interval', 'run', 'swap', 'lim', 'sim', 'audio', 'pacer', 'draws', 'stage', 'tick',
           'rooms', 'models', 'tex', 'shaders', 'uploads')
EVENTS = COLUMNS[10:]
TITLE_STAGE = 90
# Frame-time budgets for 120, 60 and 30 frames a second, in microseconds.
BUDGETS_US = (('over_8ms', 8334), ('over_16ms', 16667), ('over_33ms', 33334))

Frame = NamedTuple('Frame', [(name, int) for name in COLUMNS])


def parse_log(text):
    """Return (header, frames). header is the '# ge-bench' line's key=value pairs."""
    lines = text.splitlines()
    if len(lines) < 2 or not lines[0].startswith('# '+FORMAT):
        raise ValueError('not a ge-bench 1 frame log')
    if tuple(lines[1].split(',')) != COLUMNS:
        raise ValueError('unexpected frame log columns: '+lines[1])
    header = dict(item.split('=', 1) for item in lines[0].split()[3:] if '=' in item)
    frames = []
    for number, line in enumerate(lines[2:], start=3):
        fields = line.split(',')
        if len(fields) != len(COLUMNS):
            raise ValueError(f'line {number}: expected {len(COLUMNS)} fields, found {len(fields)}')
        frames.append(Frame(*map(int, fields)))
    return header, frames


def stage_frames(frames):
    """Frames of the mission the run loaded: everything after the title stage."""
    return [frame for frame in frames if frame.stage != TITLE_STAGE]


def select_window(frames, first_tick, last_tick):
    """Mission frames whose game tick is in [first_tick, last_tick)."""
    return [frame for frame in stage_frames(frames) if first_tick <= frame.tick < last_tick]


def percentile(ordered, fraction):
    """Nearest-rank percentile of an ascending list."""
    if not ordered:
        raise ValueError('percentile of no values')
    rank = max(1, math.ceil(fraction*len(ordered)))
    return ordered[rank-1]


def low_fps(ordered_us, fraction):
    """Frames a second over the slowest `fraction` of frames (the '1% low')."""
    count = max(1, math.ceil(fraction*len(ordered_us)))
    return 1e6/statistics.fmean(ordered_us[-count:])


def game_us(frame, render_thread):
    """Game code in a frame: the tick and the display-list build.

    With the render thread off (always, in the browser) the renderer runs
    inside the game's stretch, so its time comes off.
    """
    if render_thread:
        return frame.sim
    return max(0, frame.sim-frame.run)


def frame_metrics(window, render_thread=True):
    """Numbers for one measured window of frames."""
    if not window:
        raise ValueError('the measured window holds no frames')
    intervals = sorted(frame.interval for frame in window)
    total_us = sum(intervals)

    def mean_ms(value):
        return statistics.fmean(value(frame) for frame in window)/1000

    metrics = {
        'frames': len(window),
        'seconds': total_us/1e6,
        'avg_fps': len(window)*1e6/total_us,
        'low_1pct_fps': low_fps(intervals, 0.01),
        'low_01pct_fps': low_fps(intervals, 0.001),
        'frame_ms_p50': percentile(intervals, 0.50)/1000,
        'frame_ms_p95': percentile(intervals, 0.95)/1000,
        'frame_ms_p99': percentile(intervals, 0.99)/1000,
        'frame_ms_max': intervals[-1]/1000,
        'render_ms': mean_ms(lambda frame: max(0, frame.run-frame.swap-frame.lim)),
        'swap_ms': mean_ms(lambda frame: frame.swap),
        'sim_ms': mean_ms(lambda frame: game_us(frame, render_thread)),
        'audio_ms': mean_ms(lambda frame: frame.audio),
        'pacer_ms': mean_ms(lambda frame: frame.pacer),
        'draws': statistics.fmean(frame.draws for frame in window),
        'events_ms': sum(getattr(frame, name) for frame in window for name in EVENTS)/1000,
    }
    # Processor time a frame costs, wherever it ran: what the browser's own
    # frame scheduling and a hidden window's throttling cannot distort.
    metrics['work_ms'] = metrics['render_ms']+metrics['sim_ms']
    for name, budget in BUDGETS_US:
        metrics[name] = sum(1 for interval in intervals if interval > budget)
    return metrics


def load_metrics(frames, first_tick):
    """Stage load cost: the mission frames before the measured window."""
    before = [frame for frame in stage_frames(frames) if frame.tick < first_tick]
    if not before:
        return {'load_ms': 0.0, 'load_events_ms': {name: 0.0 for name in EVENTS}}
    return {
        # The load runs inside one long frame, the first of the stage.
        'load_ms': max(frame.interval for frame in before)/1000,
        'load_events_ms': {name: sum(getattr(frame, name) for frame in before)/1000 for name in EVENTS},
    }


def run_metrics(text, first_tick, last_tick):
    """All numbers for one run's frame log."""
    header, frames = parse_log(text)
    window = select_window(frames, first_tick, last_tick)
    metrics = frame_metrics(window, render_thread=header.get('render_thread') != '0')
    metrics.update(load_metrics(frames, first_tick))
    metrics['ticks'] = window[-1].tick-window[0].tick+1
    metrics['header'] = header
    return metrics


def summarize(runs):
    """Median, fastest and slowest of every numeric metric across repeats."""
    summary = {}
    for name, value in runs[0].items():
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            continue
        values = [run[name] for run in runs]
        summary[name] = {'median': statistics.median(values), 'min': min(values), 'max': max(values)}
    summary['runs'] = len(runs)
    return summary


def change(base, other):
    """Relative change of `other` against `base`, or None when base is zero."""
    if base == 0:
        return None
    return (other-base)/base
