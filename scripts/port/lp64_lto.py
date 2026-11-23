"""List the link-time type mismatches of a `-flto -Wlto-type-mismatch` build log and hold them to a list.

A declaration that disagrees with its definition in another file (a function declared to return
`int` and defined returning `u8`, an `extern long` defined as `int[4]`) compiles in each file on its
own; only the link sees both. At 64 bits a disagreement in size makes the caller read or pass the
wrong bytes. The log comes from one of the scratch trees `build/lp64/lto` (64-bit headless) and
`build/lp64/lto-video` (64-bit video), configured with `-flto=auto -Wlto-type-mismatch` in the C,
C++ and linker flags and linked outside make's jobserver (see AGENTS.md for the commands).

`list LOG...` prints one line per mismatch:
    SYMBOL: FILE_A:LINE DECLARATION_A | FILE_B:LINE DECLARATION_B -- GCC's detail notes
where A is the declaration GCC warns about, B the one it calls original, and each declaration is
the source line GCC quoted. `check LOG...` requires the set of mismatched symbols over all the logs
to equal the symbols of `lp64_lto_allowed.txt` (`SYMBOL reason` a line, `#` comments): exact match,
and a missing file allows none.
"""
import argparse
from pathlib import Path
import re
import sys
from typing import Optional

ALLOWLIST = Path(__file__).with_name('lp64_lto_allowed.txt')
ROOT = Path(__file__).resolve().parents[2].as_posix()
CONTAINER_ROOT = '/home/dev/project'  # the checkout inside lp64_env.py's containers
LOCATION = r'(?P<file>[^\s:][^:]*):(?P<line>\d+):(?P<col>\d+): '
WARNING = re.compile(LOCATION + r"warning: type of '(?P<symbol>[^']+)' does not match original declaration "
                     r'\[-Wlto-type-mismatch\]')
NOTE = re.compile(LOCATION + r'note: (?P<text>.*)$')
QUOTE = re.compile(r'^\s*(?P<line>\d+) \| (?P<text>.*)$')
CARET = re.compile(r'^\s*\| ')
PREVIOUS = re.compile(r"^'(?P<symbol>[^']+)' was previously declared here$")
IGNORED_NOTE = re.compile(r"^code may be misoptimized unless '-fno-strict-aliasing' is used$")
FINISHED_LINK = re.compile(r'Built target ge$')


def relative(path: str, roots: tuple[str, ...]) -> str:
    """Return `path` without the first of `roots` it starts with (unchanged if none)."""
    for root in roots:
        if path.startswith(root.rstrip('/') + '/'):
            return path[len(root.rstrip('/')) + 1:]
    return path


def quoted_lines(lines: list[str], roots: tuple[str, ...]) -> dict[tuple[str, int], str]:
    """Return (file, line) -> the source text GCC quoted under a diagnostic at that place."""
    quotes = {}
    for index, text in enumerate(lines[:-1]):
        match = WARNING.match(text) or NOTE.match(text)
        quote = QUOTE.match(lines[index + 1]) if match else None
        if quote and quote['line'] == match['line']:
            quotes.setdefault((relative(match['file'], roots), int(match['line'])), quote['text'].strip())
    return quotes


def parse_log(text: str, root: Optional[str] = None) -> tuple[dict[tuple, str], dict[tuple[str, int], str]]:
    """Return ((symbol, file A, line A, file B, line B) -> detail notes, the quoted source lines) of a log."""
    roots = (root,) if root else (ROOT, CONTAINER_ROOT)
    lines = text.splitlines()
    blocks = []
    for text_line in lines:
        warning = WARNING.match(text_line)
        if warning:
            blocks.append(dict(symbol=warning['symbol'], a=(relative(warning['file'], roots), int(warning['line'])),
                               b=('', 0), notes=[]))
            continue
        note = NOTE.match(text_line)
        if not note:
            if not (QUOTE.match(text_line) or CARET.match(text_line) or text_line.startswith('  +++ |')):
                blocks.append(None)  # anything else ends the warning's notes
            continue
        block = blocks[-1] if blocks else None
        if block is None:
            continue
        previous = PREVIOUS.match(note['text'])
        if previous and previous['symbol'] == block['symbol']:
            block['b'] = (relative(note['file'], roots), int(note['line']))
        elif not IGNORED_NOTE.match(note['text']) and note['text'] not in block['notes']:
            block['notes'].append(note['text'])
    found = {(block['symbol'], *block['a'], *block['b']): '; '.join(block['notes']) for block in blocks if block}
    return found, quoted_lines(lines, roots)


def format_lines(parsed: tuple[dict[tuple, str], dict[tuple[str, int], str]]) -> list[str]:
    """Return the `list` lines of parsed mismatches, sorted by symbol."""
    found, quotes = parsed
    result = []
    for symbol, file_a, line_a, file_b, line_b in sorted(found):
        detail = found[(symbol, file_a, line_a, file_b, line_b)]
        side_a = f"{file_a}:{line_a} {quotes.get((file_a, line_a), '')}".rstrip()
        side_b = f"{file_b}:{line_b} {quotes.get((file_b, line_b), '')}".rstrip()
        result.append(f'{symbol}: {side_a} | {side_b}' + (f' -- {detail}' if detail else ''))
    return result


def read_logs(paths: list[Path], root: Optional[str] = None) -> tuple[dict, dict]:
    """Parse and merge several logs. A log whose link did not finish measured nothing: raise ValueError."""
    found, quotes = {}, {}
    for path in paths:
        text = path.read_text(errors='replace')
        if not any(FINISHED_LINK.search(line) for line in text.splitlines()):
            raise ValueError(f'{path} shows no finished link (no "Built target ge" line); refusing to report it')
        more_found, more_quotes = parse_log(text, root)
        found.update(more_found)
        for key, value in more_quotes.items():
            quotes.setdefault(key, value)
    return found, quotes


def parse_allowlist(text: str) -> dict[str, str]:
    """Return symbol -> reason from `SYMBOL reason` lines; `#` lines and blanks are skipped."""
    allowed = {}
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip() or line.startswith('#'):
            continue
        symbol, _, reason = line.partition(' ')
        if not reason.strip() or symbol in allowed:
            raise ValueError(f'allowlist line {number} is not a new SYMBOL followed by its reason: {line!r}')
        allowed[symbol] = reason.strip()
    return allowed


def check(paths: list[Path], allowlist: Path, root: Optional[str] = None) -> int:
    """Print each mismatch not allowed and each allowed symbol no longer mismatched; return 1 if any."""
    parsed = read_logs(paths, root)
    lines = format_lines(parsed)
    allowed = parse_allowlist(allowlist.read_text()) if allowlist.exists() else {}
    seen = {symbol for symbol, *_ in parsed[0]}
    problems = 0
    for line in lines:
        if line.split(':', 1)[0] not in allowed:
            problems += 1
            print(f"{line.split(':', 1)[0]}: not allowed\n  {line}")
    for symbol in sorted(set(allowed) - seen):
        problems += 1
        print(f'{symbol}: allowed but no longer mismatched; remove its line')
    if problems:
        print(f'The allowlist ({allowlist.name}) must match exactly. Make the declaration match its '
              'definition instead of adding to it; `lp64_lto.py list LOG...` lists every mismatch.')
        return 1
    print(f'OK: {len(lines)} mismatches, all allowed')
    return 0


def main(argv: list[str]) -> int:
    """Run `list` or `check`; return the process exit status."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    for name, help_text in (('list', 'print each link-time type mismatch of the logs'),
                            ('check', 'compare the logs\' mismatched symbols with the allowlist')):
        command = commands.add_parser(name, help=help_text, description=help_text)
        command.add_argument('logs', nargs='+', type=Path, metavar='LOG')
    command.add_argument('--allowlist', type=Path, default=ALLOWLIST, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        if args.command == 'check':
            return check(args.logs, args.allowlist)
        for line in format_lines(read_logs(args.logs)):
            print(line)
        return 0
    except (OSError, ValueError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
