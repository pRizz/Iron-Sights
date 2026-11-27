#!/usr/bin/env python3
"""Check what a macOS link left to be looked up when the game starts.

The macOS game links with `-Wl,-undefined,dynamic_lookup` (MAC-001, FORK-093): a 64-bit build has
no compiled-in assets, so several hundred weak references (the file symbols of
`assets/obseg/obseg.h`, and the video-only functions a headless build tests for) are undefined.
ELF resolves an undefined weak reference to null at link time; Mach-O needs dynamic lookup to do
the same at load. That option also lets a *strong* undefined symbol through, which an ELF link
would refuse and which would stop the game at its first use. This tool refuses it instead.

`macos_link_check.py EXECUTABLE` runs `nm -m` on it and reads the lines ending
`(dynamically looked up)`. When every one is `weak external` it prints
`OK: N weak references, 0 strong` and exits 0; otherwise it prints each strong name and exits 1.
It exits 2 when `nm` cannot read the file. The build runs it after every link of the game.
"""
import argparse
import subprocess
import sys

LOOKED_UP = '(dynamically looked up)'
WEAK = '(undefined) weak external '
STRONG = '(undefined) external '


def lookup_lines(nm_text: str) -> list[str]:
    """Return the stripped `nm -m` lines of symbols left to dynamic lookup."""
    return [line.strip() for line in nm_text.splitlines() if line.rstrip().endswith(LOOKED_UP)]


def lookup_count(nm_text: str) -> int:
    """Return how many symbols, weak or strong, the link left to dynamic lookup."""
    return len(lookup_lines(nm_text))


def strong_lookups(nm_text: str) -> list[str]:
    """Return the names of the strong symbols left to dynamic lookup, in the text's order.

    A line that is neither `weak external` nor `external` (a form this tool does not know) is
    returned whole, so an unread line can never pass as weak."""
    names = []
    for line in lookup_lines(nm_text):
        if line.startswith(WEAK):
            continue
        if line.startswith(STRONG):
            names.append(line[len(STRONG):-len(LOOKED_UP)].strip())
        else:
            names.append(line)
    return names


def report(nm_text: str) -> int:
    """Print the verdict for `nm_text` and return the exit status (0 all weak, 1 a strong one)."""
    strong = strong_lookups(nm_text)
    if strong:
        print(f'macos_link_check: {len(strong)} strong symbols are undefined and left to dynamic lookup:',
              file=sys.stderr)
        for name in strong:
            print(f'  {name}', file=sys.stderr)
        return 1
    print(f'OK: {lookup_count(nm_text)} weak references, 0 strong')
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('executable', help='the linked macOS game')
    args = parser.parse_args()
    try:
        result = subprocess.run(['nm', '-m', args.executable], capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f'macos_link_check: cannot run nm on {args.executable}: {error}', file=sys.stderr)
        return 2
    if result.returncode != 0:
        print(f'macos_link_check: nm -m {args.executable} failed:\n{result.stderr}', file=sys.stderr)
        return 2
    return report(result.stdout)


if __name__ == '__main__':
    sys.exit(main())
