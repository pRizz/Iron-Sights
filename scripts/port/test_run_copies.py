"""ROM-free test that concurrent case starts in lp64_play_run.py and audit_buffers.py cannot fail with
ETXTBSY ("Text file busy"), the race run_copies.py removes.

The race needs a thread to hold an executable copy open for writing while another thread's forked
child has not yet exec'ed. The test makes that overlap certain instead of waiting for it by chance:
every first child started through subprocess.Popen sleeps SPAWN_HOLD seconds between fork and exec (a
preexec_fn), so it keeps duplicates of the parent's descriptors that long, and the first executable
copy the tool makes stays open for writing until another thread has entered Popen, plus HOLD_AFTER
seconds. A tool that copies per case inside its thread pool then execs its copy while the other
case's child still holds a writer, and fails with ETXTBSY. A tool that copies once before the pool
makes no copy while a child is forking: the first copy's wait times out (no Popen has started) and
every case passes, whatever the timing.

ETXTBSY is Linux's (and the containers' and CI's): on macOS an exec of a file open for writing
succeeds, so there the test passes against either version and discriminates nothing.
"""
from concurrent.futures import ThreadPoolExecutor
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock
import warnings

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audit_buffers  # noqa: E402
import lp64_play  # noqa: E402
from run_copies import SharedExecutable  # noqa: E402

SPAWN_HOLD = 1.5
HOLD_AFTER = 0.3
FIRST_COPY_WAIT = 2.0


class HeldCopies:
    """Patches shutil.copy2 and subprocess.Popen as the module docstring says, and records what
    happened: EVENTS in order ('copy' when a copy is finished and closed, 'popen' when a start
    begins), and OVERLAPPED when a start began while the first copy was still open for writing."""

    def __init__(self):
        self.spawning = threading.Event()
        self.copies = 0
        self.events = []
        self.overlapped = False
        self.lock = threading.Lock()
        self.real_copy, self.real_popen = shutil.copy2, subprocess.Popen

    def copy2(self, source, target, *args, **kwargs):
        result = self.real_copy(source, target, *args, **kwargs)
        with self.lock:
            self.copies += 1
            first = self.copies == 1
        if first:
            with open(target, 'ab'):
                if self.spawning.wait(FIRST_COPY_WAIT):
                    self.overlapped = True
                    time.sleep(HOLD_AFTER)
        with self.lock:
            self.events.append('copy')
        return result

    def popen(self, *args, **kwargs):
        with self.lock:
            first = not self.spawning.is_set()
            self.spawning.set()
            self.events.append('popen')
        if first:
            kwargs['preexec_fn'] = lambda: time.sleep(SPAWN_HOLD)
        return self.real_popen(*args, **kwargs)

    def __enter__(self):
        self.patches = [mock.patch.object(shutil, 'copy2', self.copy2),
                        mock.patch.object(subprocess, 'Popen', self.popen)]
        for patch in self.patches:
            patch.start()
        return self

    def __exit__(self, *exc):
        for patch in self.patches:
            patch.stop()


class ConcurrentStarts(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.rom = self.folder/'rom.z64'
        self.rom.write_bytes(b'not a rom')

    def stand_in(self, log: str) -> Path:
        path = self.folder/'IronSights'
        path.write_text(f'#!/bin/sh\nprintf "{log}"\n')
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def assert_one_copy_before_every_start(self, held: HeldCopies, starts: int) -> None:
        """What replaces the race: the tool made one copy, finished before its first start."""
        self.assertEqual(held.copies, 1, held.events)
        self.assertEqual(held.events[0], 'copy', held.events)
        self.assertEqual(held.events.count('popen'), starts, held.events)

    def test_a_copy_per_case_in_a_pool_overlaps_a_start(self):
        # Arrange: the shape the tools had before run_copies.py, each case copying in its own thread
        exe = self.stand_in('started\\n')

        def run_case(index: int) -> None:
            folder = self.folder/f'case{index}'
            folder.mkdir()
            shutil.copy2(exe, folder/exe.name)
            try:
                subprocess.run([str(folder/exe.name)], stdout=subprocess.DEVNULL, check=False)
            except OSError:
                pass  # ETXTBSY on Linux: the race itself, which this test does not need to see

        # Act
        with HeldCopies() as held, ThreadPoolExecutor(2) as pool:
            list(pool.map(run_case, range(2)))

        # Assert: the harness held the first copy open while another case's start began
        self.assertTrue(held.overlapped, held.events)

    def test_play_cases_started_together_all_start(self):
        # Arrange: two stage cases, two at a time
        exe = self.stand_in('ge: GE_STAGE direct launch: levelid=33 diff=0\\n'
                            'ge: load_bg_file levelid=33 stan=x\\nge: frame limit reached: frames=1800 gfxtasks=0\\n')

        # Act
        out = StringIO()
        with HeldCopies() as held, redirect_stdout(out):
            status = lp64_play.main(['run', '--rom', str(self.rom), '--set', 'stages', '--only', 'dam,facility',
                                     '--jobs', '2', '--output', str(self.folder/'out'), str(exe)])

        # Assert
        self.assertEqual(status, 0, out.getvalue())
        self.assert_one_copy_before_every_start(held, 2)

    def test_mission_smokes_started_together_all_start(self):
        # Arrange: two missions, two at a time
        exe = self.stand_in('GE_STAGE direct launch: x\\nframe limit reached: frames=600\\n')

        # Act
        with HeldCopies() as held, mock.patch.object(audit_buffers, 'MISSIONS', ('dam', 'facility')), \
                redirect_stdout(StringIO()):
            status = audit_buffers.mission_smoke(exe, self.rom, self.folder/'smoke.json', 600, 2)

        # Assert
        self.assertEqual(status, 0)
        self.assert_one_copy_before_every_start(held, 2)



class FailedCopy(unittest.TestCase):
    def test_a_copy_that_fails_removes_its_folder_and_raises(self):
        # Arrange
        missing = Path(tempfile.gettempdir())/'midas-no-such-executable'/'IronSights'
        before = set(Path(tempfile.gettempdir()).glob('midas-run-copy-*'))

        # Act
        with warnings.catch_warnings(record=True) as caught:
            warnings.simplefilter('always')
            with self.assertRaises(FileNotFoundError):
                SharedExecutable(missing)

        # Assert: removed by the constructor, not left to the folder's finalizer
        self.assertEqual(set(Path(tempfile.gettempdir()).glob('midas-run-copy-*')), before)
        self.assertEqual([str(w.message) for w in caught if issubclass(w.category, ResourceWarning)], [])

if __name__ == '__main__':
    unittest.main()
