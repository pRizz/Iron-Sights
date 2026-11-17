"""ROM-free tests for lp64_boot.py: the log parser on sample logs, and the command line against
stand-in executables (shell scripts that print a log and exit with a chosen status)."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import stat
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('lp64_boot', Path(__file__).with_name('lp64_boot.py'))
boot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(boot)

ADDRESS = 'ge: lp64: address space OK: memp arena 0xffff90e00000, ROM image 0xffff8ce00000\n'
BOOT = 'ge007 pc port: booting (GE_FRAMES=n limits the run)\nge: romdata: texture table: 3000 entries\n'


def stop(name: str) -> str:
    return f'ge: lp64: stopping: asset class "{name}" has no 64-bit converter yet (LP64-004)\n'


CLEAN = BOOT + ADDRESS + stop('animation tables')
OTHER_CLASS = BOOT + ADDRESS + 'ge: romdata: text LtitleE  12 slots from the ROM\n' + stop('text banks')
FATAL = (BOOT + ADDRESS + '\nge: fatal signal 11, backtrace (9 frames):\n'
         './IronSights(modelAnimReadRootMotionValue+0x40)[0xaaaa00001234]\n')
SANITIZED = (BOOT + ADDRESS + 'src/game/initanitable.c:241:9: runtime error: member access within misaligned address\n'
             + stop('animation tables'))
POOL = 'ge: lp64: pool "music heap" exhausted\n'
REFUSED = 'ge: lp64: cannot convert models "GtriggerZ": needs 46000 bytes, has 45008\n'
RAN = BOOT + ADDRESS + 'ge: frame limit reached: frames=600 gfxtasks=600\n'


class ParseLog(unittest.TestCase):
    def test_clean_stop_names_its_class(self):
        # Act
        result = boot.parse_log(CLEAN)

        # Assert
        self.assertEqual((result.address_ok, result.stop_class, result.fatal_signal, result.sanitizer_errors),
                         (True, 'animation tables', False, []))

    def test_stop_at_another_class_is_reported_as_that_class(self):
        # Act
        result = boot.parse_log(OTHER_CLASS)

        # Assert
        self.assertEqual(result.stop_class, 'text banks')

    def test_fatal_signal_is_seen_and_there_is_no_stop(self):
        # Act
        result = boot.parse_log(FATAL)

        # Assert
        self.assertEqual((result.fatal_signal, result.stop_class), (True, None))

    def test_sanitizer_report_before_the_stop_is_listed(self):
        # Act
        result = boot.parse_log(SANITIZED)

        # Assert
        self.assertEqual(len(result.sanitizer_errors), 1)
        self.assertIn('initanitable.c:241', result.sanitizer_errors[0])

    def test_log_without_the_address_line_is_not_address_ok(self):
        # Act
        result = boot.parse_log(BOOT + stop('animation tables'))

        # Assert
        self.assertFalse(result.address_ok)

    def test_address_failure_line_is_not_address_ok(self):
        # Arrange
        text = BOOT + 'ge: lp64: executable image at 0x400000 is below 4 GB; a 64-bit build needs ...\n'

        # Act
        result = boot.parse_log(text)

        # Assert
        self.assertFalse(result.address_ok)


class StandIns(unittest.TestCase):
    """A temporary folder with a stand-in ROM, and stand-in executables made in it."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.rom = self.folder/'rom.z64'
        self.rom.write_bytes(b'not a rom')

    def fake(self, name: str, log: str, status: int, script: str = '') -> Path:
        """A stand-in executable: runs SCRIPT, prints LOG to stderr, exits with STATUS."""
        (self.folder/(name + '.log')).write_text(log)
        path = self.folder/name
        path.write_text(f'#!/bin/sh\n{script}\ncat "{self.folder/(name + ".log")}" >&2\nexit {status}\n')
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def run_main(self, *args: str) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = boot.main(['--rom', str(self.rom), '--expect-stop', 'animation tables', *args])
        return status, out.getvalue()


class CommandLine(StandIns):
    def test_clean_stop_with_status_five_passes(self):
        # Arrange
        exe = self.fake('good', CLEAN, 5)

        # Act
        status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: stopped at "animation tables"', out)

    def test_crash_status_139_fails(self):
        # Arrange
        exe = self.fake('crash', FATAL, 139)

        # Act
        status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('exit status 139', out)

    def test_stop_at_another_class_fails(self):
        # Arrange
        exe = self.fake('other', OTHER_CLASS, 5)

        # Act
        status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stopped at "text banks", expected "animation tables"', out)

    def test_missing_address_line_fails(self):
        # Arrange
        exe = self.fake('noaddress', BOOT + stop('animation tables'), 5)

        # Act
        status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('no "ge: lp64: address space OK" line', out)

    def test_sanitizer_report_fails_even_with_the_right_stop(self):
        # Arrange
        exe = self.fake('sanitized', SANITIZED, 5)

        # Act
        status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('sanitizer report', out)

    def test_fatal_signal_line_fails_even_with_status_five(self):
        # Arrange
        exe = self.fake('signalled', FATAL + stop('animation tables'), 5)

        # Act
        status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('fatal signal', out)

    def test_run_past_the_timeout_fails(self):
        # Arrange
        exe = self.fake('hang', CLEAN, 5, script='sleep 5')

        # Act
        with mock.patch.object(boot, 'TIMEOUT_SECONDS', 1):
            status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('timed out after 1 s', out)

    def test_failure_prints_the_last_twenty_log_lines(self):
        # Arrange
        lines = ''.join(f'line {n}\n' for n in range(40))
        exe = self.fake('long', BOOT + ADDRESS + lines, 139)

        # Act
        status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('line 39', out)
        self.assertIn('line 20', out)
        self.assertNotIn('line 19\n', out)

    def test_one_failing_executable_fails_the_run(self):
        # Arrange
        good = self.fake('good', CLEAN, 5)
        crash = self.fake('crash', FATAL, 139)

        # Act
        status, out = self.run_main(str(good), str(crash))

        # Assert
        self.assertEqual(status, 1, out)

    def test_runs_with_the_boot_environment_outside_the_checkout(self):
        # Arrange: the stand-in stops with status 3 unless each variable is as the tool must set it.
        check = ('[ "$GE_FRAMES" = 120 ] && [ "$GE_SEED" = 1 ] && [ "$GE_NOINISAVE" = 1 ] '
                 f'&& [ "$GE_ROM" = "{self.rom.resolve()}" ] && [ -z "$GE_STAGE" ] '
                 f'&& [ "$(pwd -P)" != "{boot.ROOT}" ] || exit 3')
        exe = self.fake('env', CLEAN, 5, script=check)

        # Act
        with mock.patch.dict(os.environ, {'GE_STAGE': 'leaked'}):
            status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 0, out)

    def test_stage_option_sets_ge_stage(self):
        # Arrange
        exe = self.fake('stage', CLEAN, 5, script='[ "$GE_STAGE" = dam ] || exit 3')

        # Act
        status, out = self.run_main('--stage', 'dam', str(exe))

        # Assert
        self.assertEqual(status, 0, out)

    def test_pool_line_fails_even_with_the_right_stop(self):
        # Arrange
        exe = self.fake('pool', BOOT + ADDRESS + POOL + stop('animation tables'), 5)

        # Act
        status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('ge: lp64: pool "music heap" exhausted', out)

    def test_refused_conversion_fails_even_with_the_right_stop(self):
        # Arrange
        exe = self.fake('refused', BOOT + ADDRESS + REFUSED + stop('animation tables'), 5)

        # Act
        status, out = self.run_main(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('ge: lp64: cannot convert models', out)


class RunMode(StandIns):
    """--expect-run: the executable runs its frames and exits 0 with no stop or refusal."""

    def run_mode(self, *args: str) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = boot.main(['--rom', str(self.rom), '--expect-run', *args])
        return status, out.getvalue()

    def test_clean_run_with_status_zero_passes(self):
        # Arrange
        exe = self.fake('ran', RAN, 0)

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn(f'OK: ran 600 frames ({exe.resolve()})', out)

    def test_exit_zero_without_the_frame_limit_line_fails(self):
        # Arrange: a run that quits early, cleanly, before its frames are up.
        exe = self.fake('early', BOOT + ADDRESS, 0)

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('no "ge: frame limit reached: frames=600" line', out)

    def test_frame_limit_line_with_another_count_fails(self):
        # Arrange
        exe = self.fake('short', BOOT + ADDRESS + 'ge: frame limit reached: frames=120 gfxtasks=120\n', 0)

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('ran 120 frames, expected 600', out)

    def test_stop_at_a_class_fails(self):
        # Arrange
        exe = self.fake('stopped', CLEAN, 0)

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stopped at "animation tables", expected to run', out)

    def test_pool_line_fails(self):
        # Arrange
        exe = self.fake('pool', RAN + POOL, 0)

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('ge: lp64: pool "music heap" exhausted', out)

    def test_refused_conversion_fails(self):
        # Arrange
        exe = self.fake('refused', RAN + REFUSED, 0)

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('ge: lp64: cannot convert models', out)

    def test_exit_status_five_fails(self):
        # Arrange
        exe = self.fake('five', RAN, 5)

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('exit status 5, expected 0', out)

    def test_missing_address_line_fails(self):
        # Arrange
        exe = self.fake('noaddress', BOOT, 0)

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('no "ge: lp64: address space OK" line', out)

    def test_fatal_signal_line_fails(self):
        # Arrange
        exe = self.fake('signalled', FATAL, 0)

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('fatal signal', out)

    def test_sanitizer_report_fails(self):
        # Arrange
        exe = self.fake('sanitized', RAN + 'src/game/bg.c:10:2: runtime error: load of misaligned address\n', 0)

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('sanitizer report', out)

    def test_frames_default_to_600(self):
        # Arrange
        exe = self.fake('frames', RAN, 0, script='[ "$GE_FRAMES" = 600 ] || exit 3')

        # Act
        status, out = self.run_mode(str(exe))

        # Assert
        self.assertEqual(status, 0, out)

    def test_frames_option_sets_ge_frames_and_the_verdict(self):
        # Arrange
        exe = self.fake('frames', BOOT + ADDRESS + 'ge: frame limit reached: frames=90 gfxtasks=90\n', 0,
                        script='[ "$GE_FRAMES" = 90 ] || exit 3')

        # Act
        status, out = self.run_mode('--frames', '90', str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: ran 90 frames', out)

    def test_expect_run_with_expect_stop_is_refused(self):
        # Arrange
        exe = self.fake('both', BOOT + ADDRESS, 0)

        # Act
        with self.assertRaises(SystemExit) as raised:
            self.run_mode('--expect-stop', 'animation tables', str(exe))

        # Assert
        self.assertEqual(raised.exception.code, 2)

    def test_frames_with_expect_stop_is_refused(self):
        # Arrange
        exe = self.fake('stopframes', CLEAN, 5)

        # Act
        with self.assertRaises(SystemExit) as raised:
            self.run_main('--frames', '90', str(exe))

        # Assert
        self.assertEqual(raised.exception.code, 2)


if __name__ == '__main__':
    unittest.main()
