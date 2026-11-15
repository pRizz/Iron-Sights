"""Turn GE_PROFILE sample files into tables of where the processor time went.

    python scripts/port/bench/bench.py run --keep --repeats 1 --env GE_PROFILE=profile.txt ...
    python scripts/port/bench/bench_profile.py --run fork=build_win/bench-run-<time>/default-dam-walk-fork-1 \\
        --run stock=build_win/bench-run-<time>/default-dam-walk-stock-1 --exe fork=build_win/IronSights.exe \\
        --exe stock=build_win/bench-stock/build/ProjectMidas.exe

Each --run is a kept run folder (profile.txt and frames.csv). The game samples
itself (port/src/port_profile.c); this script names the functions with the
project container's `nm`, keeps the samples of the measured tick window, and
prints processor milliseconds per frame by function: self time and inclusive
time per thread, and with two runs, the functions whose inclusive time differs
most. See scripts/port/benchmarks.md.
"""
import argparse
import bisect
import collections
import ntpath
import subprocess
import sys
from pathlib import Path

import bench_metrics

REPO = Path(__file__).resolve().parents[3]
CONTAINER_ROOT = '/home/dev/project'
FORMAT = '# ge-profile 1'
TITLE_STAGE = bench_metrics.TITLE_STAGE
THREAD_FLOOR = 0.02  # threads below this share of a processor are left out
# An unpaced run's game thread spins in the pump while the renderer catches up,
# polling input and events tens of thousands of times a second. That is the
# benchmark waiting, not the cost of a frame, so it is totalled and set aside.
PUMP = '_portPumpEvents'

Sample = collections.namedtuple('Sample', 'thread stage tick wake micros addresses')


def parse_profile(text):
    """Return (modules, samples); modules are (base, size, path)."""
    lines = text.splitlines()
    if not lines or not lines[0].startswith(FORMAT):
        raise ValueError('not a ge-profile 1 sample file')
    modules, samples = [], []
    for line in lines[1:]:
        kind, _, rest = line.partition(' ')
        if kind == 'M':
            base, size, path = rest.split(' ', 2)
            modules.append((int(base, 16), int(size, 16), path))
        elif kind == 'S':
            fields = rest.split()
            samples.append(Sample(int(fields[0]), int(fields[1]), int(fields[2]), int(fields[3]),
                                  int(fields[4]), tuple(int(word, 16) for word in fields[5:])))
    return modules, samples


def parse_symbols(nm_text):
    """Sorted (address, name) of the functions in `nm -n --defined-only` output."""
    symbols = []
    for line in nm_text.splitlines():
        fields = line.split(None, 2)
        if len(fields) == 3 and fields[1] in 'tT':
            symbols.append((int(fields[0], 16), fields[2]))
    symbols.sort()
    return symbols


class Namer:
    """Names an address: a function of the game, or the module it lies in."""

    def __init__(self, modules, symbols, executable_name):
        self.modules = sorted(modules)
        self.starts = [address for address, _ in symbols]
        self.names = [name for _, name in symbols]
        self.executable = executable_name.lower()

    def module_of(self, address):
        for base, size, path in self.modules:
            if base <= address < base+size:
                return ntpath.basename(path)
        return None

    def name(self, address):
        maybe_module = self.module_of(address)
        if maybe_module is None:
            return None
        if maybe_module.lower() != self.executable:
            return f'[{maybe_module}]'
        index = bisect.bisect_right(self.starts, address)-1
        return self.names[index] if index >= 0 else f'[{maybe_module}]'


def named_stack(sample, namer):
    """The sample's frames, innermost first, dropping addresses in no module
    (a walk that ran off a frame-pointer-less function) and repeats."""
    stack = []
    for address in sample.addresses:
        maybe_name = namer.name(address)
        if maybe_name is not None and (not stack or stack[-1] != maybe_name):
            stack.append(maybe_name)
    return stack or ['[unknown]']


def window_samples(samples, first_tick, last_tick):
    return [sample for sample in samples
            if sample.stage != TITLE_STAGE and first_tick <= sample.tick < last_tick]


def thread_name(stack):
    """A thread is named by its outermost function of the game."""
    for name in reversed(stack):
        if not name.startswith('['):
            return name
    return stack[-1]


def aggregate(samples, namer, first_tick, last_tick):
    """Per-thread processor time in the tick window, in milliseconds per frame.

    A thread's samples over the sampler's wake-ups is its share of a processor;
    that share of the window's wall time, over the frames, is its cost a frame.
    """
    window = window_samples(samples, first_tick, last_tick)
    if not window:
        raise ValueError('no samples in the measured window')
    wakes = max(sample.wake for sample in window)-min(sample.wake for sample in window)+1
    wall_ms = (max(sample.micros for sample in window)-min(sample.micros for sample in window))/1000
    frames = last_tick-first_tick
    ms_per_sample = wall_ms/wakes/frames
    threads = {}
    for sample in window:
        thread = threads.setdefault(sample.thread, {'samples': 0, 'pump': 0, 'self': collections.Counter(),
                                                    'inclusive': collections.Counter(),
                                                    'roots': collections.Counter(),
                                                    'stacks': collections.Counter()})
        stack = named_stack(sample, namer)
        thread['samples'] += 1
        thread['roots'][thread_name(stack)] += 1
        if PUMP in stack:
            thread['pump'] += 1
            continue
        thread['self'][stack[0]] += 1
        thread['inclusive'].update(set(stack))
        thread['stacks'][';'.join(reversed(stack))] += 1
    result = []
    for thread in threads.values():
        share = thread['samples']/wakes
        if share < THREAD_FLOOR:
            continue
        result.append({
            'root': thread['roots'].most_common(1)[0][0], 'share': share,
            'ms': (thread['samples']-thread['pump'])*ms_per_sample, 'pump_ms': thread['pump']*ms_per_sample,
            'self': {name: count*ms_per_sample for name, count in thread['self'].items()},
            'inclusive': {name: count*ms_per_sample for name, count in thread['inclusive'].items()},
            'stacks': thread['stacks'],
        })
    result.sort(key=lambda thread: -thread['ms'])
    return result


def top(table, count):
    return sorted(table.items(), key=lambda item: -item[1])[:count]


def render_run(label, threads, count):
    lines = [f'## {label}', '']
    for thread in threads:
        waiting = f", plus {thread['pump_ms']:.2f} ms waiting in the pump" if thread['pump_ms'] else ''
        lines += [f"### Thread under `{thread['root']}`: {thread['ms']:.2f} ms a frame{waiting}", '',
                  '| Self ms | Function | | Inclusive ms | Function |', '|---:|---|---|---:|---|']
        for (self_name, self_ms), (all_name, all_ms) in zip(top(thread['self'], count),
                                                            top(thread['inclusive'], count)):
            lines.append(f'| {self_ms:.3f} | `{self_name}` | | {all_ms:.3f} | `{all_name}` |')
        lines.append('')
    return lines


def inclusive_totals(threads):
    totals = collections.Counter()
    for thread in threads:
        totals.update(thread['inclusive'])
    return totals


def render_difference(base_label, base, other_label, other, count):
    """Functions whose inclusive time differs most between two runs."""
    names = set(base) | set(other)
    rows = sorted(((other.get(name, 0.0)-base.get(name, 0.0), name) for name in names),
                  key=lambda row: -abs(row[0]))[:count]
    lines = [f'## Largest differences, {other_label} against {base_label} (inclusive ms a frame, all threads)', '',
             f'| Difference | {base_label} | {other_label} | Function |', '|---:|---:|---:|---|']
    for difference, name in rows:
        lines.append(f'| {difference:+.3f} | {base.get(name, 0.0):.3f} | {other.get(name, 0.0):.3f} | `{name}` |')
    return lines+['']


def read_symbols(executable):
    """Function symbols of a game executable, from the project container's nm."""
    relative = executable.resolve().relative_to(REPO).as_posix()
    mount = f'{REPO.as_posix()}:{CONTAINER_ROOT}'
    listing = subprocess.run(['docker', 'run', '--rm', '-v', mount, '-w', CONTAINER_ROOT, 'goldeneye',
                              'i686-w64-mingw32-nm', '-n', '--defined-only', relative],
                             check=True, capture_output=True, text=True).stdout
    return parse_symbols(listing)


def labelled(items, what):
    pairs = {}
    for item in items or []:
        label, separator, path = item.partition('=')
        if not separator:
            raise SystemExit(f'bench: {what} wants label=path, got {item!r}')
        pairs[label] = (REPO/path).resolve()
    return pairs


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--run', action='append', required=True, metavar='LABEL=FOLDER',
                        help='a kept run folder holding profile.txt (repeatable; the first is the base)')
    parser.add_argument('--exe', action='append', required=True, metavar='LABEL=PATH',
                        help='the executable each run used, for its symbols')
    parser.add_argument('--first-tick', type=int, default=300)
    parser.add_argument('--last-tick', type=int, default=3900)
    parser.add_argument('--top', type=int, default=25, help='rows per table')
    parser.add_argument('--out', help='write the Markdown here instead of printing it')
    parser.add_argument('--folded', help='also write folded stacks (flame graph input) with this prefix')
    options = parser.parse_args()
    runs, executables = labelled(options.run, '--run'), labelled(options.exe, '--exe')
    lines, totals = ['# Profile', '', f'Ticks {options.first_tick} to {options.last_tick}; processor '
                     'milliseconds per frame from samples. Self is time in the function itself; inclusive '
                     'adds everything it called. Names in brackets are modules without symbols. Time the game thread '
                     'spends in the pump is set aside: unpaced, it spins there waiting for the renderer.', ''], {}
    for label, folder in runs.items():
        if label not in executables:
            raise SystemExit(f'bench: no --exe for {label}')
        modules, samples = parse_profile((folder/'profile.txt').read_text(encoding='utf-8', errors='replace'))
        namer = Namer(modules, read_symbols(executables[label]), executables[label].name)
        threads = aggregate(samples, namer, options.first_tick, options.last_tick)
        totals[label] = inclusive_totals(threads)
        lines += render_run(label, threads, options.top)
        if options.folded:
            folded = collections.Counter()
            for thread in threads:
                folded.update(thread['stacks'])
            Path(f'{options.folded}-{label}.txt').write_text(
                ''.join(f'{stack} {count}\n' for stack, count in folded.most_common()), encoding='utf-8')
    labels = list(runs)
    for other in labels[1:]:
        lines += render_difference(labels[0], totals[labels[0]], other, totals[other], options.top*2)
    text = '\n'.join(lines)
    if options.out:
        Path(options.out).write_text(text+'\n', encoding='utf-8', newline='\n')
        print('bench: wrote', options.out)
    else:
        print(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
