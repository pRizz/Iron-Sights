#!/usr/bin/env python3
"""Compile 64-bit build trees as a gate, and list where C mixes pointers with 32-bit integers.

`gate BUILD_DIR...` takes real `GE_POINTER_BITS=64` configurations (CMake run with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, then `--target ge_buildinfo` built so the generated header
exists) and compiles every C and C++ entry of each directory's `compile_commands.json` syntax-only
with the entry's own flags, in parallel. At 64 bits those flags make the three int/pointer
conversions errors (`CMakeLists.txt`). Before compiling anything it exits 2 naming every entry whose
flags do not: a C entry lacking any of the three `-Werror=` flags, a C++ entry lacking
`-Werror=int-to-pointer-cast` (the one conversion C++ only warns about), or either carrying `-w`.
It exits 1 listing every file that fails, with its first error, 2 when a directory is not such a
configuration, and 0 otherwise. A source file two
directories list is compiled once in each, with that directory's command.

`sites [--messages] [--build-dir DIR]... [FILE...]` prints every int/pointer diagnostic of the C
files in the same directories as `file:line: kind` (`--messages` adds GCC's message and the quoted
source line). A source file that contains `#line` directives has compiler line numbers that are
not its own lines, so `sites` reports its diagnostics at the PHYSICAL line, found by compiling a
copy of the file without the directives (kept outside the source tree, with the original's folder
searched first for quoted includes). A file that stops at a fatal error was not measured, so
`sites` refuses to report and names the files.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
from typing import Optional

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BUILD_DIRS = (ROOT/'build'/'lp64'/'gate', ROOT/'build'/'lp64'/'gate-video')
GENERATED_HEADER = Path('generated')/'port_buildinfo.h'
KINDS = ('int-conversion', 'pointer-to-int-cast', 'int-to-pointer-cast')
COMPILE_TIMEOUT_SECONDS = 60
# Syntax-only C++ of the renderer takes minutes under x86_64 emulation on the Mac.
GATE_TIMEOUT_SECONDS = 600
MAX_WORKERS = 8
GATE_SOURCES = ('.c', '.cc', '.cpp', '.cxx')
OUTPUT_FLAGS = {'-c', '-MD', '-MMD'}
# The flags a gated entry must carry (CMakeLists.txt). In C++ the other two conversions are errors
# without any flag; int-to-pointer-cast is only a warning there.
C_GATE_FLAGS = tuple(f'-Werror={kind}' for kind in KINDS)
CXX_GATE_FLAGS = ('-Werror=int-to-pointer-cast',)

DROPPED_FLAGS = {'-m32', '-msse2', '-mfpmath=sse', '-w', '-c', '-MD', '-MMD'}
DROPPED_WITH_VALUE = {'-o', '-MT', '-MF'}
ADDED_FLAGS = ['-fsyntax-only', '-Wint-conversion', '-Wpointer-to-int-cast',
               '-Wint-to-pointer-cast', '-fmax-errors=0']

DIAGNOSTIC = re.compile(
    r'^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): (?P<level>warning|error|fatal error): '
    r'(?P<text>.*?)(?: \[-W(?P<flag>[\w=-]+)\])?$')
# Any compiler line reporting an error, located or not (`gcc: error: ...`, `cc1: fatal error: ...`).
QUOTED_SOURCE = re.compile(r'^\s*\d+ \| ?(?P<text>.*)$')
LINE_DIRECTIVE = re.compile(r'^[ \t]*#[ \t]*line\b', re.MULTILINE)
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
        flag = (match['flag'] or '').removeprefix('error=')
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


def has_line_directives(text: str) -> bool:
    """Return whether C source `text` contains a `#line` directive."""
    return LINE_DIRECTIVE.search(text) is not None


def strip_line_directives(text: str) -> str:
    """Return `text` with each `#line` directive line emptied, so every other line keeps its number."""
    return '\n'.join('' if LINE_DIRECTIVE.match(line) else line for line in text.split('\n'))


def physical_command(argv: list[str], source: str, copy: str) -> list[str]:
    """Return `argv` compiling `copy` in place of `source`, searching the original's folder first."""
    return argv[:1] + ['-iquote', os.path.dirname(source)] + [copy if arg == source else arg for arg in argv[1:]]


def physical_diagnostics(entry: dict, argv: list[str], root: Path) -> dict:
    """Return the diagnostics in `entry`'s own file at their physical lines.

    The file is compiled again from a copy without its `#line` directives, written to a temporary
    folder outside the source tree; the copy's path is mapped back to the original in the output."""
    source = os.path.normpath(os.path.join(entry['directory'], entry['file']))
    with tempfile.TemporaryDirectory(prefix='lp64-physical-') as folder:
        copy = os.path.join(folder, os.path.basename(source))
        Path(copy).write_text(strip_line_directives(Path(source).read_text(errors='replace')))
        command = physical_command([source if arg == entry['file'] else arg for arg in argv], source, copy)
        try:
            done = subprocess.run(command, cwd=entry['directory'], capture_output=True,
                                  text=True, timeout=COMPILE_TIMEOUT_SECONDS)
        except (OSError, subprocess.TimeoutExpired) as error:
            raise RuntimeError(f"cannot compile a copy of {entry['file']}: {error}") from error
    return parse_with_text(done.stderr.replace(copy, source), root)


def compile_one(entry: dict, argv: list[str], root: Path, physical: bool = False) -> tuple[dict, bool]:
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
    source = os.path.normpath(os.path.join(entry['directory'], entry['file']))
    if physical and not fatal and has_line_directives(Path(source).read_text(errors='replace')):
        own = Path(source).relative_to(root).as_posix() if Path(source).is_relative_to(root) else source
        found = {key: text for key, text in found.items() if key[0] != own}
        found.update({key: text for key, text in physical_diagnostics(entry, argv, root).items()
                      if key[0] == own})
    return found, fatal


def gate_command(entry: dict) -> Optional[list[str]]:
    """Return the entry's own command made syntax-only, or None for a file that is not C or C++."""
    if not entry['file'].endswith(GATE_SOURCES):
        return None
    argv = entry['arguments'] if 'arguments' in entry else shlex.split(entry['command'])
    kept = []
    skip_next = False
    for arg in argv:
        if skip_next:
            skip_next = False
        elif arg in DROPPED_WITH_VALUE:
            skip_next = True
        elif arg not in OUTPUT_FLAGS:
            kept.append(arg)
    return kept + ['-fsyntax-only']


def ungated_reason(entry: dict, argv: list[str]) -> Optional[str]:
    """Return what keeps an entry's flags from making the int/pointer conversions errors, or None.

    A C entry needs all three `-Werror=` flags, a C++ entry `-Werror=int-to-pointer-cast`; `-w`
    switches every `-Werror=` off, so neither may carry it."""
    required = C_GATE_FLAGS if entry['file'].endswith('.c') else CXX_GATE_FLAGS
    reasons = [f'lacks {flag}' for flag in required if flag not in argv]
    if '-w' in argv:
        reasons.append('carries -w')
    return ', '.join(reasons) or None


def build_dir_problem(build_dir: Path) -> Optional[str]:
    """Return why `build_dir` is not a configured 64-bit tree with its generated header, or None."""
    cache = build_dir/'CMakeCache.txt'
    if not cache.is_file() or not (build_dir/'compile_commands.json').is_file():
        return (f'{build_dir} is not a CMake tree with compile_commands.json; configure it with '
                '-DGE_POINTER_BITS=64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON')
    if 'GE_POINTER_BITS:STRING=64' not in cache.read_text(errors='replace').splitlines():
        return f'{build_dir} is not configured with GE_POINTER_BITS=64'
    if not (build_dir/GENERATED_HEADER).is_file():
        return (f'{build_dir} lacks {GENERATED_HEADER}; build that target first: '
                f'cmake --build {build_dir} --target ge_buildinfo')
    return None


def first_error(stderr: str, status: int) -> str:
    """Return the first error line of a failed compile, or what is known when it printed none."""
    lines = stderr.splitlines()
    return next((text for text in lines if ERROR_LINE.match(text)),
                next((text for text in lines if text.strip()), f'exit status {status} with no output'))


def gate_one(entry: dict, argv: list[str]) -> Optional[str]:
    """Compile one entry; return its first error, or None when it compiles."""
    try:
        done = subprocess.run(argv, cwd=entry['directory'], capture_output=True,
                              text=True, timeout=GATE_TIMEOUT_SECONDS)
    except (OSError, subprocess.TimeoutExpired) as error:
        return f'cannot compile: {error}'
    return None if done.returncode == 0 else first_error(done.stderr, done.returncode)


def gate(build_dirs: list[Path]) -> int:
    """Compile every C and C++ entry of each tree; print failures; return the exit status."""
    problems = [problem for build_dir in build_dirs if (problem := build_dir_problem(build_dir))]
    for problem in problems:
        print(f'error: {problem}', file=sys.stderr)
    if problems:
        return 2
    trees = []
    for build_dir in build_dirs:
        entries = json.loads((build_dir/'compile_commands.json').read_text())
        commands = [(entry, argv) for entry in entries if (argv := gate_command(entry)) is not None]
        if not commands:
            print(f'error: {build_dir} holds no C or C++ compile commands', file=sys.stderr)
            return 2
        trees.append((build_dir, commands))
    ungated = [f"error: {build_dir}: {entry['file']} is not gated: {reason}"
               for build_dir, commands in trees for entry, argv in commands
               if (reason := ungated_reason(entry, argv))]
    for line in ungated:
        print(line, file=sys.stderr)
    if ungated:
        return 2
    failed = False
    for build_dir, commands in trees:
        with ThreadPoolExecutor(max_workers=os.cpu_count() or 1) as pool:
            results = list(pool.map(lambda pair: gate_one(*pair), commands))
        failures = sorted((entry['file'], error) for (entry, _), error in zip(commands, results) if error)
        for path, error in failures:
            print(f'{build_dir}: FAIL {path}: {error}')
        print(f'{build_dir}: {len(commands)} files compiled, {len(failures)} failed')
        failed = failed or bool(failures)
    return 1 if failed else 0


def measure(build_dirs: list[Path], root: Path) -> dict:
    """Compile every C file of each tree with its own command; return the diagnostics found, with text.

    Raises RuntimeError naming every file that stopped at a fatal error."""
    commands = []
    for build_dir in build_dirs:
        path = build_dir/'compile_commands.json'
        commands += c_entries(json.loads(path.read_text()), path)
    with ThreadPoolExecutor(max_workers=min(MAX_WORKERS, os.cpu_count() or 1)) as pool:
        results = list(pool.map(lambda pair: compile_one(*pair, root, physical=True), commands))
    stopped = sorted({entry['file'] for (entry, _), (_, fatal) in zip(commands, results) if fatal})
    if stopped:
        raise RuntimeError('stopped at a fatal error, so not measured: ' + ', '.join(stopped))
    merged = {}
    for found, _ in results:
        for key, text in found.items():
            merged.setdefault(key, text)
    return merged


def main(argv: list[str]) -> int:
    """Run `gate` or `sites`; return the process exit status."""
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    gate_parser = commands.add_parser('gate', help='compile every C and C++ file of 64-bit trees')
    gate_parser.add_argument('build_dirs', nargs='+', type=Path, metavar='BUILD_DIR')
    sites_parser = commands.add_parser('sites', help='list int/pointer diagnostics of the C files')
    sites_parser.add_argument('files', nargs='*', help='repository-relative files to list')
    sites_parser.add_argument('--build-dir', type=Path, action='append', dest='build_dirs',
                              help='a 64-bit tree, as for gate; repeat for several '
                                   '(default: build/lp64/gate and build/lp64/gate-video)')
    sites_parser.add_argument('--messages', action='store_true',
                              help="add GCC's message and the quoted source line")
    args = parser.parse_args(argv)
    if args.command == 'gate':
        return gate([path.resolve() for path in args.build_dirs])
    build_dirs = [path.resolve() for path in args.build_dirs or DEFAULT_BUILD_DIRS]
    problems = [problem for build_dir in build_dirs if (problem := build_dir_problem(build_dir))]
    try:
        if problems:
            raise RuntimeError('\n'.join(problems))
        details = measure(build_dirs, ROOT)
    except RuntimeError as error:
        print(f'error: {error}', file=sys.stderr)
        return 2
    for line in site_lines(details if args.messages else set(details), args.files or None, args.messages):
        print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
