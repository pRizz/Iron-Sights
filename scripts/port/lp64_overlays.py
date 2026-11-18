#!/usr/bin/env python3
"""List every place the C sources read a record through another type, and check each has a verdict.

`list [PATH...]` prints one site a line, `file:function:kind:text` (`--lines` adds each site's line
after a tab). `check` requires every site to have exactly one line in `lp64_overlays_reviewed.txt`,
`file:function:kind:text<TAB>VERDICT<TAB>reason`, and every line there to match a site; it prints
`OK: N sites, all reviewed`, or each `unreviewed:` site and `stale:` line and exits 1.

Kinds (patterns on C text, not a C parser; see lp64_overlays_kinds.py and lp64_overlays_types.py):
  index   ((u16 *) x)[n]            an index through a cast to a pointer to a scalar or to a pointer
  offset  *(T *) ((u8 *) x + K)     a dereference of a cast of a sum, or *((s32 *) x + 2)
  opaque  x->act_bytes.padding[84]  a literal index into a member named pad..., padding... or unk...
  record  (ChrRecord *) p->voidp    a cast to a pointer to a struct or union of the record headers
  run     Gfx dl[2]; +2 fillers     a Gfx, Model or record member followed by field_/pad/unk fillers

Files: tracked `.c` and `.h` files under src/, port/src/ and port/include/, except the romconv_,
romfile_ and romdata loaders (their inputs are file layouts). Left out: port/fast3d (the C++
renderer; its address keys, such as roc_part_key, are another pattern and are read by hand), port/web
(the browser build, 32-bit wasm only), port/crash, port/win and include/ (the SDK headers). Naming a
PATH lists only the files under it, and may name a folder the default leaves out (C++ is read as C).
Record types come from src/bondtypes.h, src/game/*.h and include/PR/libaudio.h; `run` reads every
header listed and include/PR/libaudio.h.

Lines a 64-bit PC build does not compile are left out: `#ifdef TARGET_N64`, `#if !IS_64_BIT`, `#if 0`,
the `#else` of `#if IS_64_BIT`, and the symbols no PC build defines (DEBUG, VERSION_EU, VERSION_JP and
the rest of UNDEFINED_ON_PC), and _WIN32, __EMSCRIPTEN__ and GE_WEB, which no 64-bit build defines
(CMakeLists.txt refuses 64 bits on Windows and Emscripten). A branch on a symbol a PC build may define (GE_VIDEO, __APPLE__) is kept with
its `#else`. Macro definitions are read: a cast in a `#define` is a site owned by `#define NAME`.

A site's key has no line number, so moving code changes no key; whitespace is normalized, so does
reformatting. Any other change to the text is a new site, which must be reviewed. The Nth occurrence
of one key in a file has ` #N` appended to its text.

What it cannot see: a cast written through a typedef'd pointer type, a `void *` assigned to a record
pointer without a cast (C needs none), an offset added to a `u8 *` variable assigned earlier, memcpy
between records of different types, a union read through another member, and a cast to a type not
declared in the record headers (Gfx, the port's own structs), each expansion of a macro (its
definition is the one site), and a `//` comment ending in a backslash, which C continues on the next
line. A file whose compiled code leaves braces unbalanced is refused (exit 2), not misread. FORK-089's
"LP64-005 task 3" note says which other check covers each blind spot.
"""
import argparse
from pathlib import Path
import subprocess
import sys

from lp64_overlays_kinds import RecordTypes, Site, sites_in_prepared
from lp64_overlays_source import DEFINED_ON_PC, UNDEFINED_AT_64, UNDEFINED_ON_PC, SourceError, prepare
from lp64_overlays_types import record_types, runs_in

__all__ = ['DEFINED_ON_PC', 'UNDEFINED_AT_64', 'UNDEFINED_ON_PC', 'SourceError', 'record_types', 'runs_in', 'sites_in',
           'in_scope', 'list_sites', 'parse_reviewed', 'report', 'REVIEWED', 'main']

ROOT = Path(__file__).resolve().parents[2]
REVIEWED = Path(__file__).with_name('lp64_overlays_reviewed.txt')
SCOPE_DIRS = ('src/', 'port/src/', 'port/include/')
SUFFIXES = ('.c', '.h')
EXPLICIT_SUFFIXES = ('.c', '.h', '.cpp', '.inl', '.cc')
LIBAUDIO = 'include/PR/libaudio.h'
VERDICTS = ('typed', 'same', 'overlay-64', 'dead')
GIT_TIMEOUT_SECONDS = 60


def is_loader(path: str) -> bool:
    name = path.rsplit('/', 1)[-1]
    return 'romconv_' in name or name.startswith(('romfile_', 'romdata'))


def in_scope(path: str) -> bool:
    """Return whether `path` is one of the files `list` and `check` read by default."""
    return path.startswith(SCOPE_DIRS) and path.endswith(SUFFIXES) and not is_loader(path)


def is_record_header(path: str) -> bool:
    return path == 'src/bondtypes.h' or path == LIBAUDIO or \
        (path.startswith('src/game/') and path.count('/') == 2 and path.endswith('.h'))


def sites_in(path: str, text: str, types: RecordTypes) -> list[Site]:
    """Return the index, offset, opaque and record sites of one file's text."""
    return sites_in_prepared(path, prepare(text), types)


def read(root: Path, path: str) -> str:
    return (root/path).read_text(errors='replace')


def list_sites(root: Path, files: list[str], paths: list[str] | None = None) -> list[Site]:
    """Return every site of `files` (relative to `root`) that the scope, or the given paths, take."""
    if paths:
        chosen = [f for f in files if f.endswith(EXPLICIT_SUFFIXES) and not is_loader(f)
                  and any(f == p.rstrip('/') or f.startswith(p.rstrip('/') + '/') for p in paths)]
    else:
        chosen = [f for f in files if in_scope(f)]
    headers = {f: read(root, f) for f in files if is_record_header(f)}
    if (root/LIBAUDIO).exists():
        headers.setdefault(LIBAUDIO, read(root, LIBAUDIO))
    types = record_types(headers)
    sites = []
    run_sources = [f for f in chosen if f.endswith('.h')] + ([LIBAUDIO] if LIBAUDIO in headers and not paths else [])
    for path in chosen:
        try:
            sites += sites_in(path, read(root, path), types)
        except SourceError as error:
            raise SourceError(f'{path}: {error}') from None
    for path in run_sources:
        sites += runs_in(path, headers.get(path) or read(root, path), types)
    return sorted(sites, key=lambda site: (site.path, site.line, site.key))


def parse_reviewed(text: str) -> dict[str, tuple[str, str]]:
    """Return key -> (verdict, reason) from the reviewed list; `#` lines and blank lines are skipped."""
    reviewed = {}
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip() or line.startswith('#'):
            continue
        fields = line.split('\t')
        if len(fields) != 3 or fields[1] not in VERDICTS or not fields[2].strip():
            raise ValueError(f'reviewed line {number} is not key<TAB>VERDICT<TAB>reason with VERDICT one of '
                             f'{", ".join(VERDICTS)}: {line!r}')
        if fields[0] in reviewed:
            raise ValueError(f'reviewed line {number} repeats a key: {fields[0]!r}')
        reviewed[fields[0]] = (fields[1], fields[2])
    return reviewed


def report(sites: list[Site], reviewed: dict[str, tuple[str, str]]) -> int:
    """Print the result of `check` and return its exit status."""
    keys = {site.key: site for site in sites}
    unreviewed = [site for key, site in keys.items() if key not in reviewed]
    stale = [key for key in reviewed if key not in keys]
    for site in unreviewed:
        print(f'unreviewed: {site.key}\t(line {site.line})')
    for key in stale:
        print(f'stale: {key}')
    if unreviewed or stale:
        print(f'{len(unreviewed)} unreviewed sites, {len(stale)} stale lines. Read each unreviewed site and '
              f'add its verdict to {REVIEWED.name}; remove each stale line (a site whose text changed is a new site).')
        return 1
    print(f'OK: {len(keys)} sites, all reviewed')
    return 0


def tracked_files(root: Path) -> list[str]:
    out = subprocess.run(['git', 'ls-files', '-z'], cwd=root, capture_output=True, text=True,
                         check=True, timeout=GIT_TIMEOUT_SECONDS).stdout
    return [path for path in out.split('\0') if path]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('command', choices=('list', 'check'))
    parser.add_argument('paths', nargs='*', help='list: only the files under these paths')
    parser.add_argument('--lines', action='store_true', help='list: add each site\'s line after a tab')
    args = parser.parse_args(argv)
    if args.command == 'check' and args.paths:
        parser.error('check reads the whole scope; it takes no paths')
    try:
        sites = list_sites(ROOT, tracked_files(ROOT), args.paths)
        reviewed = parse_reviewed(REVIEWED.read_text()) if args.command == 'check' else {}
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 2
    if args.command == 'check':
        return report(sites, reviewed)
    for site in sites:
        print(f'{site.key}\t{site.line}' if args.lines else site.key)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
