#!/usr/bin/env python3
"""Run named sets of 64-bit game runs and judge each by what its log shows (LP64-005 Task 1).

    python3 scripts/port/lp64_play.py run --rom ROM --set NAME[,NAME...] [--jobs N]
        [--only CASE[,CASE...]] [--env NAME=VALUE]... [--video [--shots N [--keep-shots]]]
        [--timeout-scale F] [--frames-scale K] --output DIR EXECUTABLE
    python3 scripts/port/lp64_play.py traces --expect FILE DIR_REF DIR_NEW
    python3 scripts/port/lp64_play.py traces --reference-check DIR_A DIR_B
    python3 scripts/port/lp64_play.py callers --tick T LOG EXECUTABLE
    python3 scripts/port/lp64_play.py sound DIR
    python3 scripts/port/lp64_play.py peaks [--by-stage | --budget FILE] DIR...

run: each case of the named sets (lp64_play_cases.py: demos, stages, stops, menus, tours,
attract; lp64_play_input.py: input, whose cases replay a route as pad 0; lp64_play_sound.py: sound,
video only, whose cases record DIR/CASE.wav and fail on a skipped audio frame or a recording that is
not 22,050 Hz stereo, at least 10 s long and peaking at 1,000 or more) on a copy of EXECUTABLE in a fresh temporary folder (lp64_play_run.py), JOBS at a time
(7 by default); --only names a case, or names it without its set's prefix (`dam` for
`stage-dam`) when that selects exactly one case. Prints `CASE: PASS` or `CASE: FAIL (first
failing or missing line)` as each ends, then `OK: N of N cases` (exit 0) or `FAIL: M of N cases` (exit 1). Logs go to DIR/CASE.log, the
outcomes to DIR/play.json. A case fails on a status other than its own (0, or 1 for a stop), a
timeout, a sanitizer marker, `ge: fatal signal`, `ASSERTION FAULT`, a failing `ge: lp64:` line
or a missing required line (lp64_play_judge.py). Runs are paced in real time: never set
GE_FIXEDTICK or GE_NOSLEEP, which --env refuses, as it refuses what the tool sets for every case
(GE_ROM, GE_FRAMES, GE_SEED, GE_NOINISAVE, GE_INPUTPLAY). A case marked lockstep=True runs with
GE_LOCKSTEP=1 instead (one retrace a game frame, LP64-006); --env refuses GE_LOCKSTEP, and a demo
never runs in lockstep.
--video runs a GE_VIDEO executable offscreen (SDL_VIDEODRIVER=offscreen; on Windows a real window, on
macOS a real Cocoa window that is never shown, GE_WINDOW_HIDDEN=1; SDL_AUDIODRIVER=dummy,
GE_WINDOW=640x480 windowed through a midas.ini in the case's folder, GE_NOINPUT=1, HOME and
GE_ICON_CACHE_DIR in the case's folder, and GE_TICKHZ=60, so video runs count retraces at 60 a
second as headless ones do, not at the shipped default of 120 (at 60 the pump waits for each
picture, so a renderer slower than 60 a second stretches a case's wall time); --env refuses these
names with --video, and GE_SHOT with --shots); every case must then log `ge: video: window + GL context up` and draw at least its
`pictures` (30), read from the last `gfxtasks=` count of its frame-limit line or of the once-a-second
heartbeat (`ge: frame N | gfx tasks submitted M`) for a case that ends itself; a stop needs only the
GL line. --shots N adds GE_SHOT=N and keeps each case's last screenshot as
DIR/CASE.bmp (ROM-derived: never commit it). --timeout-scale F multiplies every case's wall-clock
limit (and its route's recording runs') by F >= 1; use it on a slow host, such as the emulated
i686 container running the 32-bit video reference, where a case needs many times its frames' time.
With --video each verdict names the pictures the case drew (`CASE: PASS (N pictures)`), and
play.json records them. --keep-shots also keeps every screenshot of a case in DIR/CASE.shots/.
--frames-scale K runs each case for K times its frames and requires its frame-limit line at that
count (the sanitizer video runs triple them, so a slow renderer still draws each case's pictures);
the wall-clock limit follows the frames, and ticks, routes and the other required lines do not change.

traces: the demos' `ge: demo tick` lines of two run folders, tick by tick
(lp64_play_traces.py): against lp64_demo_traces.txt with --expect, or two runs of one build with
--reference-check, which prints `unstable` for a demo that differs from itself.

callers: the `ge: rng draw` lines of a GE_RNGTRACE=T log as `tick function file:line`, through
addr2line (lp64_play_callers.py).

sound: each recording in DIR (its *.wav files) as `CASE: SECONDS s, peak PEAK, non-zero SHARE%`, to
compare a run's recordings with the 32-bit reference's (lp64_play_sound.py).

peaks: the largest `ge: lp64: peak` of each pool over every DIR/*.log and DIR/runK/*.log, as
`POOL: USED of SIZE bytes (SHARE%) in DIR/CASE`, marked past 80% of its size and with any exhaustion
lines the pool printed (lp64_play_peaks.py). --by-stage keeps them by the level whose background
loaded since the print before (0, the front end, when none did), one line a pool, level and size:
`POOL LEVEL SIZE PEAK DIR/CASE`, marked ` ABOVE HALF` past half its size. --budget FILE
(scripts/port/lp64_budgets.txt, lines of that form) exits 1, naming the pool and level, when a pool,
level and size of the logs is not in FILE, when its peak is above half its size (unless FILE's line
ends ` withheld: REASON`, which is printed), or when it printed an exhaustion line; else it prints
`OK: N pool and stage pairs within budget`.
"""
import argparse
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

from lp64_play_callers import callers  # noqa: E402
from lp64_play_cases import VIDEO_SETS, cases_of, scale_frames, select_cases  # noqa: E402
from lp64_play_peaks import budget_report, by_stage_report, peaks_report  # noqa: E402
from lp64_play_run import (CAPTURE_VARIABLE, FOLDER_VARIABLES, FORBIDDEN_VARIABLES, LOCKSTEP_VARIABLE,  # noqa: E402
                           TOOL_VARIABLES, VIDEO_VARIABLES, Display, run_cases)
from lp64_play_sound import sound_report  # noqa: E402
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
        if name == LOCKSTEP_VARIABLE:
            parser.error(f'--env {name} is refused: a case marked lockstep=True sets it, and a demo never runs '
                         'in lockstep')
        if name in TOOL_VARIABLES:
            parser.error(f'--env {name} is refused: the tool sets it for every case')
        extra[name] = value
    return extra


def command_run(parser: argparse.ArgumentParser, options: argparse.Namespace) -> int:
    extra = parse_env(parser, options.env)
    if options.jobs < 1:
        parser.error('--jobs must be at least 1')
    if options.shots is not None and not options.video:
        parser.error('--shots needs --video: a headless build draws no picture')
    if options.shots is not None and options.shots < 1:
        parser.error('--shots must be at least 1')
    if options.keep_shots and options.shots is None:
        parser.error('--keep-shots needs --shots: it keeps the screenshots --shots takes')
    frames_scale = int(options.frames_scale) if options.frames_scale.isdigit() else 0
    if frames_scale < 1:
        parser.error(f'--frames-scale takes a whole number of at least 1, not {options.frames_scale!r}')
    try:
        timeout_scale = float(options.timeout_scale)
    except ValueError:
        timeout_scale = math.nan
    if not (math.isfinite(timeout_scale) and timeout_scale >= 1):
        parser.error(f'--timeout-scale takes a number of at least 1, not {options.timeout_scale!r}')
    if options.video:
        for name in extra:
            if name in VIDEO_VARIABLES or name in FOLDER_VARIABLES:
                parser.error(f'--env {name} is refused with --video: the tool sets it for every case')
    if options.shots is not None and 'GE_SHOT' in extra:
        parser.error('--env GE_SHOT is refused with --shots: --shots sets it')
    set_names = [name for name in options.set.split(',') if name]
    video_only = [name for name in set_names if name in VIDEO_SETS]
    if video_only and not options.video:
        parser.error(f'the {", ".join(video_only)} set needs --video: a headless build stubs the audio pump')
    try:
        cases = cases_of(set_names)
    except ValueError as error:
        parser.error(str(error))
    if options.only is not None:
        wanted = [name for name in options.only.split(',') if name]
        if not wanted:
            parser.error(f'--only names no case: {options.only!r}')
        try:
            cases = select_cases(cases, wanted)
        except ValueError as error:
            parser.error(str(error))
    cases = [scale_frames(case, frames_scale) for case in cases]
    if CAPTURE_VARIABLE in extra:
        parser.error(f'--env {CAPTURE_VARIABLE} is refused: parallel cases would write one file, and each'
                     ' recording case records to DIR/CASE.wav')
    outcomes = run_cases(cases, options.executable.resolve(strict=True), options.rom.resolve(strict=True),
                         extra, options.output, options.jobs,
                         Display(options.video, options.shots, timeout_scale, options.keep_shots), frames_scale)
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


def command_sound(parser: argparse.ArgumentParser, options: argparse.Namespace) -> int:
    return sound_report(options.dir)


def command_peaks(parser: argparse.ArgumentParser, options: argparse.Namespace) -> int:
    if options.budget is not None:
        return budget_report(options.budget, options.dirs)
    return by_stage_report(options.dirs) if options.by_stage else peaks_report(options.dirs)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    run = sub.add_parser('run', help='run the cases of named sets and judge each')
    run.add_argument('--rom', type=Path, required=True)
    run.add_argument('--set', required=True, metavar='NAME[,NAME...]')
    run.add_argument('--jobs', type=int, default=DEFAULT_JOBS, metavar='N')
    run.add_argument('--only', metavar='CASE[,CASE...]', help='run only these cases of the sets')
    run.add_argument('--env', action='append', default=[], metavar='NAME=VALUE',
                     help='set a variable for every case (never GE_FIXEDTICK, GE_NOSLEEP or GE_LOCKSTEP)')
    run.add_argument('--video', action='store_true',
                     help='run a GE_VIDEO executable offscreen (on macOS in a hidden window); each case must draw '
                          'its pictures')
    run.add_argument('--shots', type=int, metavar='N',
                     help='with --video: a screenshot every N pictures drawn, the last kept as DIR/CASE.bmp')
    run.add_argument('--timeout-scale', default='1', metavar='F',
                     help='multiply every case\'s wall-clock limit by F (at least 1; default 1), for a slow '
                          'host such as an emulated container running a video build; frames, required '
                          'lines and pictures do not change')
    run.add_argument('--keep-shots', action='store_true',
                     help='with --shots: keep every screenshot of a case in DIR/CASE.shots/, not only the last')
    run.add_argument('--frames-scale', default='1', metavar='K',
                     help='run each case for K times its frames (a whole number, at least 1; default 1), '
                          'requiring its frame-limit line at that count; ticks and routes do not change')
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
    sound = sub.add_parser('sound', help='the length, peak and non-zero share of each recording in a run folder')
    sound.add_argument('dir', type=Path, metavar='DIR')
    peaks = sub.add_parser('peaks', help='the largest peak of each pool over run folders')
    stages = peaks.add_mutually_exclusive_group()
    stages.add_argument('--by-stage', action='store_true', help='each pool\'s peak by level and size')
    stages.add_argument('--budget', type=Path, metavar='FILE',
                        help='fail on a pool, level and size not in FILE or past half its size')
    peaks.add_argument('dirs', type=Path, nargs='+', metavar='DIR')
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    options = parser.parse_args(argv)
    command = {'run': command_run, 'traces': command_traces, 'callers': command_callers,
               'sound': command_sound, 'peaks': command_peaks}[options.command]
    return command(parser, options)


if __name__ == '__main__':
    sys.exit(main())
