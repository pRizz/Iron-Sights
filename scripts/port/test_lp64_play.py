"""ROM-free tests for lp64_play.py run: each kind of required line, the forbidden lines, the status
and the timeout, through stand-in executables (shell scripts that print a log and exit with a
chosen status), as test_lp64_boot.py does. The options, environment and set sizes are tested in
test_lp64_play_options.py, traces and callers in test_lp64_play_traces.py."""
import contextlib
import io
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_play  # noqa: E402
import lp64_play_run  # noqa: E402

BOOT = ('ge007 pc port: booting (GE_FRAMES=n limits the run)\n'
        'ge: lp64: address space OK: memp arena 0x7f0000000000, executable image 0x555500000000\n')
DEMO_START = 'ge: demo start: stage=33 diff=0 pads=2 totaltime=1200 vsyncs (~20s)\n'
DAM1_START = DEMO_START + 'ge: GE_DEMO direct playback: dam1 (index 0)\n'


def demo_exit(tick: int, reason: str = 'totaltime end (timer/limit)') -> str:
    return f'ge: demo exit: {reason} at tick {tick}, gametime 1300 vsyncs (~21s), detail 514/514\n'


def frames(count: int) -> str:
    return f'ge: frame limit reached: frames={count} gfxtasks={count // 2}\n'


DAM1_GOOD = BOOT + DAM1_START + 'ge: demo tick 1 draws=3 live=01 rec=01\n' + demo_exit(438) + frames(3000)
STAGE_GOOD = (BOOT + 'ge: GE_STAGE direct launch: levelid=33 diff=0\n'
              'ge: load_bg_file levelid=33 stan=Tbg_dam_all_p_stanZ file=0x1 record=0x2 first=0x3\n'
              + frames(1800))
STOP_CITADEL = 'ge: FATAL: The setup file UsetupcatZ did not load from the ROM; a 64-bit build cannot read it\n'
TOUR_ARCHIVES = (BOOT + 'ge: GE_STAGE direct launch: levelid=24 diff=0\n'
                 'ge: load_bg_file levelid=24 stan=Tbg_arch_all_p_stanZ file=0x1 record=0x2 first=0x3\n')


class StandIns(unittest.TestCase):
    """A temporary folder with a stand-in ROM, stand-in executables and an output folder."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.rom = self.folder/'rom.z64'
        self.rom.write_bytes(b'not a rom')
        self.output = self.folder/'out'

    def fake(self, log: str, status: int = 0, script: str = '') -> Path:
        """A stand-in executable: runs SCRIPT, prints LOG to stderr, exits with STATUS."""
        log_path = self.folder/f'canned{len(list(self.folder.iterdir()))}.log'
        log_path.write_text(log)
        path = self.folder/f'IronSights{len(list(self.folder.iterdir()))}'
        path.write_text(f'#!/bin/sh\n{script}\ncat "{log_path}" >&2\nexit {status}\n')
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def play(self, executable: Path, set_name: str, only: str, *extra: str) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            try:
                status = lp64_play.main(['run', '--rom', str(self.rom), '--set', set_name, '--only', only,
                                         '--jobs', '2', *extra, '--output', str(self.output), str(executable)])
            except SystemExit as stop:
                status = stop.code
        return status, out.getvalue()


class Demos(StandIns):
    def test_start_exit_at_the_measured_tick_and_frame_limit_passes(self):
        # Arrange
        exe = self.fake(DAM1_GOOD)

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('dam1: PASS', out)
        self.assertIn('OK: 1 of 1 cases', out)

    def test_exit_at_another_tick_fails(self):
        # Arrange
        exe = self.fake(BOOT + DAM1_START + demo_exit(437) + frames(3000))

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (ge: demo exit: totaltime end (timer/limit) at tick 437,: expected 438)', out)
        self.assertIn('FAIL: 1 of 1 cases', out)

    def test_exit_for_another_reason_fails_naming_it(self):
        # Arrange
        exe = self.fake(BOOT + DAM1_START + demo_exit(217, 'RANDSEED desync (recorded/live)') + frames(3000))

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('the log has: ge: demo exit: RANDSEED desync', out)

    def test_no_demo_start_fails_though_it_exits_zero(self):
        # Arrange
        exe = self.fake(BOOT + demo_exit(438) + frames(3000))

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (missing line: ge: demo start:)', out)

    def test_assertion_fault_fails(self):
        # Arrange
        exe = self.fake(DAM1_GOOD + '\n--- ASSERTION FAULT - samples >= 0 - env.c, line 104\n')

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (--- ASSERTION FAULT - samples >= 0', out)

    def test_frame_limit_at_another_count_fails(self):
        # Arrange
        exe = self.fake(BOOT + DAM1_START + demo_exit(438) + frames(600))

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('frames=600: expected 3000', out)

    def test_first_exit_a_checksum_mismatch_fails_though_a_later_demo_ends_at_the_tick(self):
        # Arrange
        later = DEMO_START + 'ge: GE_DEMO direct playback: runway2 (index 6)\n' + demo_exit(438)
        exe = self.fake(BOOT + DAM1_START + demo_exit(201, 'CHECKSUM mismatch (stored/computed)') + later
                        + frames(3000))

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('the log has: ge: demo exit: CHECKSUM mismatch (stored/computed) at tick 201', out)

    def test_first_playback_of_another_demo_fails(self):
        # Arrange
        exe = self.fake(BOOT + DEMO_START + 'ge: GE_DEMO direct playback: dam2 (index 1)\n' + demo_exit(438)
                        + frames(3000))

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (missing line: ge: GE_DEMO direct playback: dam1 (index )', out)


class ForbiddenLines(StandIns):
    def test_sanitizer_report_fails(self):
        # Arrange
        exe = self.fake('src/str.c:17:9: runtime error: store to address\n' + DAM1_GOOD)

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (src/str.c:17:9: runtime error', out)

    def test_fatal_signal_fails(self):
        # Arrange
        exe = self.fake(DAM1_GOOD + 'ge: fatal signal 11, backtrace (9 frames):\n')

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (ge: fatal signal 11', out)

    def test_failing_lp64_line_fails(self):
        # Arrange
        exe = self.fake(DAM1_GOOD + 'ge: lp64: pool "music heap" exhausted\n')

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (ge: lp64: pool "music heap" exhausted)', out)

    def test_informational_lp64_line_passes(self):
        # Arrange
        exe = self.fake('ge: lp64: audio bank "sfx": 1234 bytes\n' + DAM1_GOOD)

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 0, out)

    def test_timeout_fails_with_the_last_line(self):
        # Arrange
        exe = self.fake('', script='echo "ge: still loading" >&2; sleep 10')

        # Act
        with mock.patch.object(lp64_play_run, 'timeout_seconds', return_value=1):
            status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (timed out after 1 s; last line: ge: still loading)', out)


class OtherSets(StandIns):
    def test_stage_with_launch_background_and_frame_limit_passes(self):
        # Arrange
        exe = self.fake(STAGE_GOOD)

        # Act
        status, out = self.play(exe, 'stages', 'stage-dam')

        # Assert
        self.assertEqual(status, 0, out)

    def test_stage_without_the_direct_launch_line_fails(self):
        # Arrange
        exe = self.fake(BOOT + frames(1800))

        # Act
        status, out = self.play(exe, 'stages', 'stage-dam')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (missing line: ge: GE_STAGE direct launch:)', out)

    def test_arena_without_its_launch_line_fails(self):
        # Arrange
        exe = self.fake(STAGE_GOOD)

        # Act
        status, out = self.play(exe, 'stages', 'mp-temple-4p-s3')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('mp-temple-4p-s3: FAIL (missing line: ge: GE_MP direct launch:)', out)

    def test_stop_with_status_one_and_its_fatal_line_passes(self):
        # Arrange
        exe = self.fake(BOOT + STOP_CITADEL, status=1)

        # Act
        status, out = self.play(exe, 'stops', 'stop-citadel')

        # Assert
        self.assertEqual(status, 0, out)

    def test_stop_with_status_zero_fails(self):
        # Arrange
        exe = self.fake(BOOT + STOP_CITADEL, status=0)

        # Act
        status, out = self.play(exe, 'stops', 'stop-citadel')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stop-citadel: FAIL (exit status 0, expected 1)', out)

    def test_stop_naming_another_file_fails(self):
        # Arrange
        exe = self.fake(BOOT + STOP_CITADEL.replace('UsetupcatZ', 'UsetuprefZ'), status=1)

        # Act
        status, out = self.play(exe, 'stops', 'stop-citadel')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('the log has: ge: FATAL: The setup file UsetuprefZ', out)

    def test_setup_stop_through_a_registered_name_fails(self):
        # Arrange: the stop a dangling host answer used to reach (romdata.c's message)
        exe = self.fake(BOOT + 'ge: FATAL: The setup file UsetupcatZ is not served from the ROM; a 64-bit build '
                        'has no compiled-in copy to read\n', status=1)

        # Act
        status, out = self.play(exe, 'stops', 'stop-citadel')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stop-citadel: FAIL (missing line: ge: FATAL: The setup file UsetupcatZ did not load', out)

    def test_menu_without_the_harness_line_fails(self):
        # Arrange
        exe = self.fake(BOOT + frames(900))

        # Act
        status, out = self.play(exe, 'menus', 'menu-select')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('menu-select: FAIL (missing line: ge: GE_MENU harness: menu=)', out)

    def test_tour_short_of_its_rooms_fails(self):
        # Arrange
        exe = self.fake(TOUR_ARCHIVES + 'ge: loadtour: done, 5 of 59 rooms (the mission ended there)\n')

        # Act
        status, out = self.play(exe, 'tours', 'tour-archives')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('tour-archives: FAIL (ge: loadtour: done, 5 of 59 rooms: expected at least 55)', out)

    def test_tour_with_its_rooms_passes(self):
        # Arrange
        exe = self.fake(TOUR_ARCHIVES + 'ge: loadtour: done, 55 of 59 rooms (the mission ended there)\n')

        # Act
        status, out = self.play(exe, 'tours', 'tour-archives')

        # Assert
        self.assertEqual(status, 0, out)

    def test_attract_with_one_demo_fails(self):
        # Arrange
        exe = self.fake(BOOT + DEMO_START + demo_exit(438) + frames(30000))

        # Act
        status, out = self.play(exe, 'attract', 'attract')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('attract: FAIL (missing line: ge: demo start: (twice) (1 of 2))', out)

    def test_attract_with_two_demos_passes(self):
        # Arrange
        exe = self.fake(BOOT + (DEMO_START + demo_exit(438)) * 2 + frames(30000))

        # Act
        status, out = self.play(exe, 'attract', 'attract')

        # Assert
        self.assertEqual(status, 0, out)

    def test_attract_with_a_checksum_exit_in_the_loop_fails(self):
        # Arrange
        exe = self.fake(BOOT + (DEMO_START + demo_exit(438)) * 2 + DEMO_START
                        + demo_exit(90, 'CHECKSUM mismatch (stored/computed)') + frames(30000))

        # Act
        status, out = self.play(exe, 'attract', 'attract')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('attract: FAIL (ge: demo exit: CHECKSUM mismatch', out)

    def test_attract_with_one_timed_end_and_one_clean_end_fails(self):
        # Arrange
        exe = self.fake(BOOT + DEMO_START + demo_exit(438) + DEMO_START
                        + demo_exit(455, 'clean end (terminator block)') + frames(30000))

        # Act
        status, out = self.play(exe, 'attract', 'attract')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('attract: FAIL (ge: demo exit: clean end (terminator block)', out)

    def test_soak_without_its_completion_line_fails(self):
        # Arrange
        exe = self.fake(BOOT + frames(30000))

        # Act
        status, out = self.play(exe, 'attract', 'soak')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('soak: FAIL (missing line: ge: soak: complete after 1 laps)', out)

    def test_input_case_without_its_play_line_fails_though_it_exits_zero(self):
        # Arrange: the abort route's stage loads and the title comes back, but Dam never unloads
        exe = self.fake(BOOT + 'ge: play: tick 5\nge: play: stage-load 33\n'
                        'ge: play: tick 1430\nge: play: stage-load 90\nge: play: tick 1550\nge: play: menu 13\n'
                        + frames(2400))

        # Act
        status, out = self.play(exe, 'input', 'abort')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('abort: FAIL (missing line: ge: play: stage-unload 33)', out)


if __name__ == '__main__':
    unittest.main()
