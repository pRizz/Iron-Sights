"""Tables of where the browser build's processor time went, from a CPU profile.

    (in web/)  MIDAS_BENCH_PROFILE=1 node tools/bench-audit.mjs <rom> <folder> <origin>
    python scripts/port/bench/bench_profile_web.py --profile web=build/web-bench/<folder>/<run>.cpuprofile

The browser runner records a Chromium CPU profile of each run and the number
of frames it covers. This prints milliseconds per frame by function, self and
inclusive, for the page's main thread; with two profiles, also the functions
whose inclusive time differs most. Functions of the game keep their C names
(the runtime is linked with names); the rest are the browser's own.
"""
import argparse
import collections
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]


def frame_label(call_frame):
    """A profile node's name; browser internals come in parentheses."""
    name = call_frame.get('functionName') or '(anonymous)'
    url = call_frame.get('url', '')
    if url.startswith('wasm://') or name.startswith('('):
        return name
    return f"{name} [{url.rsplit('/', 1)[-1] or 'js'}]"


def aggregate(profile):
    """(self, inclusive, total) milliseconds per frame by function label.

    `(idle)` is the event loop waiting and is left out of the total.
    """
    frames = profile.get('midasFrames')
    if not frames:
        raise ValueError('the profile does not say how many frames it covers')
    nodes = {node['id']: node for node in profile['nodes']}
    parents = {child: node['id'] for node in profile['nodes'] for child in node.get('children', [])}
    labels = {identity: frame_label(node['callFrame']) for identity, node in nodes.items()}
    stacks = {}

    def stack_of(identity):
        if identity not in stacks:
            maybe_parent = parents.get(identity)
            above = stack_of(maybe_parent) if maybe_parent is not None else frozenset()
            stacks[identity] = above | {labels[identity]}
        return stacks[identity]

    self_us, inclusive_us = collections.Counter(), collections.Counter()
    for identity, delta in zip(profile['samples'], profile['timeDeltas']):
        self_us[labels[identity]] += delta
        for label in stack_of(identity):
            inclusive_us[label] += delta
    scale = 1/1000/frames
    idle = self_us.get('(idle)', 0)
    total = (sum(self_us.values())-idle)*scale
    return ({name: value*scale for name, value in self_us.items()},
            {name: value*scale for name, value in inclusive_us.items()}, total)


def top(table, count, skip=('(root)', '(idle)')):
    rows = [(name, value) for name, value in table.items() if name not in skip]
    return sorted(rows, key=lambda row: -row[1])[:count]


def render(label, self_ms, inclusive_ms, total, count):
    lines = [f'## {label}: {total:.2f} ms a frame on the main thread', '',
             '| Self ms | Function | | Inclusive ms | Function |', '|---:|---|---|---:|---|']
    for (self_name, self_value), (all_name, all_value) in zip(top(self_ms, count), top(inclusive_ms, count)):
        lines.append(f'| {self_value:.3f} | `{self_name}` | | {all_value:.3f} | `{all_name}` |')
    return lines+['']


def render_difference(base_label, base, other_label, other, count):
    rows = sorted(((other.get(name, 0.0)-base.get(name, 0.0), name) for name in set(base) | set(other)
                   if name not in ('(root)', '(idle)')), key=lambda row: -abs(row[0]))[:count]
    lines = [f'## Largest differences, {other_label} against {base_label} (inclusive ms a frame)', '',
             f'| Difference | {base_label} | {other_label} | Function |', '|---:|---:|---:|---|']
    for difference, name in rows:
        lines.append(f'| {difference:+.3f} | {base.get(name, 0.0):.3f} | {other.get(name, 0.0):.3f} | `{name}` |')
    return lines+['']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--profile', action='append', required=True, metavar='LABEL=FILE',
                        help='a .cpuprofile from the browser runner (repeatable; the first is the base)')
    parser.add_argument('--top', type=int, default=30, help='rows per table')
    parser.add_argument('--out', help='write the Markdown here instead of printing it')
    options = parser.parse_args()
    lines = ['# Browser profile', '', 'Processor milliseconds per frame from samples of the page\'s main '
             'thread, from the moment the player has control to the end of the run. Self is time in the '
             'function itself; inclusive adds everything it called.', '']
    inclusive = {}
    for item in options.profile:
        label, separator, path = item.partition('=')
        if not separator:
            raise SystemExit(f'bench: --profile wants label=file, got {item!r}')
        profile = json.loads((REPO/path).read_text(encoding='utf-8'))
        self_ms, inclusive[label], total = aggregate(profile)
        lines += render(label, self_ms, inclusive[label], total, options.top)
    labels = list(inclusive)
    for other in labels[1:]:
        lines += render_difference(labels[0], inclusive[labels[0]], other, inclusive[other], options.top)
    text = '\n'.join(lines)
    if options.out:
        Path(options.out).write_text(text+'\n', encoding='utf-8', newline='\n')
        print('bench: wrote', options.out)
    else:
        print(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
