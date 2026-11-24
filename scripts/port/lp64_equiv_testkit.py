"""Shared stand-ins for the tests of lp64_equiv.py (LP64-006): the canned logs and traces, a
recording builder and StandIns, a temporary folder with stand-in executables (shell scripts that
print a log, write the simulation trace GE_SIMTRACE names and exit), as test_lp64_play.py uses.
Imported by test_lp64_equiv.py, test_lp64_equiv_video.py and test_lp64_equiv_cases.py; it holds no
test of its own."""
import contextlib
import io
import json
from pathlib import Path
import stat
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_equiv  # noqa: E402

COMPARE_ON = 'ge: compare: on\n'
COMPARE_LIBM = 'ge: compare: on (libm)\n'
LOCKSTEP_ON = 'ge: lockstep: on\n'
DAM_LAUNCH = ('ge: GE_STAGE direct launch: levelid=33 diff=0\n'
              'ge: load_bg_file levelid=33 stan=Tbg_dam_all_p_stanZ file=0x1 record=0x2 first=0x3\n')
MOVED = 'ge: play: tick 600\nge: play: moved 102\n'
SHOTS = ''.join(f'ge: play: tick {640 + 120 * shot}\nge: play: shots {shot}\n' for shot in range(1, 31))
FRAMES = 'ge: frame limit reached: frames=3900 gfxtasks=3897\n'
DAM_GOOD = COMPARE_ON + LOCKSTEP_ON + DAM_LAUNCH + MOVED + SHOTS + FRAMES
DAM1_GOOD = (COMPARE_ON + 'ge: demo start: stage=33 diff=0 pads=2 totaltime=1200 vsyncs (~20s)\n'
             'ge: GE_DEMO direct playback: dam1 (index 0)\n'
             'ge: demo exit: totaltime end (timer/limit) at tick 438, gametime 1300 vsyncs (~21s), detail 514/514\n'
             'ge: frame limit reached: frames=3000 gfxtasks=1500\n')


AUDIO_LOCKED = 'ge: audio: locked to game fields\n'


def wav(*samples: tuple[int, int]) -> bytes:
    """A 22,050 Hz stereo recording of SAMPLES (left, right), as port_audio_capture.c writes one."""
    data = b''.join(struct.pack('<hh', left, right) for left, right in samples)
    return (b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVEfmt '
            + struct.pack('<IHHIIHH', 16, 1, 2, 22050, 22050 * 4, 4, 16) + b'data' + struct.pack('<I', len(data))
            + data)


def sim_line(tick: int, seed: str = '0000000000000001') -> str:
    return (f'T n={tick} timer={tick} seed={seed} draws=0 players=1 '
            'p0=00000000,00000000,00000000,00000000,00000000 chrs=0:00000000 props=0:00000000 rooms=0:00000000\n')


def demo_sim(first_n: int, timer_shift: int = 0, demo_ticks: int = 3) -> str:
    """A paced demo's trace: lines before the replay from 1 to FIRST_N - 1, then DEMO_TICKS replay
    lines (demo=1, 2, ...), then one after it; every timer shifted by TIMER_SHIFT."""
    lines = [sim_line(n).replace(f'timer={n} ', f'timer={n + timer_shift} ') for n in range(1, first_n + demo_ticks + 1)]
    for tick in range(1, demo_ticks + 1):
        lines[first_n - 2 + tick] = lines[first_n - 2 + tick].replace('\n', f' demo={tick}\n')
    return ''.join(lines)


def sim(ticks: int = 3, seed_at: dict[int, str] | None = None) -> str:
    seeds = seed_at or {}
    return ''.join(sim_line(tick, seeds.get(tick, '0000000000000001')) for tick in range(1, ticks + 1))


class StandIns(unittest.TestCase):
    """A temporary folder with a stand-in ROM, stand-in executables and an output folder."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.rom = self.folder/'rom.z64'
        self.rom.write_bytes(b'not a rom')
        self.output = self.folder/'out'

    def canned(self, name: str, text: str) -> Path:
        path = self.folder/name
        path.write_text(text)
        return path

    def fake(self, log: str, trace: str = sim(), script: str = '') -> Path:
        """A stand-in executable: runs SCRIPT, writes TRACE to $GE_SIMTRACE, prints LOG, exits 0."""
        number = len(list(self.folder.iterdir()))
        log_path = self.canned(f'canned{number}.log', log)
        trace_path = self.canned(f'canned{number}.sim', trace)
        path = self.folder/f'IronSights{number}'
        path.write_text(f'#!/bin/sh\n{script}\n[ -n "$GE_SIMTRACE" ] && cp "{trace_path}" "$GE_SIMTRACE"\n'
                        f'cat "{log_path}" >&2\nexit 0\n')
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def equiv(self, *argv: str) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            try:
                status = lp64_equiv.main(list(argv))
            except SystemExit as stop:
                status = stop.code
        return status, out.getvalue()

    def run_dam(self, executable: Path, *extra: str) -> tuple[int, str]:
        return self.equiv('run', '--rom', str(self.rom), '--set', 'solo', '--only', 'dam', '--jobs', '2', *extra,
                          '--output', str(self.output), str(executable))


AGREED = {'ran': True, 'agreed': True}


class RunFolders(StandIns):
    """StandIns that also write run folders by hand, as `run` leaves them, for compare."""

    def run_folder(self, label: str, trace: str, case: str = 'stage-dam', profile: str = 'default',
                   **record: object) -> Path:
        """A run folder holding CASE's trace in run1 and a record of two runs that agreed, changed by RECORD."""
        folder = self.folder/label
        (folder/'run1').mkdir(parents=True)
        (folder/'run1'/f'{case}.sim').write_text(trace)
        written = {'profile': profile, 'video': False, 'repeat': 2, 'cases': [case], 'verdicts': {case: AGREED}}
        written.update(record)
        (folder/lp64_equiv.RECORD).write_text(json.dumps({key: value for key, value in written.items()
                                                          if value is not None}))
        return folder
