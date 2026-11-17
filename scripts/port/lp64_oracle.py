"""Run the converter oracle over a boot and the 20 missions, and sum its verdicts (LP64-004).

The oracle build (32-bit, headless, configured with -DGE_LP64_ORACLE=ON) runs each 64-bit ROM data
converter beside the game's in-place conversion it replaces and logs, at exit, one line a class:
`ge: lp64: oracle: CLASS: N files, M differ` (port/src/port_lp64_oracle.c). This runs that
executable once to boot, with GE_ROMPROBE=1 (the game loads every served text bank, briefing,
setup and stan file at boot) and GE_LP64_ORACLE_SWEEP=1 (the oracle's own sweeps of files no run
loads), for BOOT_FRAMES frames; then the 20 missions of audit_buffers.py's smoke, through that
tool, for MISSION_FRAMES frames each. It sums the exit lines of all 21 runs and passes only if every
run succeeded and reported, and each class named in --classes has N above zero and M zero.

    python3 scripts/port/lp64_oracle.py --rom ROM --classes "animation tables,text banks" EXECUTABLE

--classes "" checks the runs and the report only (no class has a converter yet). The logs go to
--logs (default: an oracle-logs folder beside the executable): boot.log and one per mission.
"""
import argparse
import importlib.util
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

_spec = importlib.util.spec_from_file_location('audit_buffers', Path(__file__).with_name('audit_buffers.py'))
audit_buffers = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(audit_buffers)

BOOT_FRAMES = 600
MISSION_FRAMES = 600
BOOT_TIMEOUT_SECONDS = 300
EXIT_LINE = re.compile(r'^ge: lp64: oracle: (.+): (\d+) files, (\d+) differ$', re.MULTILINE)

Counts = dict[str, tuple[int, int]]


def summarize(text: str) -> Counts:
    """The oracle's exit lines in one log, as {class: (files, differ)}."""
    return {match.group(1): (int(match.group(2)), int(match.group(3))) for match in EXIT_LINE.finditer(text)}


def add_counts(totals: Counts, run: Counts) -> Counts:
    """TOTALS with RUN's counts added, class by class."""
    summed = dict(totals)
    for name, (files, differ) in run.items():
        old_files, old_differ = summed.get(name, (0, 0))
        summed[name] = (old_files + files, old_differ + differ)
    return summed


def problems(totals: Counts, classes: list[str]) -> list[str]:
    """Every named class that compared no file or had a file differ; empty when all are clean."""
    found = []
    for name in classes:
        files, differ = totals.get(name, (0, 0))
        if files == 0:
            found.append(f'{name}: no file compared')
        elif differ != 0:
            found.append(f'{name}: {differ} of {files} files differ')
    return found


def boot_environment(rom: Path) -> dict[str, str]:
    env = {key: value for key, value in os.environ.items() if not key.startswith('GE_')}
    env.update(GE_ROM=str(rom), GE_FRAMES=str(BOOT_FRAMES), GE_SEED='1', GE_NOINISAVE='1',
               GE_ROMPROBE='1', GE_LP64_ORACLE_SWEEP='1')
    return env


def run_boot(executable: Path, rom: Path, log_path: Path) -> int | None:
    """Boots EXECUTABLE in a fresh temporary directory, logging to LOG_PATH; returns its exit status,
    or None when it ran past BOOT_TIMEOUT_SECONDS."""
    with tempfile.TemporaryDirectory(prefix='midas-lp64-oracle-') as temporary:
        folder = Path(temporary)
        child = folder/executable.name
        shutil.copy2(executable, child)
        with log_path.open('wb') as log:
            try:
                return subprocess.run([str(child)], cwd=folder, env=boot_environment(rom), stdout=log,
                                      stderr=subprocess.STDOUT, timeout=BOOT_TIMEOUT_SECONDS).returncode
            except subprocess.TimeoutExpired:
                return None


def collect(label: str, log_path: Path, totals: Counts, failures: list[str]) -> Counts:
    """TOTALS with LOG_PATH's exit lines added; a log that is missing or lacks them is a failure
    named LABEL."""
    if not log_path.is_file():
        failures.append(f'{label}: no log at {log_path}')
        return totals
    run = summarize(log_path.read_text(errors='replace'))
    if not run:
        failures.append(f'{label}: no oracle exit lines in {log_path} (not a -DGE_LP64_ORACLE=ON build?)')
    return add_counts(totals, run)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--classes', required=True, metavar='"A,B"',
                        help='the classes that must be clean, as the oracle names them; "" for none')
    parser.add_argument('--jobs', type=int, choices=range(1, 9), default=2, help='missions run at once')
    parser.add_argument('--logs', type=Path, metavar='DIR', help='where the logs go')
    parser.add_argument('executable', type=Path, metavar='EXECUTABLE')
    options = parser.parse_args(argv)
    executable = options.executable.resolve(strict=True)
    rom = options.rom.resolve(strict=True)
    classes = [name.strip() for name in options.classes.split(',') if name.strip()]
    logs = options.logs if options.logs is not None else executable.parent/'oracle-logs'
    logs.mkdir(parents=True, exist_ok=True)

    failures: list[str] = []
    boot_log = logs/'boot.log'
    status = run_boot(executable, rom, boot_log)
    if status != 0:
        failures.append(f'boot: exit status {status}' if status is not None
                        else f'boot: timed out after {BOOT_TIMEOUT_SECONDS} s')
    totals = collect('boot', boot_log, {}, failures)
    print(f'boot: {"PASS" if status == 0 else "FAIL"}', flush=True)

    if audit_buffers.mission_smoke(executable, rom, logs/'missions.json', MISSION_FRAMES, options.jobs) != 0:
        failures.append('missions: the smoke failed (see each mission\'s line above)')
    for stage in audit_buffers.MISSIONS:
        totals = collect(stage, logs/(stage + '.log'), totals, failures)

    for name in sorted(totals):
        print(f'{name}: {totals[name][0]} files, {totals[name][1]} differ')
    failures += problems(totals, classes)
    if failures:
        print('FAIL: oracle: ' + '; '.join(failures), flush=True)
        return 1
    named = ', '.join(classes) if classes else 'no class named'
    print(f'OK: oracle: boot and {len(audit_buffers.MISSIONS)} missions ran and reported ({named}; logs in {logs})',
          flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
