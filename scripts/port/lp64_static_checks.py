"""The `narrowing` and `unprototyped` checks of lp64_diagnostics.py, which cannot be told apart from
ordinary C by the 64-bit compile gate.

`narrowing` compiles every C and C++ entry syntax-only with `-Wconversion -Wno-sign-conversion
-Wno-float-conversion` under `LC_ALL=C` and keeps each implicit conversion whose source type is
`uintptr_t`, `intptr_t`, `romptr_t` or `Aword` (`PR/abi.h`'s audio command word, `uintptr_t` at
64 bits). `--all` adds the other 64-bit source types as a report.
GCC drops the typedef name after arithmetic, so `(uintptr_t) a + b` narrowed to `u32` shows as
`long unsigned int` and is found only under `--all`. A file with `#line` directives (`gunfire.c`,
`gun.c`, `lv.c`, `env.c`) is listed at the compiler's line numbers, which are not its own lines.

`unprototyped` compiles every C entry with `-Wimplicit-function-declaration` restored and
`-aux-info`, and keeps each call without a prototype to a function whose definition in the tree
takes or returns a pointer or a 64-bit type: such a call passes an `int`-sized argument where the
callee reads a pointer. A call to a function the tree does not define (a C library function such
as `memset` or `strlen`, whose `size_t` and pointers the tree cannot show) is kept too, under
either of GCC's wordings ("implicit declaration of function", and "incompatible implicit
declaration of built-in function" where the built-ins are on).

Both keep one line per place, `file:line:column: ...`, and `--check` requires each file's count to
equal its line in an allowlist (`path<TAB>count<TAB>reason`), as `lp64_casts.py check` does.
"""
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Optional

COMPILE_TIMEOUT_SECONDS = 600
POINTER_SIZED = {'uintptr_t', 'intptr_t', 'romptr_t', 'Aword'}  # Aword: PR/abi.h's audio command word
OTHER_64_BIT = {'long int', 'long unsigned int', 'long long int', 'long long unsigned int', 'size_t',
                'ptrdiff_t', 'ssize_t', 'u64', 's64', 'uint64_t', 'int64_t', 'Uint64', 'Sint64'}
NARROWING_FLAGS = ['-Wconversion', '-Wno-sign-conversion', '-Wno-float-conversion', '-Wno-error',
                   '-fmax-errors=0']
IMPLICIT_FLAGS = ['-Wimplicit-function-declaration', '-Wno-error', '-fmax-errors=0']

CONVERSION = re.compile(
    r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): warning: conversion from '(?P<src>[^']+)'(?: \{aka '(?P<aka>[^']+)'\})? "
    r"to '(?P<dst>[^']+)'(?: \{aka '[^']+'\})? may change value")
IMPLICIT = re.compile(
    r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): (?:warning|error): (?:incompatible )?implicit declaration of "
    r"(?:built-in )?function '(?P<name>[^']+)'")
QUOTED_SOURCE = re.compile(r'^\s*\d+ \| ?(?P<text>.*)$')
# `/* file:line:NF */ extern void f (char *); ` is a prototyped definition; `OF` an old-style one.
DEFINITION = re.compile(r'^/\* (?P<file>.+?):\d+:[NO]F \*/ (?P<decl>.*?);\s*(?:/\*(?P<old>.*)\*/)?\s*$')
SIGNATURE = re.compile(r'^(?P<head>.*?)\b(?P<name>\w+) \((?P<params>.*)\)$')
POINTER_OR_64_BIT = re.compile(r'\*|\blong\b|\b(?:uintptr_t|intptr_t|romptr_t|size_t|ptrdiff_t|ssize_t|'
                               r'u64|s64|uint64_t|int64_t)\b')


def relative(path: str, directory: str, root: Path) -> Optional[str]:
    """Return `path` (as the compiler printed it) relative to `root`, or None outside the repository."""
    full = Path(os.path.normpath(os.path.join(directory, path)))
    return full.relative_to(root).as_posix() if full.is_relative_to(root) else None


def parse_narrowing(stderr: str, directory: str, root: Path, every_type: bool) -> dict[tuple, str]:
    """Return (file, line, column, source, target) -> quoted source text for each narrowing of interest."""
    found = {}
    lines = stderr.splitlines()
    for index, text in enumerate(lines):
        match = CONVERSION.match(text)
        if not match:
            continue
        wide = POINTER_SIZED | OTHER_64_BIT if every_type else POINTER_SIZED
        if match['src'] not in wide and (not every_type or match['aka'] not in wide):
            continue
        path = relative(match['file'], directory, root)
        if path is None:
            continue
        quoted = QUOTED_SOURCE.match(lines[index + 1]) if index + 1 < len(lines) else None
        key = (path, int(match['line']), int(match['col']), match['src'], match['dst'])
        found.setdefault(key, quoted['text'].strip() if quoted else '')
    return found


def narrowing_lines(found: dict[tuple, str]) -> list[str]:
    """Return `file:line:column: SOURCE -> TARGET | source text` per place, by file then position."""
    return [f'{path}:{line}:{col}: {src} -> {dst} | {found[(path, line, col, src, dst)]}'
            for path, line, col, src, dst in sorted(found)]


def definitions(aux: str) -> list[tuple[str, str, str]]:
    """Return (name, declaration, parameter text) of each function the `-aux-info` text defines."""
    found = []
    for line in aux.splitlines():
        match = DEFINITION.match(line)
        signature = SIGNATURE.match(match['decl']) if match else None
        if signature:
            params = signature['params'] or (match['old'] or '')
            found.append((signature['name'], match['decl'], f"{signature['head']} {params}"))
    return found


def parse_unprototyped(stderr: str, directory: str, root: Path) -> dict[tuple, str]:
    """Return (file, line, column) -> function name for each implicit declaration in the repository."""
    found = {}
    for text in stderr.splitlines():
        match = IMPLICIT.match(text)
        path = relative(match['file'], directory, root) if match else None
        if path is not None:
            found.setdefault((path, int(match['line']), int(match['col'])), match['name'])
    return found


def run_compiler(entry: dict, argv: list[str]) -> str:
    """Run one command under LC_ALL=C; return its stderr. A failing compile was not measured: raise."""
    try:
        done = subprocess.run(argv, cwd=entry['directory'], capture_output=True, text=True,
                              timeout=COMPILE_TIMEOUT_SECONDS, env={**os.environ, 'LC_ALL': 'C'})
    except (OSError, subprocess.TimeoutExpired) as error:
        raise RuntimeError(f"cannot compile {entry['file']}: {error}") from error
    if done.returncode != 0:
        raise RuntimeError(f"{entry['file']} failed with status {done.returncode}, so is not measured:\n{done.stderr}")
    return done.stderr


def measure_narrowing(commands: list[tuple[dict, list[str]]], root: Path, every_type: bool) -> dict[tuple, str]:
    """Compile every command with the narrowing flags; return the merged narrowings."""
    def one(pair):
        entry, argv = pair
        return parse_narrowing(run_compiler(entry, argv + NARROWING_FLAGS), entry['directory'], root, every_type)
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 1) as pool:
        results = list(pool.map(one, commands))
    return {key: text for found in results for key, text in found.items()}


def measure_unprototyped(commands: list[tuple[dict, list[str]]], root: Path) -> list[str]:
    """Compile every C command with `-aux-info`; return `file:line:column: NAME | PROTOTYPE` per call.

    PROTOTYPE is the definition's, or `no definition in the tree` for a library function."""
    def one(pair):
        entry, argv = pair
        with tempfile.TemporaryDirectory(prefix='lp64-aux-') as folder:
            aux = os.path.join(folder, 'aux-info.txt')
            stderr = run_compiler(entry, argv + IMPLICIT_FLAGS + ['-aux-info', aux])
            text = Path(aux).read_text(errors='replace') if os.path.exists(aux) else ''
        return parse_unprototyped(stderr, entry['directory'], root), definitions(text)
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 1) as pool:
        results = list(pool.map(one, commands))
    risky, defined_names = {}, set()
    for _, defined in results:
        for name, decl, rest in defined:
            defined_names.add(name)
            if POINTER_OR_64_BIT.search(rest):
                risky.setdefault(name, decl)
    for found, _ in results:
        for name in found.values():
            if name not in defined_names:
                risky.setdefault(name, 'no definition in the tree')
    calls = {key: name for found, _ in results for key, name in found.items() if name in risky}
    return [f'{path}:{line}:{col}: {name} | {risky[name]}' for (path, line, col), name in sorted(calls.items())]


def parse_allowlist(text: str) -> dict[str, int]:
    """Return path -> allowed count from `path<TAB>count<TAB>reason` lines; `#` lines and blanks are skipped."""
    allowed = {}
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip() or line.startswith('#'):
            continue
        fields = line.split('\t')
        if len(fields) != 3 or not fields[1].isdigit() or not fields[2].strip():
            raise ValueError(f'allowlist line {number} is not path<TAB>count<TAB>reason: {line!r}')
        allowed[fields[0]] = int(fields[1])
    return allowed


def check(lines: list[str], noun: str, allowlist: Path, command: str) -> int:
    """Print each file whose count differs from its allowlist line; return 1 if any, else 0.

    `lines` are `path:...` places; `noun` names what is counted ("narrowings"). A missing allowlist allows none."""
    allowed = parse_allowlist(allowlist.read_text()) if allowlist.exists() else {}
    sites: dict[str, list[str]] = {}
    for line in lines:
        sites.setdefault(line.split(':', 1)[0], []).append(line)
    problems = 0
    for path in sorted(set(sites) | set(allowed)):
        actual, granted = len(sites.get(path, [])), allowed.get(path, 0)
        if actual == granted:
            continue
        problems += 1
        fix = '' if actual > granted else \
            f'; lower the allowlist line to {actual}' if actual else '; remove the allowlist line'
        print(f'{path}: {actual} {noun}, allowed {granted}{fix}')
        print('\n'.join(f'  {line}' for line in sites.get(path, [])))
    if problems:
        print(f'The allowlist ({allowlist.name}) must match exactly. Fix the declaration instead of '
              f'adding to it; `{command}` lists every place.')
        return 1
    print(f'OK: {len(lines)} {noun} in {len(sites)} files, all allowed')
    return 0
