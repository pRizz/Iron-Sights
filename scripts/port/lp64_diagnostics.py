#!/usr/bin/env python3
"""Count places where C code mixes pointers with 32-bit integers, as 64-bit.

Every C file in a `compile_commands.json` is compiled syntax-only with 64-bit
pointers. `count` prints the tally, `update` records it as the baseline,
`check` fails when any file has more diagnostics than the baseline records and
then lists those files' sites, and `sites [FILE...]` prints every diagnostic as
`file:line: kind` (`--messages` adds GCC's message and the quoted source line). `--compile-commands`
may be repeated to measure several configurations together. Build the tree first: a file that stops at a fatal error
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
QUOTED_SOURCE = re.compile(r'^\s*\d+ \| ?(?P<text>.*)$')
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
    return set(parse_with_text(stderr, root))


def parse_with_text(stderr: str, root: Path) -> dict[tuple[str, int, int, str], tuple[str, str]]:
    """Return each diagnostic's (file, line, column, kind) key with its (message, quoted source line).

    The source text is the line GCC quotes under the diagnostic, stripped; empty when it quotes none.
    The first report of a place wins."""
    found = {}
    lines = stderr.splitlines()
    for index, text in enumerate(lines):
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
        quoted = QUOTED_SOURCE.match(lines[index + 1]) if index + 1 < len(lines) else None
        key = (path.as_posix(), int(match['line']), int(match['col']), kind)
        found.setdefault(key, (match['text'], quoted['text'].strip() if quoted else ''))
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


def site_lines(found, files: Optional[list[str]] = None, with_messages: bool = False) -> list[str]:
    """Return each diagnostic as `file:line: kind`, by file then line, limited to `files` if given.

    With `with_messages` (`found` is then the dict of `parse_with_text`) each line continues
    `: message | source text`."""
    lines = []
    for key in sorted(found):
        path, line, _, kind = key
        if files is not None and path not in files:
            continue
        text = f'{path}:{line}: {kind}'
        if with_messages:
            message, source = found[key]
            text += f': {message} | {source}'
        lines.append(text)
    return lines


def merge_entries(trees: list[list[dict]]) -> list[dict]:
    """Return the entries of several compile databases, one per source file; the first tree's wins."""
    merged = {}
    for entries in trees:
        for entry in entries:
            merged.setdefault(os.path.normpath(os.path.join(entry.get('directory', ''), entry['file'])), entry)
    return list(merged.values())


def rise_refusal(recorded_total: int, new_total: int) -> Optional[str]:
    """Return why recording `new_total` over `recorded_total` is refused, or None when it may be written."""
    if new_total <= recorded_total:
        return None
    return (f'the total would rise from {recorded_total} to {new_total}; the baseline only falls. '
            'Pass --allow-rise REASON to record a rise deliberately')


def risen_files(baseline: dict, current: dict) -> list[str]:
    """Return the files whose total or compile-error count rose above the baseline."""
    errors = baseline.get('errors_by_file', {})
    return sorted({path for path, count in current['by_file'].items() if count > baseline['by_file'].get(path, 0)}
                  | {path for path, count in current.get('errors_by_file', {}).items()
                     if count > errors.get(path, 0)})


def compile_one(entry: dict, argv: list[str], root: Path) -> tuple[dict, bool]:
    """Compile one entry; return its diagnostics and whether it stopped at a fatal error.

    A failure to run, or a failing compile whose errors are not all located diagnostics (a driver
    error such as an unknown option), raises RuntimeError: such a file was not measured.
    """
    try:
        done = subprocess.run(argv, cwd=entry['directory'], capture_output=True,
                              text=True, timeout=COMPILE_TIMEOUT_SECONDS)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise RuntimeError(f"cannot compile {entry['file']}: {error}") from error
    found = parse_with_text(done.stderr, root)
    lines = done.stderr.splitlines()
    fatal = any(FATAL_LINE.match(text) for text in lines)
    unlocated = [text for text in lines if ERROR_LINE.match(text) and not DIAGNOSTIC.match(text)]
    if done.returncode != 0 and not fatal and (unlocated or not found):
        raise RuntimeError(f"{entry['file']} failed with status {done.returncode} "
                           f"and an unrecognised error:\n{done.stderr}")
    return found, fatal


def measure(compile_commands: list[Path], root: Path) -> dict:
    """Compile every C file in the compile databases once and return the diagnostics found, with text.

    A source file several databases list is compiled with the first database's command.
    Raises RuntimeError naming every file that stopped at a fatal error."""
    trees = [json.loads(path.read_text()) for path in compile_commands]
    for path, entries in zip(compile_commands, trees):
        c_entries(entries, path)
    commands = c_entries(merge_entries(trees), compile_commands[0])
    with ThreadPoolExecutor(max_workers=min(MAX_WORKERS, os.cpu_count() or 1)) as pool:
        results = list(pool.map(lambda pair: compile_one(*pair, root), commands))
    stopped = sorted(entry['file'] for (entry, _), (_, fatal) in zip(commands, results) if fatal)
    if stopped:
        raise RuntimeError('stopped at a fatal error, so not measured (build the tree first?): '
                           + ', '.join(stopped))
    merged = {}
    for found, _ in results:
        for key, text in found.items():
            merged.setdefault(key, text)
    return merged


def render(summary: dict) -> str:
    """Return `summary` as deterministic JSON with a trailing newline."""
    return json.dumps(summary, indent=2, sort_keys=True) + '\n'


def main(argv: list[str]) -> int:
    """Run `count`, `check`, `update` or `sites`; return the process exit status."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('count', 'check', 'update', 'sites'))
    parser.add_argument('files', nargs='*', help='sites only: repository-relative files to list')
    parser.add_argument('--compile-commands', type=Path, action='append',
                        help='a compile_commands.json; repeat to measure several configurations together')
    parser.add_argument('--messages', action='store_true',
                        help='sites only: add GCC\'s message and the quoted source line')
    parser.add_argument('--allow-rise', metavar='REASON',
                        help='update only: record a total higher than the recorded one, for this reason')
    args = parser.parse_args(argv)
    if args.files and args.command != 'sites':
        parser.error('only sites takes file names')
    if args.messages and args.command != 'sites':
        parser.error('only sites takes --messages')
    if args.allow_rise is not None and args.command != 'update':
        parser.error('only update takes --allow-rise')
    try:
        details = measure([path.resolve() for path in args.compile_commands or [DEFAULT_COMPILE_COMMANDS]], ROOT)
    except RuntimeError as error:
        print(f'error: {error}', file=sys.stderr)
        return 2
    found = set(details)
    if args.command == 'sites':
        print('\n'.join(site_lines(details if args.messages else found, args.files or None, args.messages)))
        return 0
    current = summarize(found, compiler_id())
    if args.command == 'count':
        sys.stdout.write(render(current))
    elif args.command == 'update':
        refusal = None if args.allow_rise is not None or not BASELINE.exists() else rise_refusal(
            json.loads(BASELINE.read_text())['total'], current['total'])
        if refusal:
            print(f'error: {refusal}', file=sys.stderr)
            return 2
        if args.allow_rise is not None:
            print(f'Rise allowed: {args.allow_rise}')
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
