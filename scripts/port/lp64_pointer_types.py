#!/usr/bin/env python3
"""Print the record types a 64-bit PC build only ever reaches through a pointer (LP64-005 Task 3, R11).

    python3 scripts/port/lp64_pointer_types.py scan BUILD_DIR [BUILD_DIR...] [--explain TYPE]

Each BUILD_DIR is a 64-bit CMake tree configured with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (and its
`ge_buildinfo` target built), as the CI tests step configures `build/lp64/gate` and `gate-video`; run it
in the container of that tree. Every C and C++ entry of each tree's `compile_commands.json` is run
through its own compiler with `-E`, so what is read is exactly what the 64-bit build compiles: headers
only where they are included, every `#if` evaluated by the compiler.

The types are the structs and unions defined in the record headers (`src/bondtypes.h`, `src/game/*.h`,
`include/PR/libaudio.h`) and in the `.c` files the overlay lister reads (`src/`, `port/src/`,
`port/include/`), each known by its tag and its typedef names (`typedef struct X Name;` included). A
type is printed when no compiled line declares an object of it (a variable, a member, a by-value
parameter or return, a cast to the type itself or a compound literal), embeds it in a definition, or
takes its `sizeof`: the build then holds no object of that type, and whatever its pointers point to
is some other record seen through it. That is R11's question: which record it really is, and whether
every member read through it is the same at both widths.

What it does not see: a `sizeof *p` or `sizeof(p[0])` of a pointer to the type counts as nothing,
and a type named only through a macro the compiler expands elsewhere is read as expanded, so the
counted use is at the macro's call. Each printed type gives the number of pointer uses and the first
files they are in, where its pointers come from.

Exit status 0 when every tree was read, 1 when a tree or a compile failed (nothing is printed then).
"""
import argparse
import bisect
import collections
import json
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RECORD_FILES = re.compile(r'^(src/bondtypes\.h|src/game/[^/]+\.h|include/PR/libaudio\.h|(src|port/src|port/include)/.+\.c)$')
SOURCES = ('.c', '.cc', '.cpp', '.cxx')
DROPPED_FLAGS = {'-c', '-MD', '-MMD', '-w'}
DROPPED_WITH_VALUE = {'-o', '-MT', '-MF'}
COMPILE_TIMEOUT_SECONDS = 120
MAX_WORKERS = 8
MARKER = re.compile(r'^# (\d+) "([^"]*)"')
QUALIFIERS = re.compile(r'(?:\s|\bconst\b|\bvolatile\b|\brestrict\b|\b__restrict\b)*')
DEFINITION = re.compile(r'\b(struct|union)\s+(\w+)?\s*\{')
OBJECT, POINTER, SIZED, OTHER = 'object', 'pointer', 'sized', 'other'


class Unit:
    """One preprocessed translation unit: its code with line markers removed, and each line's origin."""

    def __init__(self, preprocessed: str, root: str):
        lines, self.origins, self.starts = [], [], []
        origin, number, offset = '', 0, 0
        for line in preprocessed.split('\n'):
            marker = MARKER.match(line)
            if marker:
                number, origin = int(marker.group(1)), relative(marker.group(2), root)
                continue
            if line.startswith('#'):
                line = ''
            self.starts.append(offset)
            self.origins.append((origin, number))
            lines.append(line)
            offset += len(line) + 1
            number += 1
        self.code = '\n'.join(lines)

    def origin(self, offset: int) -> tuple[str, int]:
        return self.origins[bisect.bisect_right(self.starts, offset) - 1]


def relative(path: str, root: str) -> str:
    """Return `path` relative to the checkout, or '' for a file outside it (a system header)."""
    root = root.rstrip('/') + '/'
    path = os.path.normpath(os.path.join(root, path))
    return path[len(root):] if path.startswith(root) else ''


LINE_DIRECTIVE = re.compile(r'^\s*#\s*line\s+(\d+)')


def physical_lines(path: Path) -> dict[int, int]:
    """Map the line numbers the compiler reports for a file with `#line` directives to its physical lines."""
    mapping, logical = {}, 1
    for physical, text in enumerate(path.read_text(errors='replace').split('\n'), start=1):
        directive = LINE_DIRECTIVE.match(text)
        if directive:
            logical = int(directive.group(1))
            continue
        mapping.setdefault(logical, physical)
        logical += 1
    return mapping


class Physical:
    """Physical line numbers for reported origins, read once a file."""

    def __init__(self, root: Path):
        self.root, self.files = root, {}

    def __call__(self, origin: tuple[str, int]) -> str:
        path, line = origin
        if not path:
            return f'(system):{line}'
        if path not in self.files:
            source = self.root/path
            self.files[path] = physical_lines(source) if source.is_file() else {}
        return f'{path}:{self.files[path].get(line, line)}'


def matching(code: str, open_brace: int) -> int:
    """Return the index just past the brace that closes the one at `open_brace`."""
    depth = 0
    for index in range(open_brace, len(code)):
        if code[index] == '{':
            depth += 1
        elif code[index] == '}':
            depth -= 1
            if depth == 0:
                return index + 1
    raise ValueError(f'unclosed brace at {open_brace}')


def declarators(code: str, close: int) -> tuple[list[tuple[str, bool]], int]:
    """Return the (name, is a pointer) declarators after a closing brace, and the index of their `;`."""
    end = code.find(';', close)
    found = []
    for part in code[close:end].split(','):
        name = re.findall(r'\w+', re.sub(r'\[[^\]]*\]|__attribute__\s*\(\(.*?\)\)', '', part))
        if name:
            found.append((name[-1], '*' in part))
    return found, end


class Types:
    """The record types of a set of units: canonical name, where defined, and every name it goes by."""

    def __init__(self):
        self.defined: dict[str, tuple[str, int]] = {}
        self.tags: dict[str, str] = {}
        self.names: dict[str, str] = {}
        self.aliases: list[tuple[str, str]] = []

    def add_unit(self, unit: Unit) -> None:
        code = unit.code
        for match in DEFINITION.finditer(code):
            origin = unit.origin(match.start())
            if not RECORD_FILES.match(origin[0]):
                continue
            close = matching(code, match.end() - 1)
            typedef = code[max(0, match.start() - 40):match.start()].rstrip().endswith('typedef')
            names = [name for name, pointer in declarators(code, close)[0] if not pointer] if typedef else []
            tag = match.group(2)
            if not tag and not names:
                continue
            canonical = tag or names[0]
            self.defined.setdefault(canonical, origin)
            if tag:
                self.tags[tag] = canonical
            for name in names:
                self.names[name] = canonical
        self.aliases += re.findall(r'\btypedef\s+(?:struct|union)\s+(\w+)\s+(\w+)\s*;', code)
        self.aliases += [('=' + old, new) for old, new in re.findall(r'\btypedef\s+(\w+)\s+(\w+)\s*;', code)]

    def resolve(self) -> None:
        """Give each `typedef struct X Name;` and `typedef Old New;` name the type it names."""
        changed = True
        while changed:
            changed = False
            for old, new in self.aliases:
                target = self.names.get(old[1:]) if old.startswith('=') else self.tags.get(old)
                if target and new not in self.names:
                    self.names[new] = target
                    changed = True


def classify(code: str, start: int, end: int) -> str:
    """Return how the type reference code[start:end] is used: object, pointer, sized or other."""
    before = code[max(0, start - 40):start].rstrip()
    after_index = end + QUALIFIERS.match(code, end).end() - end
    after = code[after_index:after_index + 2]
    if re.search(r'\bsizeof\s*\($|\b_Alignof\s*\($', before):
        return SIZED if after[:1] in ')[' else POINTER
    if re.search(r'__builtin_offsetof\s*\($', before):
        return OTHER
    if after[:1] in '*&':
        return POINTER
    if after.startswith('(') and re.match(r'\(\s*[*&]', code[after_index:after_index + 8]):
        return POINTER
    if after[:1] in ';':
        return OTHER
    return OBJECT


def inside(ranges: list[tuple[int, int]], offset: int) -> bool:
    """Return whether `offset` falls in one of the sorted, disjoint (start, end) ranges."""
    index = bisect.bisect_right(ranges, (offset, float('inf'))) - 1
    return index >= 0 and ranges[index][0] <= offset < ranges[index][1]


def uses_in(unit: Unit, types: Types, skip: set) -> list[tuple[str, str, tuple[str, int]]]:
    """Return (canonical type, use, origin) for every reference to a record type in one unit.

    `skip` collects origins already counted (a header line seen in many units is counted once)."""
    code, found = unit.code, []
    excluded = []
    for match in DEFINITION.finditer(code):
        close = matching(code, match.end() - 1)
        typedef = code[max(0, match.start() - 40):match.start()].rstrip().endswith('typedef')
        names, end = declarators(code, close)
        if typedef:
            excluded.append((close, end))
            continue
        canonical = types.tags.get(match.group(2) or '')
        if canonical and any(not pointer for _, pointer in names):
            found.append((canonical, OBJECT, unit.origin(close)))
    for match in re.finditer(r'\b(?:(struct|union)\s+)?(\w+)\b', code):
        keyword, name = match.group(1), match.group(2)
        canonical = types.tags.get(name) if keyword else types.names.get(name)
        if not canonical or inside(excluded, match.start()):
            continue
        if not keyword and re.search(r'(\.|->|\bstruct|\bunion|\benum)\s*$', code[max(0, match.start() - 8):match.start()]):
            continue
        if keyword and code[match.end():match.end() + 8].lstrip().startswith('{'):
            continue
        if re.search(r'\btypedef\s+$', code[max(0, match.start() - 12):match.start()]):
            continue
        origin = unit.origin(match.start())
        key = (canonical, origin, match.start() - unit.starts[bisect.bisect_right(unit.starts, match.start()) - 1])
        if key in skip:
            continue
        skip.add(key)
        found.append((canonical, classify(code, match.start(), match.end()), origin))
    return found


def report(types: Types, uses: list[tuple[str, str, tuple[str, int]]], place) -> list[str]:
    """Return one line a pointer-only type: name, definition, pointer uses and their first files.

    A type the build names nowhere outside its definition is printed with `unused` in place of the
    count: no compiled code reaches anything through it."""
    counts = collections.defaultdict(collections.Counter)
    files = collections.defaultdict(collections.Counter)
    for canonical, use, origin in uses:
        counts[canonical][use] += 1
        if use == POINTER:
            files[canonical][origin[0] or '(system)'] += 1
    lines = []
    for canonical, origin in sorted(types.defined.items(), key=lambda item: (item[1], item[0])):
        if counts[canonical][OBJECT] or counts[canonical][SIZED]:
            continue
        names = sorted({name for name, target in types.names.items() if target == canonical} - {canonical})
        where = ', '.join(f'{f} {n}' for f, n in sorted(files[canonical].items(), key=lambda i: (-i[1], i[0]))[:4])
        also = f' (also {", ".join(names)})' if names else ''
        used = sum(counts[canonical].values())
        count = f'{counts[canonical][POINTER]} pointer uses' if used else 'unused'
        lines.append(f'{canonical}{also}\t{place(origin)}\t{count}\t{where or "-"}')
    return lines


def preprocess_command(entry: dict) -> list[str] | None:
    """Return the entry's own command made to print its preprocessed text, or None for a non-source."""
    if not entry['file'].endswith(SOURCES):
        return None
    argv = entry['arguments'] if 'arguments' in entry else shlex.split(entry['command'])
    kept, skip_next = [], False
    for arg in argv:
        if skip_next:
            skip_next = False
        elif arg in DROPPED_WITH_VALUE:
            skip_next = True
        elif arg not in DROPPED_FLAGS:
            kept.append(arg)
    return kept + ['-E']


def preprocess(entry: dict, argv: list[str]) -> str:
    result = subprocess.run(argv, cwd=entry['directory'], capture_output=True, text=True,
                            timeout=COMPILE_TIMEOUT_SECONDS, errors='replace')
    if result.returncode != 0:
        raise RuntimeError(f'{entry["file"]}: the preprocessor exited {result.returncode}: {result.stderr[:400]}')
    return result.stdout


def explain(uses: list[tuple[str, str, tuple[str, int]]], name: str, place) -> list[str]:
    """Return each counted use of the type `name` as `file:line: use`."""
    return sorted(f'{place(origin)}: {use}' for canonical, use, origin in uses if canonical == name)


def scan(build_dirs: list[Path], maybe_explain: str | None = None) -> list[str]:
    commands = []
    for build_dir in build_dirs:
        path = build_dir/'compile_commands.json'
        if not path.is_file():
            raise RuntimeError(f'{build_dir} has no compile_commands.json; configure it with '
                               '-DGE_POINTER_BITS=64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON')
        found = [(e, argv) for e in json.loads(path.read_text()) if (argv := preprocess_command(e)) is not None]
        if not found:
            raise RuntimeError(f'{path} holds no C or C++ compile commands; refusing to report none')
        commands += found
    with ThreadPoolExecutor(MAX_WORKERS) as pool:
        texts = list(pool.map(lambda command: preprocess(*command), commands))
    units = [Unit(text, str(ROOT)) for text in texts]
    types = Types()
    for unit in units:
        types.add_unit(unit)
    types.resolve()
    skip, uses = set(), []
    for unit in units:
        uses += uses_in(unit, types, skip)
    place = Physical(ROOT)
    if maybe_explain:
        name = types.names.get(maybe_explain) or types.tags.get(maybe_explain) or maybe_explain
        return explain(uses, name, place)
    return report(types, uses, place)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    scan_command = commands.add_parser('scan', help='print the types reached only through pointers')
    scan_command.add_argument('build_dirs', nargs='+', type=Path)
    scan_command.add_argument('--explain', metavar='TYPE', help='print every counted use of one type instead')
    args = parser.parse_args(argv)
    try:
        lines = scan(args.build_dirs, args.explain)
    except (RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        print(f'lp64_pointer_types: {error}', file=sys.stderr)
        return 1
    print('\n'.join(lines))
    if not args.explain:
        print(f'{len(lines)} types reached only through pointers', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
