"""Boot a 64-bit executable to its expected asset-class stop, or run it (LP64-003 Task 4, LP64-004).

Until LP64-004 converts a class of ROM data, a 64-bit build stops at the first in-place conversion
the boot reaches, logging `ge: lp64: stopping: asset class "NAME" ...` and exiting with status 5
(port/src/port_lp64.c). This runs each executable once, with the ROM. With --expect-stop it passes
only if every run exited with status 5, logged the startup address line, stopped at the expected
class, and logged no fatal signal, no sanitizer report, no `ge: lp64: pool` line (a pool too small
at 64 bits), no `ge: lp64: cannot convert` line (a converter's refusal) and no other `ge: lp64:`
line outside INFORMATIONAL_PREFIXES (the stop line aside). With --expect-run, for
an executable that converts every class its run reaches, it passes only if every run exited with
status 0, logged the address line and the frame limit line with the expected count (pcsched.c's
`ge: frame limit reached: frames=N`), and logged no asset-class stop and none of those four.

    python3 scripts/port/lp64_boot.py --rom ROM --expect-stop "animation tables" EXECUTABLE...
    python3 scripts/port/lp64_boot.py --rom ROM --expect-run [--frames N] EXECUTABLE...

Each run gets its own temporary directory under the system temp folder (the executable is copied
there and runs there), GE_FRAMES (120 for --expect-stop, --frames for --expect-run, 600 by default),
GE_SEED=1, GE_NOINISAVE=1, GE_ROM, and with --stage GE_STAGE and GE_STAGE_AFTER=1 (the mission
starts at once, as in the smoke); every other GE_ variable of the caller's environment is dropped.
LeakSanitizer is off (ASAN_OPTIONS=detect_leaks=0): stopping
mid-boot leaves the game's allocations unfreed by design. A run longer than TIMEOUT_SECONDS fails
(for --expect-run, TIMEOUT_SECONDS for each 600 frames begun).
"""
import argparse
from dataclasses import dataclass, field
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TIMEOUT_SECONDS = 120
EXPECTED_STATUS = 5
RUN_STATUS = 0
STOP_FRAMES = 120
RUN_FRAMES = 600
TAIL_LINES = 20
# Libraries copied from the executable's folder into each run's folder when present: a
# Windows build finds its DLLs beside the executable, never on a library path.
RUNTIME_LIBRARIES = ('SDL2.dll',)
ADDRESS_LINE = 'ge: lp64: address space OK'
STOP_PATTERN = re.compile(r'ge: lp64: stopping: asset class "([^"]+)" is marked unconverted in this build')
# pcsched.c prints this, then exits 0, once GE_FRAMES frames have run.
FRAME_LIMIT_PATTERN = re.compile(r'ge: frame limit reached: frames=(\d+)')
FATAL_SIGNAL = 'ge: fatal signal'
SANITIZER_MARKERS = ('runtime error:', 'ERROR: AddressSanitizer', 'ERROR: LeakSanitizer',
                     'ERROR: UndefinedBehaviorSanitizer')
# A pool too small at 64 bits (LP64-004 Task 2) and a converter's refusal (port_romconv.h).
REFUSAL_MARKERS = ('ge: lp64: pool', 'ge: lp64: cannot convert')
# The `ge: lp64:` lines a passing run may print (owner decision 3 as the controller read it,
# FORK-089 "LP64-004 task 16"): the address check's success, the two audio banks' and the music
# sequence table's sizes, a background's tables, the 32-bit oracle build's lines, which
# lp64_oracle.py judges by their totals, and each pool's peak use (LP64-005 Task 13), which
# `lp64_play.py peaks` reads. Any other kind fails a run as a refusal does (fix wave G);
# the asset-class stop is judged by STOP_PATTERN instead.
LP64_PREFIX = 'ge: lp64: '
INFORMATIONAL_PREFIXES = ('ge: lp64: address space OK', 'ge: lp64: audio bank ',
                          'ge: lp64: music sequence table ', 'ge: lp64: bg tables ', 'ge: lp64: oracle: ',
                          'ge: lp64: peak ')


def failing_lp64_lines(lines: list[str]) -> list[str]:
    """The lines of LINES that fail a run: each with a pool or refusal marker, and each other
    `ge: lp64:` line that is neither of an informational kind nor the asset-class stop."""
    found = []
    for line in lines:
        at = line.find(LP64_PREFIX)
        if at < 0:
            continue
        text = line[at:]
        known = text.startswith(INFORMATIONAL_PREFIXES) or STOP_PATTERN.match(text)
        if any(marker in line for marker in REFUSAL_MARKERS) or not known:
            found.append(line)
    return found


@dataclass
class BootResult:
    """What one boot log says."""
    address_ok: bool
    stop_class: str | None
    fatal_signal: bool
    sanitizer_errors: list[str] = field(default_factory=list)
    refusals: list[str] = field(default_factory=list)
    frames_run: int | None = None


@dataclass
class Expectation:
    """What a run must do: stop at STOP_CLASS with status 5, or, with STOP_CLASS None, run FRAMES
    frames and exit with status 0."""
    stop_class: str | None
    frames: int

    @property
    def status(self) -> int:
        return EXPECTED_STATUS if self.stop_class is not None else RUN_STATUS

    @property
    def timeout(self) -> int:
        """TIMEOUT_SECONDS, and for a run as much again for each further 600 frames begun."""
        if self.stop_class is not None:
            return TIMEOUT_SECONDS
        return TIMEOUT_SECONDS * max(1, -(-self.frames // RUN_FRAMES))


def parse_log(text: str) -> BootResult:
    """Reads a boot log: the address line, the class of the first guard stop, a fatal signal,
    every sanitizer report line, every failing `ge: lp64:` line (failing_lp64_lines), and the
    frames the frame limit line reports."""
    match = STOP_PATTERN.search(text)
    frame_limit = FRAME_LIMIT_PATTERN.search(text)
    lines = text.splitlines()
    return BootResult(
        address_ok=any(line.startswith(ADDRESS_LINE) for line in lines),
        stop_class=match.group(1) if match else None,
        fatal_signal=any(FATAL_SIGNAL in line for line in lines),
        sanitizer_errors=[line for line in lines if any(marker in line for marker in SANITIZER_MARKERS)],
        refusals=failing_lp64_lines(lines),
        frames_run=int(frame_limit.group(1)) if frame_limit else None,
    )


def problems(status: int | None, result: BootResult, expect: Expectation) -> list[str]:
    """Every way a run differs from what EXPECT asks; empty when it passes. STATUS is None for a run
    that timed out, negative for one killed by a signal."""
    found = []
    if status is None:
        found.append(f'timed out after {expect.timeout} s')
    elif status < 0:
        found.append(f'killed by signal {-status}')
    elif status != expect.status:
        found.append(f'exit status {status}, expected {expect.status}')
    if not result.address_ok:
        found.append(f'no "{ADDRESS_LINE}" line')
    if expect.stop_class is None:
        if result.stop_class is not None:
            found.append(f'stopped at "{result.stop_class}", expected to run')
        if result.frames_run is None:
            found.append(f'no "ge: frame limit reached: frames={expect.frames}" line')
        elif result.frames_run != expect.frames:
            found.append(f'ran {result.frames_run} frames, expected {expect.frames}')
    elif result.stop_class is None:
        found.append(f'no asset-class stop, expected "{expect.stop_class}"')
    elif result.stop_class != expect.stop_class:
        found.append(f'stopped at "{result.stop_class}", expected "{expect.stop_class}"')
    if result.fatal_signal:
        found.append('fatal signal')
    if result.sanitizer_errors:
        found.append(f'sanitizer report: {result.sanitizer_errors[0].strip()}')
    if result.refusals:
        found.append(f'logged: {result.refusals[0].strip()}')
    return found


def boot_environment(rom: Path, stage: str | None, frames: int) -> dict[str, str]:
    env = {key: value for key, value in os.environ.items() if not key.startswith('GE_')}
    env.update(GE_ROM=str(rom), GE_FRAMES=str(frames), GE_SEED='1', GE_NOINISAVE='1',
               ASAN_OPTIONS='detect_leaks=0')
    if stage is not None:
        # At once, as the mission smoke launches: alone, GE_STAGE waits 120 ticks (front.c).
        env.update(GE_STAGE=stage, GE_STAGE_AFTER='1')
    return env


def run_one(executable: Path, rom: Path, stage: str | None, expect: Expectation) -> tuple[int | None, str]:
    """Runs EXECUTABLE in a fresh temporary directory; returns its status (None on timeout) and
    its combined output."""
    with tempfile.TemporaryDirectory(prefix='midas-lp64-boot-') as temporary:
        folder = Path(temporary)
        child = folder/executable.name
        shutil.copy2(executable, child)
        # A Windows video build loads SDL2.dll from its own folder (WIN64-001).
        for library in RUNTIME_LIBRARIES:
            if (executable.parent/library).is_file():
                shutil.copy2(executable.parent/library, folder/library)
        log_path = folder/'boot.log'
        with log_path.open('wb') as log:
            try:
                status = subprocess.run([str(child)], cwd=folder,
                                        env=boot_environment(rom, stage, expect.frames),
                                        stdout=log, stderr=subprocess.STDOUT,
                                        timeout=expect.timeout).returncode
            except subprocess.TimeoutExpired:
                status = None
        return status, log_path.read_text(errors='replace')


def check(executable: Path, rom: Path, expect: Expectation, stage: str | None) -> bool:
    """Runs one executable and prints its verdict; True when it did what EXPECT asks."""
    label = str(executable) + (f', GE_STAGE={stage}' if stage is not None else '')
    status, text = run_one(executable, rom, stage, expect)
    found = problems(status, parse_log(text), expect)
    if not found:
        verdict = (f'stopped at "{expect.stop_class}"' if expect.stop_class is not None
                   else f'ran {expect.frames} frames')
        print(f'OK: {verdict} ({label})', flush=True)
        return True
    print(f'FAIL: {label}: ' + '; '.join(found))
    print(f'--- last {TAIL_LINES} log lines ---')
    print('\n'.join(text.splitlines()[-TAIL_LINES:]), flush=True)
    return False


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--rom', type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--expect-stop', metavar='NAME', help='the asset class, e.g. "animation tables"')
    mode.add_argument('--expect-run', action='store_true',
                      help='the executable runs its frames and exits with status 0')
    parser.add_argument('--frames', type=int, metavar='N',
                        help=f'with --expect-run, the frames to run (GE_FRAMES; default {RUN_FRAMES})')
    parser.add_argument('--stage', metavar='NAME', help='also set GE_STAGE (a mission name such as dam) and GE_STAGE_AFTER=1')
    parser.add_argument('executables', nargs='+', type=Path, metavar='EXECUTABLE')
    options = parser.parse_args(argv)
    if options.frames is not None and not options.expect_run:
        parser.error('--frames goes with --expect-run')
    if options.frames is not None and options.frames <= 0:
        parser.error('--frames must be above zero')
    if options.expect_run:
        expect = Expectation(stop_class=None, frames=options.frames or RUN_FRAMES)
    else:
        expect = Expectation(stop_class=options.expect_stop, frames=STOP_FRAMES)
    rom = options.rom.resolve(strict=True)
    results = [check(exe.resolve(strict=True), rom, expect, options.stage) for exe in options.executables]
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
