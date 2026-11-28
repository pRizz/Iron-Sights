"""Run lp64_play.py's cases on an executable, several at a time (LP64-005 Task 1).

Each case runs on the executable in a fresh temporary folder (a link to one copy made before any
case starts, run_copies.py), with every `GE_` variable
of the caller dropped and GE_ROM, GE_FRAMES, GE_SEED=1, GE_NOINISAVE=1,
ASAN_OPTIONS=detect_leaks=0, the case's own variables and the caller's --env set. Its combined
output goes to DIR/CASE.log; DIR/play.json records every case's variables, status, time and
problems. The runs are paced in real time (60 retraces a second; with --video GE_TICKHZ=60 keeps
that rate, but the pump then waits for each picture, so a renderer slower than 60 pictures a second
stretches the case), so a case's frames fix its wall time; a case still running at
timeout_seconds(frames) is killed and fails with its last line.

With video (`run --video`) a case also gets SDL_VIDEODRIVER=offscreen (Mesa's software GL; the
dummy driver cannot make a GL context; on Windows the native `windows` driver, a real window; on
macOS `cocoa` with GE_WINDOW_HIDDEN=1 and SDL_MAC_BACKGROUND_APP=1, a real window that is never shown
and no Dock icon),
SDL_AUDIODRIVER=dummy, GE_WINDOW=640x480, GE_NOINPUT=1,
GE_TICKHZ=60, a HOME and GE_ICON_CACHE_DIR inside its temporary folder, and a midas.ini there with
`[display] fullscreen=windowed`; `--shots N` adds GE_SHOT=N, and the last shot_*.bmp the case wrote
is copied to DIR/CASE.bmp. These are ROM-derived pictures: they stay in
the output folder and are never committed.

A case that records (the `sound` set, lp64_play_sound.py) gets GE_AUDIO_CAPTURE=DIR/CASE.wav, and
its recording is judged with its log.

A case with a route (the `input` set) gets GE_INPUTPLAY=DIR/CASE.route. A route of steps is built
first (lp64_play_route.build_presses): recording run K replays the presses found so far from
DIR/CASE.stepK.route, logs to DIR/CASE.stepK.log and is stopped at the line its step waits for.
"""
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import time

from lp64_play_cases import DEMO_SETS, Case
from lp64_play_judge import pictures_drawn, problems
from lp64_play_route import RouteError, Step, build_presses, event_tick, route_text
from lp64_play_sound import capture_path, capture_problems
from run_copies import SharedExecutable

# GE_FIXEDTICK=1 makes a demo advance one field a tick instead of at its recorded pace, and
# GE_NOSLEEP=1 lets the frame limit, which counts retraces, pass before the game has done
# anything (plan "What was measured"); the tool never sets either and refuses them from --env.
FORBIDDEN_VARIABLES = ('GE_FIXEDTICK', 'GE_NOSLEEP')
# A case marked lockstep=True runs with GE_LOCKSTEP=1 (one retrace a game frame and a clock that
# counts them, LP64-006 Task 2); --env and a case's own variables never set it, and no demo gets it.
LOCKSTEP_VARIABLE = 'GE_LOCKSTEP'
# What the tool sets for every case (case_environment; GE_INPUTPLAY for a case with a route, its own
# route file): --env refuses them, since a value of the caller's would contradict the case's frame
# limit, wall-clock limit, ROM, seed or route.
TOOL_VARIABLES = ('GE_ROM', 'GE_FRAMES', 'GE_SEED', 'GE_NOINISAVE', 'GE_INPUTPLAY')
RETRACES_PER_SECOND = 60
# A case may take this many times its paced length, plus the boot, before it is a timeout: the
# sanitizers and emulation run the game slower than real time (17 to 35 frames a second measured
# under the sanitizers), and seven cases share the machine.
TIMEOUT_FACTOR = 5
TIMEOUT_BOOT_SECONDS = 120


def host_video_driver(os_name: str = os.name, platform: str = sys.platform) -> str:
    """The SDL video driver a video case uses on a host whose os.name is OS_NAME and whose
    sys.platform is PLATFORM. On Windows (WIN64-002) SDL's offscreen driver has no GL library to load
    ("Could not initialize OpenGL / GLES library"), so a case opens a real 640x480 window through the
    native driver. On macOS (MAC-002) the offscreen driver makes no GL window either, so a case gets a
    real Cocoa window, which host_video_variables keeps hidden."""
    if platform == 'darwin':
        return 'cocoa'
    return 'windows' if os_name == 'nt' else 'offscreen'


def host_video_variables(platform: str = sys.platform) -> dict[str, str]:
    """What every run with video gets on a host whose sys.platform is PLATFORM (plan "What was
    measured": video needs no display server). GE_TICKHZ=60 (src/game/frametiming.c, portTickHz; it
    wins over midas.ini's tickhz): a video build's retrace pump runs at tickhz, 120 in the ini it
    writes by default, while every case's frames were measured headless at 60 retraces a second; at
    120 a case ends at half its time. On macOS GE_WINDOW_HIDDEN=1 as well (MAC-002 decision 1): the
    game creates its window and never shows it, so the Mac's GPU draws every case and nothing takes
    the screen. Only a macOS build reads that variable, and no other host's set holds it. With it
    goes SDL_MAC_BACKGROUND_APP=1, SDL's own hint: SDL then leaves the process a background app, so
    a case shows no Dock icon either (measured: without it each case is listed as a foreground app
    while it runs; with it the hidden window is drawn the same)."""
    driver = host_video_driver('nt' if platform == 'win32' else 'posix', platform)
    variables = {'SDL_VIDEODRIVER': driver, 'SDL_AUDIODRIVER': 'dummy', 'GE_WINDOW': '640x480',
                 'GE_NOINPUT': '1', 'GE_TICKHZ': '60'}
    if platform == 'darwin':
        variables.update(GE_WINDOW_HIDDEN='1', SDL_MAC_BACKGROUND_APP='1')
    return variables


# The host's own set (host_video_variables has the reasons).
VIDEO_DRIVER = host_video_driver()
VIDEO_VARIABLES = host_video_variables()
# A case that records its sound (Case.capture) gets this variable naming DIR/CASE.wav.
CAPTURE_VARIABLE = 'GE_AUDIO_CAPTURE'
# The variables a run with video sets in its folder at launch (folder_environment).
FOLDER_VARIABLES = ('HOME', 'GE_ICON_CACHE_DIR')
# GE_WINDOW sets only the windowed size (port/src/video.c, videoInit); the default midas.ini has
# fullscreen=1, a borderless window the size of the display (1024x768 offscreen). The game reads
# midas.ini from its root, the executable's folder; a file with only this key keeps every other
# setting at its code default.
WINDOWED_INI = '[display]\nfullscreen=windowed\n'


@dataclass(frozen=True)
class Display:
    """How the cases run: with VIDEO (a GE_VIDEO executable, offscreen), with SHOTS, a screenshot
    every SHOTS pictures drawn (GE_SHOT counts swaps), the last kept as DIR/CASE.bmp, and with every
    wall-clock limit (a case's and its recording runs') multiplied by TIMEOUT_SCALE."""
    video: bool = False
    shots: int | None = None
    timeout_scale: float = 1.0
    # Every screenshot of a case copied to DIR/CASE.shots/ (`--keep-shots`), not only its last.
    keep_shots: bool = False
    # The midas.ini a run with video gets in its folder (lp64_equiv_profile.py's profiles).
    ini: str = WINDOWED_INI


HEADLESS = Display()


def timeout_seconds(frames: int, scale: float = 1.0) -> float:
    """The wall-clock limit of a case of FRAMES frames, times SCALE (`--timeout-scale`, for hosts
    slower than the factor allows, such as an emulated container running a video build)."""
    return scale * (TIMEOUT_BOOT_SECONDS + TIMEOUT_FACTOR * frames / RETRACES_PER_SECOND)


@dataclass
class Outcome:
    """What one case did."""
    case: Case
    status: int | None
    seconds: float
    problems: list[str]
    # The ticks of the route's presses, for a case with a route.
    route_ticks: list[int] | None = None
    # With video, the pictures the case drew (the last `gfxtasks=` count of its frame-limit line or
    # once-a-second heartbeat, lp64_play_judge.py), when it says.
    pictures: int | None = None

    @property
    def passed(self) -> bool:
        return not self.problems

    def verdict(self) -> str:
        drawn = '' if self.pictures is None else f' ({self.pictures} pictures)'
        return f'{self.case.name}: PASS{drawn}' if self.passed else f'{self.case.name}: FAIL ({self.problems[0]})'


def case_environment(case: Case, rom: Path, extra: dict[str, str],
                     base: dict[str, str] | None = None, display: Display = HEADLESS) -> dict[str, str]:
    """The environment of CASE's run: BASE (the caller's) without its GE_ variables, then the
    fixed variables, DISPLAY's, the case's own and EXTRA. The run folder's own variables are
    added at launch (folder_environment)."""
    source = os.environ if base is None else base
    env = {key: value for key, value in source.items() if not key.startswith('GE_')}
    env.update(GE_ROM=str(rom), GE_FRAMES=str(case.frames), GE_SEED='1', GE_NOINISAVE='1',
               ASAN_OPTIONS='detect_leaks=0')
    if display.video:
        env.update(VIDEO_VARIABLES)
    if display.shots is not None:
        env['GE_SHOT'] = str(display.shots)
    if LOCKSTEP_VARIABLE in dict(case.env) or LOCKSTEP_VARIABLE in extra:
        raise ValueError(f'{LOCKSTEP_VARIABLE} is set only for a case marked lockstep=True')
    if case.lockstep and case.set_name in DEMO_SETS:
        raise ValueError(f'{case.name}: a demo never runs in lockstep')
    env.update(dict(case.env))
    env.update(extra)
    if case.lockstep:
        env[LOCKSTEP_VARIABLE] = '1'
    forbidden = [name for name in FORBIDDEN_VARIABLES if name in env]
    if forbidden:
        raise ValueError(f'{", ".join(forbidden)} must not be set for a play run')
    return env


def folder_environment(env: dict[str, str], folder: Path, display: Display) -> dict[str, str]:
    """ENV with, for a run with video, a HOME and GE_ICON_CACHE_DIR made inside FOLDER, the run's
    temporary folder, where the video build writes its settings and icon cache, and FOLDER's
    midas.ini written from DISPLAY (WINDOWED_INI by default, to run windowed)."""
    if not display.video:
        return env
    home, icons = folder/'home', folder/'icons'
    home.mkdir()
    icons.mkdir()
    (folder/'midas.ini').write_text(display.ini)
    return dict(env, HOME=str(home), GE_ICON_CACHE_DIR=str(icons))


def keep_last_shot(folder: Path, target: Path) -> None:
    """Copies the last shot_*.bmp in FOLDER to TARGET, when there is one."""
    shots = sorted(folder.glob('shot_*.bmp'))
    if shots:
        shutil.copy2(shots[-1], target)


def keep_every_shot(folder: Path, target: Path) -> None:
    """Copies every shot_*.bmp in FOLDER into the folder TARGET."""
    target.mkdir(parents=True, exist_ok=True)
    for shot in sorted(folder.glob('shot_*.bmp')):
        shutil.copy2(shot, target/shot.name)


def recording_tick(case: Case, executable: SharedExecutable, env: dict[str, str], output: Path, route: str, step: Step,
                   after: int, index: int, display: Display = HEADLESS,
                   maybe_saves: Path | None = None) -> int | None:
    """Runs CASE with ROUTE until STEP's line appears at or after tick AFTER and gives its tick (None
    when the run ends or times out first). Its log is OUTPUT/CASE.stepINDEX.log. MAYBE_SAVES, a folder,
    is copied into the run's folder first, as run_case does."""
    route_path = (output/f'{case.name}.step{index}.route').resolve()
    route_path.write_text(route)
    env = dict(env, GE_INPUTPLAY=str(route_path))
    found = None
    with tempfile.TemporaryDirectory(prefix='midas-lp64-play-') as temporary, \
            (output/f'{case.name}.step{index}.log').open('w') as log:
        child = executable.place(Path(temporary))
        if maybe_saves is not None:
            shutil.copytree(maybe_saves, temporary, dirs_exist_ok=True)
        run_env = folder_environment(env, Path(temporary), display)
        process = subprocess.Popen([str(child)], cwd=temporary, env=run_env, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, errors='replace')
        watchdog = threading.Timer(timeout_seconds(case.frames, display.timeout_scale), process.kill)
        watchdog.start()
        try:
            seen = []
            for line in process.stdout:
                log.write(line)
                seen.append(line)
                if step.wait.search(line.rstrip('\n')):
                    found = event_tick(seen, step.wait, after)
                    if found is not None:
                        break
        finally:
            watchdog.cancel()
            process.kill()
            process.wait()
    return found


def route_file(case: Case, executable: SharedExecutable, env: dict[str, str], output: Path,
               display: Display = HEADLESS, maybe_saves: Path | None = None) -> tuple[Path, list[int]]:
    """Writes CASE's route to OUTPUT/CASE.route, building a route of steps first (its recording runs
    start from MAYBE_SAVES too); gives the file and the presses' ticks. Raises RouteError when a step's
    line never appears."""
    presses = list(case.route.presses)
    if case.route.steps:
        presses = build_presses(case.route.steps, lambda route, step, after, index: recording_tick(
            case, executable, env, output, route, step, after, index, display, maybe_saves))
    path = (output/f'{case.name}.route').resolve()
    path.write_text(route_text(presses))
    return path, sorted(tick for tick, _ in presses)


def saves_changed(saves: Path, folder: Path) -> list[str]:
    """A problem for each file of the folder SAVES whose copy in FOLDER, a run's folder, no longer
    holds the same bytes, naming its first differing byte."""
    found = []
    for source in sorted(path for path in saves.rglob('*') if path.is_file()):
        name = source.relative_to(saves)
        copy = folder/name
        before = source.read_bytes()
        after = copy.read_bytes() if copy.is_file() else None
        if after == before:
            continue
        if after is None:
            found.append(f'saves: the run removed {name.as_posix()}')
            continue
        first = next((at for at, (old, new) in enumerate(zip(before, after)) if old != new),
                     min(len(before), len(after)))
        found.append(f'saves: the run changed {name.as_posix()} (first at byte {first})')
    return found


def run_case(case: Case, executable: SharedExecutable, rom: Path, extra: dict[str, str], output: Path,
             display: Display = HEADLESS, maybe_saves: Path | None = None) -> Outcome:
    """Runs CASE and judges it; its log is OUTPUT/CASE.log. MAYBE_SAVES, a folder, is copied into
    the run's folder first, and into each recording run of a route of steps (lp64_equiv.py run
    --saves); the case fails when the run leaves one of its files with other bytes."""
    timeout = timeout_seconds(case.frames, display.timeout_scale)
    log_path = output/f'{case.name}.log'
    env = case_environment(case, rom, extra, display=display)
    if case.capture:
        env[CAPTURE_VARIABLE] = str(capture_path(output, case))
    started = time.monotonic()
    route_ticks = None
    if case.route is not None:
        try:
            path, route_ticks = route_file(case, executable, env, output, display, maybe_saves)
        except RouteError as error:
            return Outcome(case, None, time.monotonic() - started, [str(error)])
        env['GE_INPUTPLAY'] = str(path)
    with tempfile.TemporaryDirectory(prefix='midas-lp64-play-') as temporary:
        folder = Path(temporary)
        child = executable.place(folder)
        if maybe_saves is not None:
            shutil.copytree(maybe_saves, folder, dirs_exist_ok=True)
        with log_path.open('wb') as log:
            try:
                status = subprocess.run([str(child)], cwd=folder, env=folder_environment(env, folder, display),
                                        stdout=log, stderr=subprocess.STDOUT, timeout=timeout).returncode
            except subprocess.TimeoutExpired:
                status = None
        changed = [] if maybe_saves is None else saves_changed(maybe_saves, folder)
        if display.shots is not None:
            keep_last_shot(folder, output/f'{case.name}.bmp')
        if display.keep_shots:
            keep_every_shot(folder, output/f'{case.name}.shots')
    seconds = time.monotonic() - started
    text = log_path.read_text(errors='replace')
    found = problems(case, status, text, timeout, display.video) + changed
    if case.capture:
        found += capture_problems(capture_path(output, case))
    return Outcome(case, status, seconds, found, route_ticks, pictures_drawn(text) if display.video else None)


def record(outcomes: list[Outcome], executable: Path, extra: dict[str, str], path: Path,
           display: Display = HEADLESS, frames_scale: int = 1) -> None:
    """Writes play.json: the run's executable, extra variables, display and frames scale, and each
    case's outcome."""
    data = {
        'executable': str(executable),
        'extra_env': extra,
        'video': display.video,
        'shots': display.shots,
        'timeout_scale': display.timeout_scale,
        'frames_scale': frames_scale,
        'cases': [{
            'name': outcome.case.name, 'set': outcome.case.set_name, 'env': dict(outcome.case.env),
            'frames': outcome.case.frames, 'expected_status': outcome.case.status,
            'status': outcome.status, 'seconds': round(outcome.seconds, 1),
            'passed': outcome.passed, 'problems': outcome.problems, 'log': f'{outcome.case.name}.log',
            **({} if outcome.route_ticks is None else {'route_ticks': outcome.route_ticks}),
            **({'recording': f'{outcome.case.name}.wav'} if outcome.case.capture else {}),
            **({} if outcome.pictures is None else {'pictures': outcome.pictures}),
        } for outcome in outcomes],
    }
    path.write_text(json.dumps(data, indent=2) + '\n')


def run_cases(cases: list[Case], executable: Path, rom: Path, extra: dict[str, str], output: Path,
              jobs: int, display: Display = HEADLESS, frames_scale: int = 1) -> list[Outcome]:
    """Runs CASES (already scaled by FRAMES_SCALE, which play.json records), JOBS at a time, printing
    each verdict as it comes; returns the outcomes in the cases' order after writing OUTPUT/play.json."""
    for case in cases:
        case_environment(case, rom, extra, display=display)  # refuse a forbidden variable before any run starts
    output.mkdir(parents=True, exist_ok=True)
    by_name = {}
    # One copy made here, before any case starts, and linked into each case's folder (run_copies.py):
    # a copy made per case inside the pool raced with the other cases' starts (Text file busy).
    with SharedExecutable(executable) as shared, ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(run_case, case, shared, rom, extra, output, display) for case in cases]
        for future in as_completed(futures):
            outcome = future.result()
            by_name[outcome.case.name] = outcome
            print(outcome.verdict(), flush=True)
    outcomes = [by_name[case.name] for case in cases]
    record(outcomes, executable, extra, output/'play.json', display, frames_scale)
    return outcomes
