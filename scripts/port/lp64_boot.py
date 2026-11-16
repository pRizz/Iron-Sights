"""Boot a 64-bit executable to its expected asset-class stop (LP64-003 Task 4).

Until LP64-004 converts a class of ROM data, a 64-bit build stops at the first in-place conversion
the boot reaches, logging `ge: lp64: stopping: asset class "NAME" ...` and exiting with status 5
(port/src/port_lp64.c). This runs each executable once, with the ROM, and passes only if every
run exited with status 5, logged the startup address line, stopped at the expected class, and
logged no fatal signal and no sanitizer report.

    python3 scripts/port/lp64_boot.py --rom ROM --expect-stop "animation tables" EXECUTABLE...

Each run gets its own temporary directory under the system temp folder (the executable is copied
there and runs there), GE_FRAMES=120, GE_SEED=1, GE_NOINISAVE=1, GE_ROM, and with --stage
GE_STAGE; every other GE_ variable of the caller's environment is dropped. LeakSanitizer is off
(ASAN_OPTIONS=detect_leaks=0): stopping mid-boot leaves the game's allocations unfreed by design.
A run longer than TIMEOUT_SECONDS fails.
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
TAIL_LINES = 20
ADDRESS_LINE = 'ge: lp64: address space OK'
STOP_PATTERN = re.compile(r'ge: lp64: stopping: asset class "([^"]+)" has no 64-bit converter yet \(LP64-004\)')
FATAL_SIGNAL = 'ge: fatal signal'
SANITIZER_MARKERS = ('runtime error:', 'ERROR: AddressSanitizer', 'ERROR: LeakSanitizer',
                     'ERROR: UndefinedBehaviorSanitizer')


@dataclass
class BootResult:
    """What one boot log says."""
    address_ok: bool
    stop_class: str | None
    fatal_signal: bool
    sanitizer_errors: list[str] = field(default_factory=list)


def parse_log(text: str) -> BootResult:
    """Reads a boot log: the address line, the class of the first guard stop, a fatal signal and
    every sanitizer report line."""
    match = STOP_PATTERN.search(text)
    lines = text.splitlines()
    return BootResult(
        address_ok=any(line.startswith(ADDRESS_LINE) for line in lines),
        stop_class=match.group(1) if match else None,
        fatal_signal=any(FATAL_SIGNAL in line for line in lines),
        sanitizer_errors=[line for line in lines if any(marker in line for marker in SANITIZER_MARKERS)],
    )


def problems(status: int | None, result: BootResult, expect_stop: str) -> list[str]:
    """Every way a run differs from a clean stop at EXPECT_STOP; empty when it passes. STATUS is
    None for a run that timed out, negative for one killed by a signal."""
    found = []
    if status is None:
        found.append(f'timed out after {TIMEOUT_SECONDS} s')
    elif status < 0:
        found.append(f'killed by signal {-status}')
    elif status != EXPECTED_STATUS:
        found.append(f'exit status {status}, expected {EXPECTED_STATUS}')
    if not result.address_ok:
        found.append(f'no "{ADDRESS_LINE}" line')
    if result.stop_class is None:
        found.append(f'no asset-class stop, expected "{expect_stop}"')
    elif result.stop_class != expect_stop:
        found.append(f'stopped at "{result.stop_class}", expected "{expect_stop}"')
    if result.fatal_signal:
        found.append('fatal signal')
    if result.sanitizer_errors:
        found.append(f'sanitizer report: {result.sanitizer_errors[0].strip()}')
    return found


def boot_environment(rom: Path, stage: str | None) -> dict[str, str]:
    env = {key: value for key, value in os.environ.items() if not key.startswith('GE_')}
    env.update(GE_ROM=str(rom), GE_FRAMES='120', GE_SEED='1', GE_NOINISAVE='1',
               ASAN_OPTIONS='detect_leaks=0')
    if stage is not None:
        env['GE_STAGE'] = stage
    return env


def run_one(executable: Path, rom: Path, stage: str | None) -> tuple[int | None, str]:
    """Runs EXECUTABLE in a fresh temporary directory; returns its status (None on timeout) and
    its combined output."""
    with tempfile.TemporaryDirectory(prefix='midas-lp64-boot-') as temporary:
        folder = Path(temporary)
        child = folder/executable.name
        shutil.copy2(executable, child)
        log_path = folder/'boot.log'
        with log_path.open('wb') as log:
            try:
                status = subprocess.run([str(child)], cwd=folder, env=boot_environment(rom, stage),
                                        stdout=log, stderr=subprocess.STDOUT,
                                        timeout=TIMEOUT_SECONDS).returncode
            except subprocess.TimeoutExpired:
                status = None
        return status, log_path.read_text(errors='replace')


def check(executable: Path, rom: Path, expect_stop: str, stage: str | None) -> bool:
    """Boots one executable and prints its verdict; True when it stopped cleanly at EXPECT_STOP."""
    label = str(executable) + (f', GE_STAGE={stage}' if stage is not None else '')
    status, text = run_one(executable, rom, stage)
    found = problems(status, parse_log(text), expect_stop)
    if not found:
        print(f'OK: stopped at "{expect_stop}" ({label})', flush=True)
        return True
    print(f'FAIL: {label}: ' + '; '.join(found))
    print(f'--- last {TAIL_LINES} log lines ---')
    print('\n'.join(text.splitlines()[-TAIL_LINES:]), flush=True)
    return False


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--expect-stop', required=True, metavar='NAME', help='the asset class, e.g. "animation tables"')
    parser.add_argument('--stage', metavar='NAME', help='also set GE_STAGE (a mission name such as dam)')
    parser.add_argument('executables', nargs='+', type=Path, metavar='EXECUTABLE')
    options = parser.parse_args(argv)
    rom = options.rom.resolve(strict=True)
    results = [check(exe.resolve(strict=True), rom, options.expect_stop, options.stage)
               for exe in options.executables]
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
