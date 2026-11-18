"""Compare the demos' per-tick traces of two lp64_play.py runs (LP64-005 Task 1).

With GE_DEMODIAG=1 the game prints, at the end of each demo tick (src/game/ramromreplay.c):
    "ge: demo tick %d draws=%d live=%02x rec=%02x p0=%d,%d p1=%d,%d b=%04x,%04x\n"
the random draws of the tick, the seed the game has against the one the N64 recorded, and the
pads. Two runs are compared tick by tick on the text after the tick number, over the first
playback of the log (the attract loop may start another demo, whose ticks count from 1 again).

`compare_with_expectations` checks a reference run (the i686 build) and a new one against
lp64_demo_traces.txt: one line a demo, `NAME identical` or `NAME differs-from T REASON`. A demo
passes only if it is exactly as its line says, so a demo recorded as differing that now matches
fails too and the file is lowered when a fix lands. `reference_check` compares two runs of one
build: a demo that differs from itself is `unstable`, and its trace proves nothing.
"""
from dataclasses import dataclass
from pathlib import Path
import re

from lp64_play_cases import DEMOS

TICK_LINE = re.compile(r'^ge: demo tick (\d+) (.*)$')


def read_ticks(text: str) -> list[tuple[int, str]]:
    """The (tick, rest of line) pairs of TEXT's first playback, in order."""
    ticks: list[tuple[int, str]] = []
    for line in text.splitlines():
        match = TICK_LINE.match(line.strip())
        if match is None:
            continue
        tick = int(match.group(1))
        if ticks and tick <= ticks[-1][0]:
            break
        ticks.append((tick, match.group(2)))
    return ticks


@dataclass(frozen=True)
class Comparison:
    """How two traces of one demo compare: FIRST_DIFFERENCE is None when they are identical over
    TICKS ticks; PROBLEM is set when a trace is missing or empty."""
    ticks: int
    first_difference: int | None
    problem: str | None = None


def compare_ticks(reference: list[tuple[int, str]], new: list[tuple[int, str]]) -> Comparison:
    """The first tick at which the two traces differ; a tick one trace has and the other lacks
    is a difference at that tick."""
    for (ref_tick, ref_rest), (new_tick, new_rest) in zip(reference, new):
        if ref_tick != new_tick:
            return Comparison(0, min(ref_tick, new_tick))
        if ref_rest != new_rest:
            return Comparison(0, ref_tick)
    if len(reference) != len(new):
        shorter = min(len(reference), len(new))
        longer = reference if len(reference) > len(new) else new
        return Comparison(0, longer[shorter][0])
    return Comparison(len(reference), None)


def compare_demo(name: str, dir_a: Path, dir_b: Path) -> Comparison:
    traces = []
    for folder in (dir_a, dir_b):
        log = folder/f'{name}.log'
        if not log.is_file():
            return Comparison(0, None, f'no log {log}')
        ticks = read_ticks(log.read_text(errors='replace'))
        if not ticks:
            return Comparison(0, None, f'no "ge: demo tick" lines in {log}')
        traces.append(ticks)
    return compare_ticks(traces[0], traces[1])


def describe(name: str, comparison: Comparison) -> str:
    if comparison.problem is not None:
        return f'{name}: {comparison.problem}'
    if comparison.first_difference is None:
        return f'{name}: identical ({comparison.ticks} ticks)'
    return f'{name}: first differs at tick {comparison.first_difference}'


def read_expectations(path: Path) -> dict[str, int | None]:
    """lp64_demo_traces.txt: for each demo, None (identical) or the tick it first differs at.
    Blank lines and lines starting with # are skipped. Raises ValueError on a malformed line, an
    unknown or repeated demo, or a demo without a line."""
    expected: dict[str, int | None] = {}
    for number, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        words = line.split()
        if words[0] not in DEMOS or words[0] in expected:
            raise ValueError(f'{path}:{number}: unknown or repeated demo: {line}')
        if words[1:] == ['identical']:
            expected[words[0]] = None
        elif len(words) >= 4 and words[1] == 'differs-from' and words[2].isdigit():
            expected[words[0]] = int(words[2])
        else:
            raise ValueError(f'{path}:{number}: expected "NAME identical" or "NAME differs-from T REASON": {line}')
    missing = [name for name in DEMOS if name not in expected]
    if missing:
        raise ValueError(f'{path}: no line for {", ".join(missing)}')
    return expected


def compare_with_expectations(expect_file: Path, dir_ref: Path, dir_new: Path) -> int:
    """Prints each demo's comparison and its agreement with EXPECT_FILE; returns the exit status."""
    expected = read_expectations(expect_file)
    failed, identical = [], 0
    for name in DEMOS:
        comparison = compare_demo(name, dir_ref, dir_new)
        print(describe(name, comparison))
        if comparison.problem is None and comparison.first_difference == expected[name]:
            identical += comparison.first_difference is None
            continue
        want = 'identical' if expected[name] is None else f'differs-from {expected[name]}'
        print(f'  not as recorded: {expect_file.name} has "{name} {want}"')
        failed.append(name)
    if failed:
        print(f'FAIL: {len(failed)} of {len(DEMOS)} demos not as recorded: {", ".join(failed)}')
        return 1
    print(f'OK: {len(DEMOS)} demos, {identical} identical, {len(DEMOS) - identical} differing as recorded')
    return 0


def reference_check(dir_a: Path, dir_b: Path) -> int:
    """Prints whether each demo's two runs agree; returns 1 if any is unstable or missing."""
    failed = []
    for name in DEMOS:
        comparison = compare_demo(name, dir_a, dir_b)
        if comparison.problem is None and comparison.first_difference is not None:
            print(f'{name}: unstable (first differs at tick {comparison.first_difference})')
            failed.append(name)
            continue
        print(describe(name, comparison))
        if comparison.problem is not None:
            failed.append(name)
    if failed:
        print(f'FAIL: {len(failed)} of {len(DEMOS)} demos unstable or missing: {", ".join(failed)}')
        return 1
    print(f'OK: {len(DEMOS)} demos agree with themselves')
    return 0
