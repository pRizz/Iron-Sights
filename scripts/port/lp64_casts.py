#!/usr/bin/env python3
"""Stop truncating pointer round trips from hiding 64-bit problems.

A cast such as `(u32)(uintptr_t)p` silences the compiler's pointer-to-integer warning without
fixing anything: at 64 bits it drops the high half. `lp64_casts_allowed.txt` records how many of
them each tracked source file holds (`path<TAB>count<TAB>reason`). `check` requires an exact match:
it fails when a file holds more than it is allowed (or any and is not listed), and when it holds
fewer (or none, or is no longer tracked) so the line must be lowered or removed; `list` prints every
site. Only tracked
`*.c`, `*.h`, `*.cpp` and `*.inl` files under `src/`, `port/` and `include/` are read.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
ALLOWLIST = Path(__file__).with_name('lp64_casts_allowed.txt')
SCANNED_DIRS = ('src/', 'port/', 'include/')
SCANNED_SUFFIXES = ('.c', '.h', '.cpp', '.inl')
GIT_TIMEOUT_SECONDS = 60

TRUNCATING_CAST = re.compile(
    r'\(\s*(?:u32|s32|int|unsigned\s+int|unsigned|u16|s16)\s*\)\s*\(\s*(?:uintptr_t|intptr_t)\s*\)')


def find_casts(text: str) -> list[int]:
    """Return the 1-based line of every truncating round-trip cast in `text`."""
    return [text.count('\n', 0, match.start()) + 1 for match in TRUNCATING_CAST.finditer(text)]


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


def violations(sites: dict[str, list[int]], allowed: dict[str, int]) -> list[tuple[str, str]]:
    """Return (path, message) for each file whose cast count differs from its allowlist line."""
    found = []
    for path in sorted(set(sites) | set(allowed)):
        actual, granted = len(sites.get(path, [])), allowed.get(path, 0)
        if actual > granted:
            found.append((path, f'{path}: {actual} truncating casts, allowed {granted}'))
        elif actual < granted:
            fix = f'lower the allowlist line to {actual}' if actual else 'remove the allowlist line'
            found.append((path, f'{path}: {actual} truncating casts, allowed {granted}; {fix}'))
    return found


def tracked_files(root: Path) -> list[str]:
    """Return the repository's tracked files, from `git ls-files`."""
    out = subprocess.run(['git', 'ls-files', '-z'], cwd=root, capture_output=True, text=True,
                         check=True, timeout=GIT_TIMEOUT_SECONDS).stdout
    return [path for path in out.split('\0') if path]


def scan(root: Path, files: list[str]) -> dict[str, list[int]]:
    """Return path -> cast lines for each scanned file that holds any."""
    sites = {}
    for path in files:
        if not path.startswith(SCANNED_DIRS) or not path.endswith(SCANNED_SUFFIXES):
            continue
        lines = find_casts((root/path).read_text(errors='replace'))
        if lines:
            sites[path] = lines
    return sites


def main(argv: list[str]) -> int:
    """Run `check` or `list`; return the process exit status."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('check', 'list'))
    args = parser.parse_args(argv)
    try:
        sites = scan(ROOT, tracked_files(ROOT))
        allowed = parse_allowlist(ALLOWLIST.read_text())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 2
    if args.command == 'list':
        for path, lines in sites.items():
            print('\n'.join(f'{path}:{line}' for line in lines))
        return 0
    problems = violations(sites, allowed)
    for path, message in problems:
        print(message)
        print('\n'.join(f'  {path}:{line}' for line in sites.get(path, [])))
    if problems:
        print('The allowlist must match exactly. A truncating round trip hides a 64-bit problem: fix the '
              'declaration instead; where the true fix belongs to a later milestone, comment it and raise the line.')
        return 1
    print(f'OK: {sum(map(len, sites.values()))} truncating casts in {len(sites)} files, all allowed')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
