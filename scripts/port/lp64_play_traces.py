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
build: a demo that differs from itself is `unstable`, and its trace proves nothing. Both refuse a
demo whose log, on either side, does not show its playback reaching its end by the `demos` case's
rule (its own first playback, ended by its time at the measured tick): two runs that stop at the
same early tick would otherwise agree.

The tick lines go to stdout and other lines to stderr, both into the log, so a line of the other
stream can cut a tick line in two (seen once: facility1 at tick 331 in an i686 run). Such a torn
line is neither a difference nor agreement: the comparison names it, `torn line at tick T`, and
the demo fails.
"""
from dataclasses import dataclass
from pathlib import Path
import re

from lp64_play_cases import DEMOS, demo_playback_required
from lp64_play_judge import requirement_problem

# A whole tick line, field by field as ramromreplay.c prints it.
TICK_LINE = re.compile(r'^ge: demo tick (\d+) (draws=-?\d+ live=[0-9a-f]{2} rec=[0-9a-f]{2} '
                       r'p0=-?\d+,-?\d+ p1=-?\d+,-?\d+ b=[0-9a-f]{4},[0-9a-f]{4})$')
# A part of one: a cut after "demo tick" leaves it in the first part, and a cut before it leaves
# " p1=" in the second (no other line the game prints holds either).
TICK_PART = re.compile(r'demo tick| p1=')
TICK_NUMBER = re.compile(r'tick (\d+)')


@dataclass(frozen=True)
class Trace:
    """The (tick, rest of line) pairs of a log's first playback, in order, up to the first torn
    tick line; TORN is the tick that line belongs to, or None when there is none."""
    ticks: list[tuple[int, str]]
    torn: int | None = None


def read_trace(text: str) -> Trace:
    """TEXT's first playback, which ends at the first whole tick line whose number is not past the
    last one's (the next playback's tick 1). A part of a tick line that is not a whole one is torn
    wherever it falls before that line, including after the first playback's exit line and in the
    next playback's tick-1 line itself: the first playback is then reported torn, which fails safe.
    Its tick is the number it holds when that is past the last whole tick, else the tick after it
    (a cut inside the number leaves a smaller one)."""
    ticks: list[tuple[int, str]] = []
    for line in text.splitlines():
        match = TICK_LINE.match(line.strip())
        if match is None:
            if TICK_PART.search(line) is None:
                continue
            last = ticks[-1][0] if ticks else -1
            number = TICK_NUMBER.search(line)
            return Trace(ticks, max(int(number.group(1)), last + 1) if number else last + 1)
        tick = int(match.group(1))
        if ticks and tick <= ticks[-1][0]:
            break
        ticks.append((tick, match.group(2)))
    return Trace(ticks)


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


def playback_problem(name: str, text: str) -> str | None:
    """Why TEXT does not show NAME's first playback reaching its end, or None when it does."""
    lines = text.splitlines()
    return next((problem for problem in (requirement_problem(lines, required)
                                         for required in demo_playback_required(name)) if problem), None)


def compare_demo(name: str, dir_a: Path, dir_b: Path) -> Comparison:
    traces = []
    for folder in (dir_a, dir_b):
        log = folder/f'{name}.log'
        if not log.is_file():
            return Comparison(0, None, f'no log {log}')
        text = log.read_text(errors='replace')
        trace = read_trace(text)
        if trace.torn is not None:
            return Comparison(0, None, f'torn line at tick {trace.torn} in {log} (another stream cut it)')
        ticks = trace.ticks
        if not ticks:
            return Comparison(0, None, f'no "ge: demo tick" lines in {log}')
        unfinished = playback_problem(name, text)
        if unfinished is not None:
            return Comparison(0, None, f'refused: {unfinished}; last tick {ticks[-1][0]} in {log}')
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
