#!/usr/bin/env python3
"""Run named sets of 64-bit game runs and judge each by what its log shows (LP64-005 Task 1).

    python3 scripts/port/lp64_play.py run --rom ROM --set NAME[,NAME...] [--jobs N]
        [--only CASE[,CASE...]] [--env NAME=VALUE]... --output DIR EXECUTABLE
    python3 scripts/port/lp64_play.py traces --expect FILE DIR_REF DIR_NEW
    python3 scripts/port/lp64_play.py traces --reference-check DIR_A DIR_B
    python3 scripts/port/lp64_play.py callers --tick T LOG EXECUTABLE

run: each case of the named sets (lp64_play_cases.py: demos, stages, stops, menus, tours,
attract) on a copy of EXECUTABLE in a fresh temporary folder (lp64_play_run.py), JOBS at a time
(7 by default). Prints `CASE: PASS` or `CASE: FAIL (first failing or missing line)` as each ends,
then `OK: N of N cases` (exit 0) or `FAIL: M of N cases` (exit 1). Logs go to DIR/CASE.log, the
outcomes to DIR/play.json. A case fails on a status other than its own (0, or 1 for a stop), a
timeout, a sanitizer marker, `ge: fatal signal`, `ASSERTION FAULT`, a failing `ge: lp64:` line
or a missing required line (lp64_play_judge.py). Runs are paced in real time: never set
GE_FIXEDTICK or GE_NOSLEEP, which --env refuses.

traces: the demos' `ge: demo tick` lines of two run folders, tick by tick
(lp64_play_traces.py): against lp64_demo_traces.txt with --expect, or two runs of one build with
--reference-check, which prints `unstable` for a demo that differs from itself.

callers: the `ge: rng draw` lines of a GE_RNGTRACE=T log as `tick function file:line`, through
addr2line (lp64_play_callers.py).
"""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

from lp64_play_callers import callers  # noqa: E402
from lp64_play_cases import cases_of  # noqa: E402
from lp64_play_run import FORBIDDEN_VARIABLES, run_cases  # noqa: E402
from lp64_play_traces import compare_with_expectations, reference_check  # noqa: E402

DEFAULT_JOBS = 7


def parse_env(parser: argparse.ArgumentParser, pairs: list[str]) -> dict[str, str]:
    extra = {}
    for pair in pairs:
        name, equals, value = pair.partition('=')
        if not equals or not name:
            parser.error(f'--env takes NAME=VALUE, not {pair!r}')
        if name in FORBIDDEN_VARIABLES:
            parser.error(f'--env {name} is refused: play runs are paced in real time '
                         '(GE_FIXEDTICK changes a demo\'s pace, GE_NOSLEEP passes the frame limit at once)')
        extra[name] = value
    return extra


def command_run(parser: argparse.ArgumentParser, options: argparse.Namespace) -> int:
    extra = parse_env(parser, options.env)
    if options.jobs < 1:
        parser.error('--jobs must be at least 1')
    try:
        cases = cases_of([name for name in options.set.split(',') if name])
    except ValueError as error:
        parser.error(str(error))
    if options.only:
        wanted = options.only.split(',')
        unknown = [name for name in wanted if name not in {case.name for case in cases}]
        if unknown:
            parser.error(f'--only names no case of the sets: {", ".join(unknown)}')
        cases = [case for case in cases if case.name in wanted]
    outcomes = run_cases(cases, options.executable.resolve(strict=True), options.rom.resolve(strict=True),
                         extra, options.output, options.jobs)
    failed = [outcome for outcome in outcomes if not outcome.passed]
    if failed:
        print(f'FAIL: {len(failed)} of {len(outcomes)} cases')
        return 1
    print(f'OK: {len(outcomes)} of {len(outcomes)} cases')
    return 0


def command_traces(parser: argparse.ArgumentParser, options: argparse.Namespace) -> int:
    if options.reference_check:
        return reference_check(options.dir_a, options.dir_b)
    try:
        return compare_with_expectations(options.expect, options.dir_a, options.dir_b)
    except ValueError as error:
        print(f'error: {error}', file=sys.stderr)
        return 2


def command_callers(parser: argparse.ArgumentParser, options: argparse.Namespace) -> int:
    try:
        lines = callers(options.log, options.executable, options.tick, options.addr2line)
    except ValueError as error:
        print(f'error: {error}', file=sys.stderr)
        return 2
    print('\n'.join(lines))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    run = sub.add_parser('run', help='run the cases of named sets and judge each')
    run.add_argument('--rom', type=Path, required=True)
    run.add_argument('--set', required=True, metavar='NAME[,NAME...]')
    run.add_argument('--jobs', type=int, default=DEFAULT_JOBS, metavar='N')
    run.add_argument('--only', metavar='CASE[,CASE...]', help='run only these cases of the sets')
    run.add_argument('--env', action='append', default=[], metavar='NAME=VALUE',
                     help='set a variable for every case (never GE_FIXEDTICK or GE_NOSLEEP)')
    run.add_argument('--output', type=Path, required=True, metavar='DIR')
    run.add_argument('executable', type=Path, metavar='EXECUTABLE')
    traces = sub.add_parser('traces', help='compare the demos\' per-tick traces of two run folders')
    mode = traces.add_mutually_exclusive_group(required=True)
    mode.add_argument('--expect', type=Path, metavar='FILE', help='lp64_demo_traces.txt')
    mode.add_argument('--reference-check', action='store_true', help='two runs of one build must agree')
    traces.add_argument('dir_a', type=Path, metavar='DIR_REF')
    traces.add_argument('dir_b', type=Path, metavar='DIR_NEW')
    who = sub.add_parser('callers', help='name the callers of a GE_RNGTRACE log\'s draws')
    who.add_argument('--tick', type=int, required=True, metavar='T')
    who.add_argument('--addr2line', default='addr2line', help='the addr2line to use (default: addr2line)')
    who.add_argument('log', type=Path, metavar='LOG')
    who.add_argument('executable', type=Path, metavar='EXECUTABLE')
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    options = parser.parse_args(argv)
    command = {'run': command_run, 'traces': command_traces, 'callers': command_callers}[options.command]
    return command(parser, options)


if __name__ == '__main__':
    sys.exit(main())
