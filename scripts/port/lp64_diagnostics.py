#!/usr/bin/env python3
"""Count places where C code mixes pointers with 32-bit integers, as 64-bit.

Every C file in a `compile_commands.json` is compiled syntax-only with 64-bit
pointers. `count` prints the tally, `update` records it as the baseline, and
`check` fails when any file has more diagnostics than the baseline records.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
from typing import Optional

ROOT = Path(__file__).resolve().parents[2]
BASELINE = Path(__file__).with_name('lp64_diagnostics_baseline.json')
DEFAULT_COMPILE_COMMANDS = Path('build/lp64/diag/compile_commands.json')
KINDS = ('int-conversion', 'pointer-to-int-cast', 'int-to-pointer-cast')
COMPILE_TIMEOUT_SECONDS = 60
MAX_WORKERS = 8

DROPPED_FLAGS = {'-m32', '-msse2', '-mfpmath=sse', '-w', '-c', '-MD', '-MMD'}
DROPPED_WITH_VALUE = {'-o', '-MT', '-MF'}
ADDED_FLAGS = ['-fsyntax-only', '-Wint-conversion', '-Wpointer-to-int-cast',
               '-Wint-to-pointer-cast', '-fmax-errors=0']

DIAGNOSTIC = re.compile(
    r'^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): (?P<level>warning|error|fatal error): '
    r'(?P<text>.*?)(?: \[-W(?P<flag>[\w=-]+)\])?$')


def syntax_command(entry: dict) -> Optional[list[str]]:
    """Return the 64-bit syntax-only command for a C entry, or None for other files."""
    if not entry['file'].endswith('.c'):
        return None
    argv = entry['arguments'] if 'arguments' in entry else shlex.split(entry['command'])
    kept = []
    skip_next = False
    for arg in argv:
        if skip_next:
            skip_next = False
        elif arg in DROPPED_WITH_VALUE:
            skip_next = True
        elif arg in DROPPED_FLAGS or arg.startswith('-fsanitize'):
            continue
        else:
            kept.append(arg)
    return kept + ADDED_FLAGS


def parse(stderr: str, root: Path) -> set[tuple[str, int, int, str]]:
    """Return the distinct (repo-relative file, line, column, kind) diagnostics in compiler output."""
    found = set()
    for text in stderr.splitlines():
        match = DIAGNOSTIC.match(text)
        if not match:
            continue
        flag = match['flag']
        if flag in KINDS and match['level'] == 'warning':
            kind = flag
        elif match['level'] != 'warning' and flag not in KINDS:
            kind = 'error'
        else:
            continue
        path = Path(os.path.normpath(match['file']))
        if path.is_absolute() and path.is_relative_to(root):
            path = path.relative_to(root)
        found.add((path.as_posix(), int(match['line']), int(match['col']), kind))
    return found


def summarize(found: set, compiler: str) -> dict:
    """Return the compiler, total, and the counts by kind and by file."""
    return dict(
        compiler=compiler,
        total=len(found),
        by_kind=dict(Counter(kind for _, _, _, kind in found)),
        by_file=dict(Counter(path for path, _, _, _ in found)))


def regressions(baseline: dict, current: dict) -> list[str]:
    """Return one message per file whose count rose, or one refusing a compiler mismatch."""
    if baseline['compiler'] != current['compiler']:
        return [f"baseline was recorded with {baseline['compiler']} but this is "
                f"{current['compiler']}; counts from different compilers are not compared"]
    return [f'{path}: {count} diagnostics, baseline {baseline["by_file"].get(path, 0)}'
            for path, count in sorted(current['by_file'].items())
            if count > baseline['by_file'].get(path, 0)]


def compiler_id() -> str:
    """Return the compiler as `gcc-<major>`, from `gcc --version`."""
    out = subprocess.run(['gcc', '--version'], capture_output=True, text=True,
                         check=True, timeout=COMPILE_TIMEOUT_SECONDS).stdout
    versions = re.findall(r'\b(\d+)\.\d+(?:\.\d+)?\b', out.splitlines()[0])
    if not versions:
        raise RuntimeError(f'cannot read a version from {out.splitlines()[0]!r}')
    return f'gcc-{versions[-1]}'


def compile_one(entry: dict, root: Path) -> set:
    """Compile one entry and return its diagnostics; a failure to run is an error."""
    argv = syntax_command(entry)
    if argv is None:
        return set()
    try:
        done = subprocess.run(argv, cwd=entry['directory'], capture_output=True,
                              text=True, timeout=COMPILE_TIMEOUT_SECONDS)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise RuntimeError(f"cannot compile {entry['file']}: {error}") from error
    found = parse(done.stderr, root)
    if done.returncode != 0 and not found:
        raise RuntimeError(f"{entry['file']} failed with status {done.returncode} "
                           f"and no recognised diagnostic:\n{done.stderr}")
    return found


def measure(compile_commands: Path, root: Path) -> dict:
    """Compile every C file in `compile_commands` and return the summary."""
    entries = json.loads(compile_commands.read_text())
    with ThreadPoolExecutor(max_workers=min(MAX_WORKERS, os.cpu_count() or 1)) as pool:
        results = list(pool.map(lambda entry: compile_one(entry, root), entries))
    return summarize(set().union(*results), compiler_id())


def render(summary: dict) -> str:
    """Return `summary` as deterministic JSON with a trailing newline."""
    return json.dumps(summary, indent=2, sort_keys=True) + '\n'


def main(argv: list[str]) -> int:
    """Run `count`, `check` or `update`; return the process exit status."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('count', 'check', 'update'))
    parser.add_argument('--compile-commands', type=Path, default=DEFAULT_COMPILE_COMMANDS)
    args = parser.parse_args(argv)
    current = measure(args.compile_commands.resolve(), ROOT)
    if args.command == 'count':
        sys.stdout.write(render(current))
    elif args.command == 'update':
        BASELINE.write_text(render(current))
        print(f"Baseline: {current['total']} diagnostics in {len(current['by_file'])} files")
    else:
        baseline = json.loads(BASELINE.read_text())
        problems = regressions(baseline, current)
        for line in problems:
            print(line)
        if problems:
            return 1
        print(f"OK: {current['total']} diagnostics, baseline {baseline['total']}")
        if current['total'] < baseline['total']:
            print('Baseline can be lowered: run update.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
