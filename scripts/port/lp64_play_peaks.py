"""The `peaks` command of lp64_play.py (LP64-005 Task 13): each pool's largest use over run folders.

A 64-bit build prints `ge: lp64: peak POOL USED of SIZE bytes` for each pool it used, at each stage
unload and at exit (port/src/port_lp64.c, portLp64PoolPeaks). This reads every DIR/*.log and
DIR/runK/*.log (an lp64_equiv.py folder keeps each repeat in runK/) of the folders given and prints,
for each pool, the peak that was the largest share of its pool's size, with the case that had it:

    POOL: USED of SIZE bytes (SHARE%) in DIR/CASE

followed by ` ABOVE 80%` past decision 10's line (scripts/port/native_64bit_lp64_005.md) and by
`; N exhaustion lines` when any log printed `ge: lp64: pool "POOL" exhausted` for that pool (a pool
that ran dry in a run that printed no peak for it is listed with its lines alone).

`--by-stage` (LP64-006 Task 11) keeps the peaks by stage, since a stage's token gives it its own
graphics buffer, vertex pool, texture pool and room memory (src/boss.c, memallocstringtable). A
print belongs to the level of the `ge: load_bg_file levelid=` line logged since the print before
it, and to level 0, the front end, when no background loaded in between: the front end's pools are
printed at the next stage's unload, with no load of their own. One stage gives a pool several sizes
(a multiplayer token, the vertex pool by players, a name several pools share), so a line is a pool,
level and size: `POOL LEVEL SIZE PEAK DIR/CASE`, the form of scripts/port/lp64_budgets.txt, marked
` ABOVE HALF` past decision 8's line.

`--budget FILE` checks the logs against FILE's lines (`#` starts a comment): it exits 1, naming the
pool and level, when a pool, level and size of the logs is not in FILE (a size other than FILE's for
a listed pool and level is named as such), when its peak is above half its size, or when the pool
printed an exhaustion line in that level. A line ending ` withheld: REASON` accepts its pair past
half by name (Task 11's withheld raises); it is printed, never passed silently. The logs are
ROM-derived runs; only the figures belong in the record.
"""
from dataclasses import dataclass, field
from pathlib import Path
import re
import sys

PEAK = re.compile(r'ge: lp64: peak (.+) (\d+) of (\d+) bytes$')
EXHAUSTED = re.compile(r'ge: lp64: pool "([^"]+)" exhausted')
STAGE = re.compile(r'ge: load_bg_file levelid=(\d+) ')
RAISE_SHARE = 0.8  # decision 10: a pool past this share of its size is raised at 64 bits
FRONT_END = 0
WITHHELD = ' withheld: '


@dataclass
class Peak:
    """One pool's largest share of its size, and where it was seen."""
    used: int
    size: int
    case: str

    def share(self) -> float:
        return self.used / self.size if self.size else 0.0


@dataclass
class PoolReport:
    """What the logs say of one pool."""
    peak: Peak | None = None
    exhausted: int = 0


@dataclass
class StageReport:
    """What the logs say of the pools of each level: the largest use of each pool, level and size,
    and each pool and level's exhaustion lines."""
    peaks: dict[tuple[str, int, int], Peak] = field(default_factory=dict)
    exhausted: dict[tuple[str, int], int] = field(default_factory=dict)
    logs: int = 0


@dataclass(frozen=True)
class Budget:
    """One line of a budget file."""
    size: int
    peak: int
    case: str
    maybe_withheld: str | None


def log_files(dirs: list[Path]) -> list[tuple[str, Path]]:
    """Each log of DIRS with its case name: DIR/CASE, or DIR/runK/CASE for a repeat's folder."""
    found = []
    for folder in dirs:
        for log in sorted(folder.glob('*.log')):
            found.append((f'{folder.name}/{log.stem}', log))
        for run in sorted(path for path in folder.glob('run*') if path.is_dir()):
            for log in sorted(run.glob('*.log')):
                found.append((f'{folder.name}/{run.name}/{log.stem}', log))
    return found


def read_logs(dirs: list[Path]) -> tuple[dict[str, PoolReport], int]:
    """Each pool's report over every log of DIRS, and the number of logs read."""
    pools: dict[str, PoolReport] = {}
    logs = log_files(dirs)
    for case, log in logs:
        for line in log.read_text(errors='replace').splitlines():
            note_line(pools, line, case)
    return pools, len(logs)


def note_line(pools: dict[str, PoolReport], line: str, case: str) -> None:
    """Adds LINE of CASE's log to POOLS if it is a peak or an exhaustion line."""
    at = line.find('ge: lp64: ')
    if at < 0:
        return
    text = line[at:]
    found = PEAK.match(text)
    if found:
        candidate = Peak(int(found.group(2)), int(found.group(3)), case)
        report = pools.setdefault(found.group(1), PoolReport())
        if report.peak is None or candidate.used * report.peak.size > report.peak.used * candidate.size:
            report.peak = candidate
        return
    found = EXHAUSTED.match(text)
    if found:
        pools.setdefault(found.group(1), PoolReport()).exhausted += 1


def read_stages(dirs: list[Path]) -> StageReport:
    """The peaks of every log of DIRS by pool, level and size."""
    logs = log_files(dirs)
    report = StageReport(logs=len(logs))
    for case, log in logs:
        note_stages(report, log.read_text(errors='replace').splitlines(), case)
    return report


def note_stages(report: StageReport, lines: list[str], case: str) -> None:
    """Adds CASE's log LINES to REPORT, each print in the level loaded since the print before it."""
    level, printing = FRONT_END, False
    for line in lines:
        found = PEAK.search(line)
        if found:
            printing = True
            key = (found.group(1), level, int(found.group(3)))
            kept = report.peaks.get(key)
            if kept is None or int(found.group(2)) > kept.used:
                report.peaks[key] = Peak(int(found.group(2)), key[2], case)
            continue
        if printing:
            level, printing = FRONT_END, False
        found = STAGE.search(line)
        if found:
            level = int(found.group(1))
            continue
        found = EXHAUSTED.search(line)
        if found:
            key = (found.group(1), level)
            report.exhausted[key] = report.exhausted.get(key, 0) + 1


def above_half(peak: Peak) -> bool:
    """Decision 8: a pool's size must be at least twice its peak."""
    return 2 * peak.used > peak.size


def format_pool(name: str, report: PoolReport) -> str:
    """One line of the table."""
    if report.peak is None:
        line = f'{name}: no peak line'
    else:
        peak = report.peak
        line = f'{name}: {peak.used} of {peak.size} bytes ({100 * peak.share():.1f}%) in {peak.case}'
        if peak.share() > RAISE_SHARE:
            line += ' ABOVE 80%'
    if report.exhausted:
        line += f'; {report.exhausted} exhaustion lines'
    return line


def checked_folders(dirs: list[Path]) -> bool:
    """False, after saying so, when one of DIRS is not a folder."""
    missing = [folder for folder in dirs if not folder.is_dir()]
    if missing:
        print(f'error: not a folder: {missing[0]}', file=sys.stderr)
    return not missing


def peaks_report(dirs: list[Path]) -> int:
    """Prints the table; 2 when no log holds a peak line, else 0."""
    if not checked_folders(dirs):
        return 2
    pools, logs = read_logs(dirs)
    if not any(report.peak for report in pools.values()):
        print(f'error: no `ge: lp64: peak` line in {logs} logs', file=sys.stderr)
        return 2
    for name in sorted(pools):
        print(format_pool(name, pools[name]))
    print(f'{len(pools)} pools over {logs} logs')
    return 0


def stage_report(dirs: list[Path]) -> StageReport | None:
    """The by-stage report of DIRS, or None after saying why there is none."""
    if not checked_folders(dirs):
        return None
    report = read_stages(dirs)
    if not report.peaks:
        print(f'error: no `ge: lp64: peak` line in {report.logs} logs', file=sys.stderr)
        return None
    return report


def by_stage_report(dirs: list[Path]) -> int:
    """Prints one line a pool, level and size; 2 when no log holds a peak line, else 0."""
    report = stage_report(dirs)
    if report is None:
        return 2
    for (pool, level, size), peak in sorted(report.peaks.items()):
        line = f'{pool} {level} {size} {peak.used} {peak.case}'
        print(line + (' ABOVE HALF' if above_half(peak) else ''))
    for (pool, level), count in sorted(report.exhausted.items()):
        print(f'{pool} {level}: {count} exhaustion lines')
    print(f'{len(report.peaks)} pool and stage pairs over {report.logs} logs')
    return 0


def read_budgets(path: Path) -> dict[tuple[str, int], dict[int, Budget]]:
    """FILE's lines by pool and level, then size. Raises ValueError on a line that is not
    `POOL LEVEL SIZE PEAK CASE[ withheld: REASON]`."""
    budgets: dict[tuple[str, int], dict[int, Budget]] = {}
    for number, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        line, _, reason = line.partition(WITHHELD)
        fields = line.rsplit(None, 4)
        if len(fields) != 5 or not all(text.isdigit() for text in fields[1:4]):
            raise ValueError(f'{path}:{number}: not POOL LEVEL SIZE PEAK CASE: {raw}')
        pool, level, size, peak, case = fields
        budgets.setdefault((pool, int(level)), {})[int(size)] = Budget(
            int(size), int(peak), case, reason.strip() or None)
    return budgets


def budget_problems(report: StageReport, budgets: dict[tuple[str, int], dict[int, Budget]],
                    path: Path) -> tuple[list[str], list[str]]:
    """What the logs break of the budgets, and the withheld pairs they exercise."""
    failures, withheld = [], []
    for (pool, level, size), peak in sorted(report.peaks.items()):
        listed = budgets.get((pool, level), {})
        budget = listed.get(size)
        if budget is None:
            sizes = ', '.join(str(known) for known in sorted(listed))
            failures.append(f'{pool} in level {level}: {size} bytes, but the budget lists {sizes}' if listed
                            else f'{pool} in level {level}: {size} bytes, not in {path}')
            continue
        if not above_half(peak):
            continue
        figure = f'{pool} in level {level}: peak {peak.used} of {size} bytes'
        if budget.maybe_withheld is None:
            failures.append(f'{figure} is above half its size ({peak.case})')
        else:
            withheld.append(f'{figure} ({budget.maybe_withheld})')
    for (pool, level), count in sorted(report.exhausted.items()):
        failures.append(f'{pool} in level {level}: {count} exhaustion lines')
    return failures, withheld


def budget_report(budget_file: Path, dirs: list[Path]) -> int:
    """Checks the logs against BUDGET_FILE: 0 when every pair is within it, 1 naming each that is
    not, 2 when the file or the logs cannot be read."""
    report = stage_report(dirs)
    if report is None:
        return 2
    try:
        budgets = read_budgets(budget_file)
    except (OSError, ValueError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 2
    failures, withheld = budget_problems(report, budgets, budget_file)
    for line in withheld:
        print(f'withheld: {line}')
    for line in failures:
        print(f'FAIL: {line}')
    if failures:
        print(f'FAIL: {len(failures)} of {len(report.peaks)} pool and stage pairs over {report.logs} logs')
        return 1
    note = f'; {len(withheld)} withheld, named above' if withheld else ''
    print(f'OK: {len(report.peaks)} pool and stage pairs within budget{note}')
    return 0
