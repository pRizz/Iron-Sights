#!/usr/bin/env python3
"""Count places where C code mixes pointers with 32-bit integers, as 64-bit.

Every C file in a `compile_commands.json` is compiled syntax-only with 64-bit
pointers. `count` prints the tally, `update` records it as the baseline,
`check` fails when any file has more diagnostics than the baseline records and
then lists those files' sites, and `sites [FILE...]` prints every diagnostic as
`file:line: kind`. Build the tree first: a file that stops at a fatal error
(such as a header the build generates) was not measured, so every command
refuses to report and names the files.
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
DEFAULT_COMPILE_COMMANDS = ROOT/'build'/'lp64'/'diag'/'compile_commands.json'
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
# Any compiler line reporting an error, located or not (`gcc: error: ...`, `cc1: fatal error: ...`).
ERROR_LINE = re.compile(r'^\S.*?: (?:fatal )?error: ')
FATAL_LINE = re.compile(r'^\S.*?: fatal error: ')


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
        if flag in KINDS:
            kind = flag  # GCC 14 reports int-conversion as an error by default; it is still that kind
        elif match['level'] != 'warning':
            kind = 'error'
        else:
            continue
        path = Path(os.path.normpath(match['file']))
        if path.is_absolute() and path.is_relative_to(root):
            path = path.relative_to(root)
        found.add((path.as_posix(), int(match['line']), int(match['col']), kind))
    return found


def summarize(found: set, compiler: str) -> dict:
    """Return the compiler, total, and the counts by kind, by file and compile errors by file."""
    return dict(
        compiler=compiler,
        total=len(found),
        by_kind=dict(Counter(kind for _, _, _, kind in found)),
        by_file=dict(sorted(Counter(path for path, _, _, _ in found).items())),
        errors_by_file=dict(sorted(Counter(
            path for path, _, _, kind in found if kind == 'error').items())))


def regressions(baseline: dict, current: dict) -> list[str]:
    """Return one message per file whose count rose, or one refusing a compiler mismatch."""
    if baseline['compiler'] != current['compiler']:
        return [f"baseline was recorded with {baseline['compiler']} but this is "
                f"{current['compiler']}; counts from different compilers are not compared"]
    found = [f'{path}: {count} diagnostics, baseline {baseline["by_file"].get(path, 0)}'
             for path, count in sorted(current['by_file'].items())
             if count > baseline['by_file'].get(path, 0)]
    baseline_errors = baseline.get('errors_by_file', {})
    found += [f'{path}: {count} compile errors, baseline {baseline_errors.get(path, 0)}; '
              'an error can hide other diagnostics'
              for path, count in sorted(current.get('errors_by_file', {}).items())
              if count > baseline_errors.get(path, 0)]
    return found


def compiler_id() -> str:
    """Return the compiler as `gcc-<major>`, from `gcc --version`."""
    out = subprocess.run(['gcc', '--version'], capture_output=True, text=True,
                         check=True, timeout=COMPILE_TIMEOUT_SECONDS).stdout
    versions = re.findall(r'\b(\d+)\.\d+(?:\.\d+)?\b', out.splitlines()[0])
    if not versions:
        raise RuntimeError(f'cannot read a version from {out.splitlines()[0]!r}')
    return f'gcc-{versions[-1]}'


def c_entries(entries: list[dict], source: Path) -> list[tuple[dict, list[str]]]:
    """Return each C entry with its syntax-only command; raise when there is none."""
    commands = [(entry, argv) for entry in entries
                if (argv := syntax_command(entry)) is not None]
    if not commands:
        raise RuntimeError(f'{source} holds no C compile commands; refusing to report zero')
    return commands


def site_lines(found: set, files: Optional[list[str]] = None) -> list[str]:
    """Return each diagnostic as `file:line: kind`, by file then line, limited to `files` if given."""
    return [f'{path}:{line}: {kind}' for path, line, _, kind in sorted(found)
            if files is None or path in files]


def risen_files(baseline: dict, current: dict) -> list[str]:
    """Return the files whose total or compile-error count rose above the baseline."""
    errors = baseline.get('errors_by_file', {})
    return sorted({path for path, count in current['by_file'].items() if count > baseline['by_file'].get(path, 0)}
                  | {path for path, count in current.get('errors_by_file', {}).items()
                     if count > errors.get(path, 0)})


def compile_one(entry: dict, argv: list[str], root: Path) -> tuple[set, bool]:
    """Compile one entry; return its diagnostics and whether it stopped at a fatal error.

    A failure to run, or a failing compile whose errors are not all located diagnostics (a driver
    error such as an unknown option), raises RuntimeError: such a file was not measured.
    """
    try:
        done = subprocess.run(argv, cwd=entry['directory'], capture_output=True,
                              text=True, timeout=COMPILE_TIMEOUT_SECONDS)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise RuntimeError(f"cannot compile {entry['file']}: {error}") from error
    found = parse(done.stderr, root)
    lines = done.stderr.splitlines()
    fatal = any(FATAL_LINE.match(text) for text in lines)
    unlocated = [text for text in lines if ERROR_LINE.match(text) and not DIAGNOSTIC.match(text)]
    if done.returncode != 0 and not fatal and (unlocated or not found):
        raise RuntimeError(f"{entry['file']} failed with status {done.returncode} "
                           f"and an unrecognised error:\n{done.stderr}")
    return found, fatal


def measure(compile_commands: Path, root: Path) -> set:
    """Compile every C file in `compile_commands` and return the diagnostics found.

    Raises RuntimeError naming every file that stopped at a fatal error."""
    commands = c_entries(json.loads(compile_commands.read_text()), compile_commands)
    with ThreadPoolExecutor(max_workers=min(MAX_WORKERS, os.cpu_count() or 1)) as pool:
        results = list(pool.map(lambda pair: compile_one(*pair, root), commands))
    stopped = sorted(entry['file'] for (entry, _), (_, fatal) in zip(commands, results) if fatal)
    if stopped:
        raise RuntimeError('stopped at a fatal error, so not measured (build the tree first?): '
                           + ', '.join(stopped))
    return set().union(*(found for found, _ in results))


def render(summary: dict) -> str:
    """Return `summary` as deterministic JSON with a trailing newline."""
    return json.dumps(summary, indent=2, sort_keys=True) + '\n'


def main(argv: list[str]) -> int:
    """Run `count`, `check`, `update` or `sites`; return the process exit status."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('count', 'check', 'update', 'sites'))
    parser.add_argument('files', nargs='*', help='sites only: repository-relative files to list')
    parser.add_argument('--compile-commands', type=Path, default=DEFAULT_COMPILE_COMMANDS)
    args = parser.parse_args(argv)
    if args.files and args.command != 'sites':
        parser.error('only sites takes file names')
    try:
        found = measure(args.compile_commands.resolve(), ROOT)
    except RuntimeError as error:
        print(f'error: {error}', file=sys.stderr)
        return 2
    if args.command == 'sites':
        print('\n'.join(site_lines(found, args.files or None)))
        return 0
    current = summarize(found, compiler_id())
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
            risen = risen_files(baseline, current)
            if risen:
                print('Sites in the files that rose:')
                print('\n'.join(site_lines(found, risen)))
            return 1
        print(f"OK: {current['total']} diagnostics, baseline {baseline['total']}")
        if current['total'] < baseline['total']:
            print('Baseline can be lowered: run update.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
