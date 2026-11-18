"""Run lp64_play.py's cases on an executable, several at a time (LP64-005 Task 1).

Each case runs on a copy of the executable in a fresh temporary folder, with every `GE_` variable
of the caller dropped and GE_ROM, GE_FRAMES, GE_SEED=1, GE_NOINISAVE=1,
ASAN_OPTIONS=detect_leaks=0, the case's own variables and the caller's --env set. Its combined
output goes to DIR/CASE.log; DIR/play.json records every case's variables, status, time and
problems. The runs are paced in real time (60 retraces a second), so a case's frames fix its
wall time; a case still running at timeout_seconds(frames) is killed and fails with its last line.
"""
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from lp64_play_cases import Case
from lp64_play_judge import problems

# GE_FIXEDTICK=1 makes a demo advance one field a tick instead of at its recorded pace, and
# GE_NOSLEEP=1 lets the frame limit, which counts retraces, pass before the game has done
# anything (plan "What was measured"); the tool never sets either and refuses them from --env.
FORBIDDEN_VARIABLES = ('GE_FIXEDTICK', 'GE_NOSLEEP')
RETRACES_PER_SECOND = 60
# A case may take this many times its paced length, plus the boot, before it is a timeout: the
# sanitizers and emulation run the game slower than real time (17 to 35 frames a second measured
# under the sanitizers), and seven cases share the machine.
TIMEOUT_FACTOR = 5
TIMEOUT_BOOT_SECONDS = 120


def timeout_seconds(frames: int) -> float:
    return TIMEOUT_BOOT_SECONDS + TIMEOUT_FACTOR * frames / RETRACES_PER_SECOND


@dataclass
class Outcome:
    """What one case did."""
    case: Case
    status: int | None
    seconds: float
    problems: list[str]

    @property
    def passed(self) -> bool:
        return not self.problems

    def verdict(self) -> str:
        return f'{self.case.name}: PASS' if self.passed else f'{self.case.name}: FAIL ({self.problems[0]})'


def case_environment(case: Case, rom: Path, extra: dict[str, str],
                     base: dict[str, str] | None = None) -> dict[str, str]:
    """The environment of CASE's run: BASE (the caller's) without its GE_ variables, then the
    fixed variables, the case's own and EXTRA."""
    source = os.environ if base is None else base
    env = {key: value for key, value in source.items() if not key.startswith('GE_')}
    env.update(GE_ROM=str(rom), GE_FRAMES=str(case.frames), GE_SEED='1', GE_NOINISAVE='1',
               ASAN_OPTIONS='detect_leaks=0')
    env.update(dict(case.env))
    env.update(extra)
    forbidden = [name for name in FORBIDDEN_VARIABLES if name in env]
    if forbidden:
        raise ValueError(f'{", ".join(forbidden)} must not be set for a play run')
    return env


def run_case(case: Case, executable: Path, rom: Path, extra: dict[str, str], output: Path) -> Outcome:
    """Runs CASE and judges it; its log is OUTPUT/CASE.log."""
    timeout = timeout_seconds(case.frames)
    log_path = output/f'{case.name}.log'
    env = case_environment(case, rom, extra)
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix='midas-lp64-play-') as temporary:
        folder = Path(temporary)
        child = folder/executable.name
        shutil.copy2(executable, child)
        with log_path.open('wb') as log:
            try:
                status = subprocess.run([str(child)], cwd=folder, env=env, stdout=log,
                                        stderr=subprocess.STDOUT, timeout=timeout).returncode
            except subprocess.TimeoutExpired:
                status = None
    seconds = time.monotonic() - started
    text = log_path.read_text(errors='replace')
    return Outcome(case, status, seconds, problems(case, status, text, timeout))


def record(outcomes: list[Outcome], executable: Path, extra: dict[str, str], path: Path) -> None:
    """Writes play.json: the run's executable and extra variables, and each case's outcome."""
    data = {
        'executable': str(executable),
        'extra_env': extra,
        'cases': [{
            'name': outcome.case.name, 'set': outcome.case.set_name, 'env': dict(outcome.case.env),
            'frames': outcome.case.frames, 'expected_status': outcome.case.status,
            'status': outcome.status, 'seconds': round(outcome.seconds, 1),
            'passed': outcome.passed, 'problems': outcome.problems, 'log': f'{outcome.case.name}.log',
        } for outcome in outcomes],
    }
    path.write_text(json.dumps(data, indent=2) + '\n')


def run_cases(cases: list[Case], executable: Path, rom: Path, extra: dict[str, str], output: Path,
              jobs: int) -> list[Outcome]:
    """Runs CASES, JOBS at a time, printing each verdict as it comes; returns the outcomes in the
    cases' order after writing OUTPUT/play.json."""
    for case in cases:
        case_environment(case, rom, extra)  # refuse a forbidden variable before any run starts
    output.mkdir(parents=True, exist_ok=True)
    by_name = {}
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(run_case, case, executable, rom, extra, output) for case in cases]
        for future in as_completed(futures):
            outcome = future.result()
            by_name[outcome.case.name] = outcome
            print(outcome.verdict(), flush=True)
    outcomes = [by_name[case.name] for case in cases]
    record(outcomes, executable, extra, output/'play.json')
    return outcomes
