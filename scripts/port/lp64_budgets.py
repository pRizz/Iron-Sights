#!/usr/bin/env python3
"""Write the 64-bit pool budgets (scripts/port/lp64_budgets.txt) from run folders' peak lines.

    python3 scripts/port/lp64_budgets.py [--budgets FILE] [--pool NAME]... [--retire POOL:SIZE]...
        [--withhold POOL:LEVEL:SIZE:REASON]... [--write] DIR...

LP64-006 Tasks 11 and 11b wrote the file with scratch scripts; this is their logic as a tool (final
review, I3). It reads FILE's lines (`POOL LEVEL SIZE PEAK CASE[ withheld: REASON]`, `#` comments,
lp64_play_peaks.read_budgets) and the `ge: lp64: peak` lines of every DIR/*.log and DIR/runK/*.log
by stage (lp64_play_peaks.read_stages, as `lp64_play.py peaks --by-stage` reads them), then:

- each pool, level and size of the logs gets a line, keeping the larger of FILE's peak and the
  logs' (with the case that had it); --pool limits this to the pools named;
- a persistent pool (PERSISTENT: the model pool that `texture pool` names at 4,473,152 bytes, and
  the high arena, which only grows) is one pool in every stage, so each level of the file gets its
  line with that pool's largest peak anywhere;
- --retire POOL:SIZE drops FILE's lines of POOL at SIZE (a size the source no longer builds), and
  --withhold POOL:LEVEL:SIZE:REASON marks that line ` withheld: REASON` (a raise the owner decides);
  withheld reasons already in FILE stay.

It refuses (exit 1, nothing written) a line past half its size (decision 8: at 64 bits every pool is
at least twice its peak) that is not withheld, and any exhaustion line; raising a pool is a source
change (port_lp64_budget.c, gun.c, front.h, objecthandler_2.c, music.c), never this tool's. It
prints the new file, or with --write replaces FILE (its comment lines are kept as they are). Exit 2
for unusable input. The logs are ROM-derived runs; only the figures reach the file.
"""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lp64_play_peaks import WITHHELD, Budget, StageReport, read_budgets, read_stages  # noqa: E402

BUDGETS = Path(__file__).with_name('lp64_budgets.txt')
# Pools that are one pool over the whole process, by name and size (lp64_budgets.txt's header).
PERSISTENT = (('texture pool', 0x444140), ('high arena', 64 * 1024 * 1024))

Key = tuple[str, int, int]


def merged(budgets: dict[tuple[str, int], dict[int, Budget]], report: StageReport,
           maybe_pools: set[str] | None) -> dict[Key, Budget]:
    """FILE's lines with the logs' peaks folded in: a key's larger peak wins, its reason stays."""
    rows = {(pool, level, size): budget for (pool, level), sizes in budgets.items()
            for size, budget in sizes.items()}
    for (pool, level, size), peak in report.peaks.items():
        if maybe_pools is not None and pool not in maybe_pools:
            continue
        kept = rows.get((pool, level, size))
        if kept is None or peak.used > kept.peak:
            reason = kept.maybe_withheld if kept is not None else None
            rows[(pool, level, size)] = Budget(size, peak.used, peak.case, reason)
    return rows


def spread_persistent(rows: dict[Key, Budget]) -> None:
    """Gives each level of ROWS the largest line of every persistent pool found in ROWS."""
    levels = {level for (_, level, _) in rows}
    for pool, size in PERSISTENT:
        found = [budget for (name, _, known), budget in rows.items() if (name, known) == (pool, size)]
        if not found:
            continue
        largest = max(found, key=lambda budget: budget.peak)
        for level in levels:
            reason = rows[(pool, level, size)].maybe_withheld if (pool, level, size) in rows else None
            rows[(pool, level, size)] = Budget(size, largest.peak, largest.case, reason)


def parse_key(text: str, parts: int, option: str) -> list[str]:
    """TEXT split at its first PARTS - 1 colons; raises ValueError naming OPTION when it does not fit."""
    fields = text.split(':', parts - 1)
    numbers = fields[1:3] if parts == 4 else fields[1:]
    if len(fields) != parts or not all(number.isdigit() for number in numbers) or not fields[-1].strip():
        raise ValueError(f'{option} {text!r} is not ' + ('POOL:SIZE' if parts == 2 else 'POOL:LEVEL:SIZE:REASON'))
    return fields


def apply_options(rows: dict[Key, Budget], retire: list[str], withhold: list[str]) -> None:
    """Drops the retired pool sizes from ROWS and marks the withheld lines."""
    for text in retire:
        pool, size = parse_key(text, 2, '--retire')
        for key in [key for key in rows if (key[0], key[2]) == (pool, int(size))]:
            del rows[key]
    for text in withhold:
        pool, level, size, reason = parse_key(text, 4, '--withhold')
        key = (pool, int(level), int(size))
        if key not in rows:
            raise ValueError(f'--withhold names {pool} {level} {size}, which neither the file nor the logs hold')
        rows[key] = Budget(rows[key].size, rows[key].peak, rows[key].case, reason.strip())


def problems(rows: dict[Key, Budget], report: StageReport) -> list[str]:
    """Why ROWS cannot be written: a line past half that is not withheld, or an exhaustion line."""
    found = [f'{pool} {level} {size}: peak {budget.peak} ({budget.case}) is past half; raise it in the source '
             'or withhold it' for (pool, level, size), budget in sorted(rows.items())
             if 2 * budget.peak > size and budget.maybe_withheld is None]
    found += [f'{pool} in level {level}: {count} exhaustion lines' for (pool, level), count
              in sorted(report.exhausted.items())]
    return found


def render(header: list[str], rows: dict[Key, Budget]) -> str:
    """The file: HEADER's comment lines, then one line a pool, level and size in that order."""
    lines = [f'{pool} {level} {size} {budget.peak} {budget.case}'
             + (f'{WITHHELD}{budget.maybe_withheld}' if budget.maybe_withheld else '')
             for (pool, level, size), budget in sorted(rows.items())]
    return '\n'.join(header + lines) + '\n'


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--budgets', type=Path, default=BUDGETS, metavar='FILE')
    parser.add_argument('--pool', action='append', metavar='NAME', help='fold in only these pools\' peaks')
    parser.add_argument('--retire', action='append', default=[], metavar='POOL:SIZE')
    parser.add_argument('--withhold', action='append', default=[], metavar='POOL:LEVEL:SIZE:REASON')
    parser.add_argument('--write', action='store_true', help='replace FILE instead of printing the new one')
    parser.add_argument('dirs', type=Path, nargs='+', metavar='DIR')
    args = parser.parse_args(argv)
    missing = [folder for folder in args.dirs if not folder.is_dir()]
    if missing:
        print(f'error: not a folder: {missing[0]}', file=sys.stderr)
        return 2
    try:
        text = args.budgets.read_text()
        budgets = read_budgets(args.budgets)
        report = read_stages(args.dirs)
        rows = merged(budgets, report, set(args.pool) if args.pool else None)
        apply_options(rows, args.retire, args.withhold)
    except (OSError, ValueError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 2
    spread_persistent(rows)
    found = problems(rows, report)
    for line in found:
        print(f'FAIL: {line}', file=sys.stderr)
    if found:
        return 1
    output = render([line for line in text.splitlines() if line.startswith('#')], rows)
    if not args.write:
        print(output, end='')
        return 0
    args.budgets.write_text(output)
    print(f'{args.budgets}: {len(rows)} lines from {report.logs} logs', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
