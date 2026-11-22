#!/usr/bin/env python3
"""Run LP64-006's 38 cases on a comparison build and compare the streams they write (Task 5).

    python3 scripts/port/lp64_equiv.py run --rom ROM --set NAME[,NAME...] [--profile default|all]
        [--video [--draw] [--shots TICK]] [--repeat N] [--jobs N] [--only CASE[,CASE...]] [--maths-off]
        [--timeout-scale F] [--env NAME=VALUE]... [--saves DIR] --output DIR EXECUTABLE
    python3 scripts/port/lp64_equiv.py compare --streams sim[,gfx,snd] [--expect FILE] DIR...
    python3 scripts/port/lp64_equiv.py sensitivity [--video] --rom ROM --output DIR EXECUTABLE

run: each case of the named sets (lp64_equiv_cases.py: demos, solo, mp, menus, reload, saves) REPEAT
times (1 by default), run K into DIR/runK/, JOBS runs at a time (7 by default), through
lp64_play_run.py (a copy of EXECUTABLE in a fresh folder a run, CASE.log). Every run writes its
simulation trace to CASE.sim (GE_SIMTRACE); with --video also its picture digest to CASE.gfx
(GE_GFXDIGEST, and GE_GFXDIGEST_NODRAW=1 unless --draw) and its sound to CASE.wav (GE_AUDIO_LOCK=1,
GE_AUDIO_CAPTURE), with the profile's midas.ini (lp64_equiv_profile.py; only video builds read it;
`all` also sets GE_IRONSIGHTS_FONTS=1, so its font_style=2 loads).
--shots TICK (with --draw) adds GE_SHOT=TICK and keeps every screenshot in DIR/runK/CASE.shots/.
Prints `CASE: RAN (T ticks, S shots)` (T the trace's ticks, for a demo its playback's, S player 0's
last shot count) or
`CASE: FAIL (reason)` once all of a case's runs end, then `OK: N of N cases ran`, and with REPEAT
above 1 `OK: N of N cases agree with themselves` or `FAIL: CASE STREAM differs from itself at I`.
A run fails on anything lp64_play_judge.problems fails, on a missing required line (`ge: lockstep:
on` but for a demo, the frame limit, the stage load, and for a route with fire `moved` and the
shots lp64_equiv_cases.fired names) and on a stream file that is missing, empty or torn; a demo's
trace and digest must hold its playback (`demo=` lines). Self-agreement and compare read a demo's
trace and digest as its playback (lp64_equiv_compare.py). Self-agreement judges `sim` and, with
--video, `gfx` and `snd` (JUDGED_VIDEO_STREAMS): a video run fails without the sound lock's line `ge:
audio: locked to game fields`, and a demo's recording is not judged (a line a demo says so and why).
DIR/equiv.json records the run, written after the self-agreement check: each case's verdict, whether
it ran and whether it agreed with itself.
Before any case the executable is started once: it is refused without the exact line `ge: compare:
on`, or with --maths-off without `ge: compare: on (libm)`. --env passes a variable to every case;
it refuses what the runner sets (RUNNER_VARIABLES, and what lp64_play.py refuses). --saves DIR is
copied into the folder of each case of the saves set, which refuses to run without it.

compare (lp64_equiv_compare.py): each later DIR against the first, case by case, run 1 with run 1: `CASE STREAM A:B
identical (N)` or `CASE STREAM A:B first differs at I (FIELD)`, A and B the folders' names. With
--expect FILE (lp64_equiv_expected.txt), exit 0 only when every difference is the one FILE records
and everything else is identical, printing `OK: N cases, S streams, I identical, D as recorded`.
It refuses (exit 2) a folder whose record shows one run (--repeat 1), a compared case that did not
run or did not agree with itself, no verdicts at all (a record from before they were written), or,
with snd, a case whose log lacks the sound lock's line. A demo's snd prints `not judged`.

sensitivity: the solo case stage-dam at GE_SEED=1 (DIR/seed1) and GE_SEED=2 (DIR/seed2); exit 0
only when every stream it judges differs (as self-agreement), printing `OK: STREAMS differ between
seeds`.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))

from lp64_equiv_cases import SAVES_SETS, SHOTS, cases_of  # noqa: E402
from lp64_equiv_compare import (AUDIO_LOCKED, DEMO_SND_REASON, RECORD, STREAM_FILES, VIDEO_STREAMS,  # noqa: E402
                                compare_folders, judged, seed_verdict, self_disagreements, stream_kind)
from lp64_equiv_profile import PROFILES, profile_env, profile_ini  # noqa: E402
from lp64_equiv_streams import READERS, Unreadable  # noqa: E402
import lp64_play  # noqa: E402
from lp64_play_cases import Case, select_cases  # noqa: E402
from lp64_play_run import (FOLDER_VARIABLES, VIDEO_VARIABLES, Display, folder_environment, run_case,  # noqa: E402
                           timeout_seconds)
from run_copies import SharedExecutable  # noqa: E402

DEFAULT_JOBS = 7
# The variables the runner sets for its own streams, its lockstep and its seed; --env refuses them
# (GE_SIMTRACE_DETAIL and GE_GFXDIGEST_DETAIL pass).
RUNNER_VARIABLES = ('GE_LOCKSTEP', 'GE_FIXEDTICK', 'GE_NOSLEEP', 'GE_SIMTRACE', 'GE_GFXDIGEST', 'GE_GFXDIGEST_NODRAW',
                    'GE_AUDIO_LOCK', 'GE_AUDIO_CAPTURE', 'GE_SEED', 'GE_IRONSIGHTS_FONTS')
# port/src/main.c in a comparison build (-DGE_COMPARE=ON), once at boot.
COMPARE_ON = 'ge: compare: on'
COMPARE_LIBM = 'ge: compare: on (libm)'
SENSITIVITY_CASE = 'stage-dam'
# The streams a video run is judged by, for its agreement with itself and in sensitivity. `snd` joined
# with Task 9's sound lock (GE_AUDIO_LOCK, port_audio_lock.h), except a demo's (DEMO_SND_REASON).
JUDGED_VIDEO_STREAMS = ('gfx', 'snd')
SEEDS = ('1', '2')


def parse_env(parser: argparse.ArgumentParser, pairs: list[str], video: bool) -> dict[str, str]:
    for pair in pairs:
        name = pair.partition('=')[0]
        if name in RUNNER_VARIABLES:
            parser.error(f'--env {name} is refused: the runner sets it')
        if video and (name in VIDEO_VARIABLES or name in FOLDER_VARIABLES or name == 'GE_SHOT'):
            parser.error(f'--env {name} is refused with --video: the runner sets it')
    return lp64_play.parse_env(parser, pairs)


@dataclass(frozen=True)
class Options:
    """What every run of an invocation shares."""
    rom: Path
    extra: dict[str, str]
    display: Display
    draw: bool
    maybe_saves: Path | None

    def streams(self) -> tuple[str, ...]:
        return ('sim',) + (JUDGED_VIDEO_STREAMS if self.display.video else ())


@dataclass
class RunResult:
    """One run of a case: its problems, its trace's ticks and player 0's last shot count."""
    case: Case
    run: int
    problems: list[str]
    ticks: int = 0
    shots: int = 0


def stream_variables(case: Case, folder: Path, options: Options) -> dict[str, str]:
    """The variables that name the run's stream files in FOLDER."""
    base = folder.resolve()/case.name
    env = {'GE_SIMTRACE': f'{base}.sim'}
    if options.display.video:
        env.update(GE_GFXDIGEST=f'{base}.gfx', GE_AUDIO_LOCK='1', GE_AUDIO_CAPTURE=f'{base}.wav')
        if not options.draw:
            env['GE_GFXDIGEST_NODRAW'] = '1'
    return env


def run_once(case: Case, run: int, shared: SharedExecutable, output: Path, options: Options) -> RunResult:
    """Runs CASE once into OUTPUT/runRUN/ and judges its log and streams."""
    folder = output/f'run{run}'
    folder.mkdir(parents=True, exist_ok=True)
    saves = options.maybe_saves if case.set_name in SAVES_SETS else None
    outcome = run_case(case, shared, options.rom, dict(options.extra, **stream_variables(case, folder, options)),
                       folder, options.display, saves)
    found = list(outcome.problems)
    ticks = 0
    kind = stream_kind(case.name, 'sim')
    try:
        ticks = sum(1 for record in READERS[kind](folder/f'{case.name}.sim') if record.kind in ('T', 'D'))
    except Unreadable as error:
        found.append(f'{kind} unreadable: {error}')
    for stream in VIDEO_STREAMS if options.display.video else ():
        path = folder/f'{case.name}{STREAM_FILES[stream]}'
        if not path.is_file() or path.stat().st_size == 0:
            found.append(f'{stream}: no {path.name}')
            continue
        kind = stream_kind(case.name, stream)
        try:
            if kind in READERS:
                READERS[kind](path)
        except Unreadable as error:
            found.append(f'{kind} unreadable: {error}')
    log_path = folder/f'{case.name}.log'
    log = log_path.read_text(errors='replace').splitlines() if log_path.is_file() else []
    if options.display.video and AUDIO_LOCKED not in log:
        found.append(f'missing line: {AUDIO_LOCKED}')
    shots = [int(match.group(1)) for match in map(SHOTS.search, log) if match]
    return RunResult(case, run, found, ticks, shots[-1] if shots else 0)


def verdict(results: list[RunResult], repeat: int) -> str:
    failed = next((result for result in results if result.problems), None)
    if failed is None:
        first = results[0]
        return f'{first.case.name}: RAN ({first.ticks} ticks, {first.shots} shots)'
    where = f'run{failed.run}: ' if repeat > 1 else ''
    return f'{failed.case.name}: FAIL ({where}{failed.problems[0]})'


def run_all(cases: list[Case], shared: SharedExecutable, output: Path, options: Options, repeat: int,
            jobs: int) -> dict[str, list[RunResult]]:
    """Every case REPEAT times, JOBS runs at a time; prints each case's verdict once its runs end."""
    done: dict[str, list[RunResult]] = {case.name: [] for case in cases}
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(run_once, case, run, shared, output, options)
                   for run in range(1, repeat + 1) for case in cases]
        for future in as_completed(futures):
            result = future.result()
            done[result.case.name].append(result)
            if len(done[result.case.name]) == repeat:
                done[result.case.name].sort(key=lambda item: item.run)
                print(verdict(done[result.case.name], repeat), flush=True)
    return done


def compare_build(shared: SharedExecutable, options: Options, maths_off: bool) -> str | None:
    """Why the executable is not the comparison build asked for, from one short run; None when it is."""
    wanted = COMPARE_LIBM if maths_off else COMPARE_ON
    with tempfile.TemporaryDirectory(prefix='midas-lp64-equiv-') as temporary:
        child = shared.place(folder := Path(temporary))
        env = {key: value for key, value in os.environ.items() if not key.startswith('GE_')}
        env.update(GE_ROM=str(options.rom), GE_FRAMES='1', GE_NOINISAVE='1')
        if options.display.video:
            env.update(VIDEO_VARIABLES)
        try:
            text = subprocess.run([str(child)], cwd=folder, env=folder_environment(env, folder, options.display),
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, errors='replace', text=True,
                                  timeout=timeout_seconds(1, options.display.timeout_scale)).stdout
        except subprocess.TimeoutExpired:
            return 'its check run timed out'
    lines = set(text.splitlines())
    if wanted in lines:
        return None
    if COMPARE_LIBM in lines:
        return f'it logs {COMPARE_LIBM}, a build that keeps libm\'s maths: run it with --maths-off'
    return f'its log lacks the line {wanted}' + ('' if maths_off else ' (configure it with -DGE_COMPARE=ON)')


def refused_build(executable: Path, maybe_reason: str | None) -> bool:
    if maybe_reason is not None:
        print(f'error: {executable} is refused: {maybe_reason}', file=sys.stderr)
    return maybe_reason is not None


def check_number(parser: argparse.ArgumentParser, value: str, name: str) -> float:
    try:
        number = float(value)
    except ValueError:
        number = math.nan
    if not (math.isfinite(number) and number >= 1):
        parser.error(f'{name} takes a number of at least 1, not {value!r}')
    return number


def run_options(parser: argparse.ArgumentParser, args: argparse.Namespace) -> tuple[list[Case], Options]:
    """The cases and options of `run`, every refusal checked before anything runs."""
    if args.jobs < 1 or args.repeat < 1:
        parser.error('--jobs and --repeat take a whole number of at least 1')
    if args.draw and not args.video:
        parser.error('--draw needs --video: a headless build draws nothing')
    if args.shots is not None and (not args.draw or args.shots < 1):
        parser.error('--shots takes a tick of at least 1 and needs --draw (an undrawn picture is empty)')
    if args.profile != 'default' and not args.video:
        parser.error(f'--profile {args.profile} needs --video: only video builds read midas.ini')
    extra = parse_env(parser, args.env, args.video)
    set_names = [name for name in args.set.split(',') if name]
    try:
        cases = cases_of(set_names)
        if args.only is not None:
            cases = select_cases(cases, [name for name in args.only.split(',') if name])
    except ValueError as error:
        parser.error(str(error))
    saves_sets = [name for name in set_names if name in SAVES_SETS]
    if saves_sets and args.saves is None:
        parser.error(f'the {", ".join(saves_sets)} set needs --saves DIR: its cases start from a saves folder')
    if args.saves is not None and not args.saves.is_dir():
        parser.error(f'--saves {args.saves} is not a folder')
    if not cases:
        parser.error(f'--set {args.set} selects no case')
    display = Display(args.video, args.shots, check_number(parser, args.timeout_scale, '--timeout-scale'),
                      args.shots is not None, profile_ini(args.profile))
    extra.update(profile_env(args.profile))
    return cases, Options(args.rom.resolve(strict=True), extra, display, args.draw, args.saves)


def write_record(path: Path, args: argparse.Namespace, cases: list[Case], done: dict[str, list[RunResult]],
                 failed: list[str], disagreements: dict[str, str]) -> None:
    """DIR/equiv.json: the run's options, each run's figures and each case's verdict, whether it ran
    and whether it agreed with itself (null when not checked: one run, or a case that failed)."""
    checked = args.repeat > 1
    verdicts = {case.name: {'ran': case.name not in failed,
                            'agreed': (case.name not in disagreements) if checked and case.name not in failed else None}
                for case in cases}
    path.write_text(json.dumps({
        'executable': str(args.executable), 'profile': args.profile, 'video': args.video, 'draw': args.draw,
        'shots': args.shots, 'repeat': args.repeat, 'maths_off': args.maths_off,
        'cases': [case.name for case in cases],
        'runs': {name: [{'run': result.run, 'ticks': result.ticks, 'shots': result.shots,
                         'problems': result.problems} for result in results] for name, results in done.items()},
        'verdicts': verdicts,
    }, indent=2) + '\n')


def command_run(parser: argparse.ArgumentParser, args: argparse.Namespace) -> int:
    cases, options = run_options(parser, args)
    with SharedExecutable(args.executable.resolve(strict=True)) as shared:
        if refused_build(args.executable, compare_build(shared, options, args.maths_off)):
            return 2
        args.output.mkdir(parents=True, exist_ok=True)
        done = run_all(cases, shared, args.output, options, args.repeat, args.jobs)
    failed = [name for name, results in done.items() if any(result.problems for result in results)]
    ran = [case.name for case in cases if case.name not in failed]
    disagreements = (self_disagreements(ran, args.output, options.streams(), args.repeat)
                     if args.repeat > 1 else {})
    write_record(args.output/RECORD, args, cases, done, failed, disagreements)
    if failed:
        print(f'FAIL: {len(failed)} of {len(cases)} cases did not run')
        return 1
    print(f'OK: {len(cases)} of {len(cases)} cases ran')
    for case in cases if args.video else ():
        if not judged(case.name, 'snd'):
            print(f'{case.name}: snd not judged: {DEMO_SND_REASON}')
    if args.repeat == 1:
        return 0
    if disagreements:
        print('\n'.join(disagreements.values()))
        return 1
    print(f'OK: {len(cases)} of {len(cases)} cases agree with themselves')
    return 0


def command_sensitivity(parser: argparse.ArgumentParser, args: argparse.Namespace) -> int:
    display = Display(args.video, timeout_scale=check_number(parser, args.timeout_scale, '--timeout-scale'))
    case = select_cases(cases_of(['solo']), [SENSITIVITY_CASE])[0]
    with SharedExecutable(args.executable.resolve(strict=True)) as shared:
        options = Options(args.rom.resolve(strict=True), {}, display, False, None)
        if refused_build(args.executable, compare_build(shared, options, False)):
            return 2
        results = {}
        for seed in SEEDS:
            seeded = Options(options.rom, {'GE_SEED': seed}, display, False, None)
            results[seed] = run_once(case, 1, shared, args.output/f'seed{seed}', seeded)
            print(f'GE_SEED={seed} ' + verdict([results[seed]], 1), flush=True)
    if any(result.problems for result in results.values()):
        return 1
    agreed, line = seed_verdict(case.name, args.output/'seed1'/'run1', args.output/'seed2'/'run1', options.streams())
    print(line)
    return 0 if agreed else 1


def command_compare(parser: argparse.ArgumentParser, args: argparse.Namespace) -> int:
    streams = [name for name in args.streams.split(',') if name]
    if not streams or any(name not in STREAM_FILES for name in streams):
        parser.error(f'--streams takes {",".join(STREAM_FILES)}, not {args.streams!r}')
    if len(args.dirs) < 2:
        parser.error('compare needs two run folders or more')
    return compare_folders(args.dirs, streams, args.expect)


# ---- the command line ---------------------------------------------------------------------------
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    run = sub.add_parser('run', help='run the cases of named sets, REPEAT times each')
    run.add_argument('--rom', type=Path, required=True)
    run.add_argument('--set', required=True, metavar='NAME[,NAME...]')
    run.add_argument('--profile', choices=PROFILES, default='default')
    run.add_argument('--video', action='store_true', help='a GE_VIDEO comparison build: also CASE.gfx and CASE.wav')
    run.add_argument('--draw', action='store_true', help='with --video: draw the pictures as well as digest them')
    run.add_argument('--shots', type=int, metavar='TICK', help='with --draw: a screenshot every TICK pictures')
    run.add_argument('--repeat', type=int, default=1, metavar='N')
    run.add_argument('--jobs', type=int, default=DEFAULT_JOBS, metavar='N')
    run.add_argument('--only', metavar='CASE[,CASE...]')
    run.add_argument('--maths-off', action='store_true', help='the build keeps libm\'s maths (-DGE_COMPARE_LIBM=ON)')
    run.add_argument('--timeout-scale', default='1', metavar='F')
    run.add_argument('--env', action='append', default=[], metavar='NAME=VALUE')
    run.add_argument('--saves', type=Path, metavar='DIR', help='copied into the folder of each case of the saves set')
    run.add_argument('--output', type=Path, required=True, metavar='DIR')
    run.add_argument('executable', type=Path, metavar='EXECUTABLE')
    compare = sub.add_parser('compare', help='compare run folders\' streams with the first folder\'s')
    compare.add_argument('--streams', required=True, metavar='sim[,gfx,snd]')
    compare.add_argument('--expect', type=Path, metavar='FILE')
    compare.add_argument('dirs', type=Path, nargs='+', metavar='DIR')
    sensitivity = sub.add_parser('sensitivity', help='every stream must differ between two seeds')
    sensitivity.add_argument('--video', action='store_true')
    sensitivity.add_argument('--rom', type=Path, required=True)
    sensitivity.add_argument('--timeout-scale', default='1', metavar='F')
    sensitivity.add_argument('--output', type=Path, required=True, metavar='DIR')
    sensitivity.add_argument('executable', type=Path, metavar='EXECUTABLE')
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    command = {'run': command_run, 'compare': command_compare, 'sensitivity': command_sensitivity}[args.command]
    return command(parser, args)


if __name__ == '__main__':
    sys.exit(main())
