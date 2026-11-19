"""The `peaks` command of lp64_play.py (LP64-005 Task 13): each pool's largest use over run folders.

A 64-bit build prints `ge: lp64: peak POOL USED of SIZE bytes` for each pool it used, at each stage
unload and at exit (port/src/port_lp64.c, portLp64PoolPeaks). This reads every DIR/*.log of the
folders given and prints, for each pool, the peak that was the largest share of its pool's size,
with the case that had it:

    POOL: USED of SIZE bytes (SHARE%) in DIR/CASE

followed by ` ABOVE 80%` past decision 10's line (scripts/port/native_64bit_lp64_005.md) and by
`; N exhaustion lines` when any log printed `ge: lp64: pool "POOL" exhausted` for that pool (a pool
that ran dry in a run that printed no peak for it is listed with its lines alone). The logs are
ROM-derived runs; only the figures belong in the record.
"""
from dataclasses import dataclass
from pathlib import Path
import re
import sys

PEAK = re.compile(r'ge: lp64: peak (.+) (\d+) of (\d+) bytes$')
EXHAUSTED = re.compile(r'ge: lp64: pool "([^"]+)" exhausted')
RAISE_SHARE = 0.8  # decision 10: a pool past this share of its size is raised at 64 bits


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


def read_logs(dirs: list[Path]) -> tuple[dict[str, PoolReport], int]:
    """Each pool's report over every DIR/*.log, and the number of logs read."""
    pools: dict[str, PoolReport] = {}
    logs = 0
    for folder in dirs:
        for log in sorted(folder.glob('*.log')):
            logs += 1
            case = f'{folder.name}/{log.stem}'
            for line in log.read_text(errors='replace').splitlines():
                note_line(pools, line, case)
    return pools, logs


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


def peaks_report(dirs: list[Path]) -> int:
    """Prints the table; 2 when no log holds a peak line, else 0."""
    missing = [folder for folder in dirs if not folder.is_dir()]
    if missing:
        print(f'error: not a folder: {missing[0]}', file=sys.stderr)
        return 2
    pools, logs = read_logs(dirs)
    if not any(report.peak for report in pools.values()):
        print(f'error: no `ge: lp64: peak` line in {logs} logs', file=sys.stderr)
        return 2
    for name in sorted(pools):
        print(format_pool(name, pools[name]))
    print(f'{len(pools)} pools over {logs} logs')
    return 0
