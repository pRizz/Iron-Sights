"""ROM-free tests for lp64_oracle.py: the summing of the oracle's exit lines on sample logs, and the
command line against stand-in executables (shell scripts that print a log and exit with a chosen
status)."""
import contextlib
import importlib.util
import io
from pathlib import Path
import stat
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('lp64_oracle', Path(__file__).with_name('lp64_oracle.py'))
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)

CLASSES = ('text banks', 'fonts', 'textures', 'animation tables', 'models', 'collision tiles',
           'setup files', 'backgrounds', 'audio banks', 'music sequences')


def exit_lines(counts: dict[str, tuple[int, int]]) -> str:
    """The oracle's exit report: one line a class, COUNTS giving (files, differ), zero otherwise."""
    return ''.join(f'ge: lp64: oracle: {name}: {counts.get(name, (0, 0))[0]} files, '
                   f'{counts.get(name, (0, 0))[1]} differ\n' for name in CLASSES)


ALL_IDENTICAL = ('ge: lp64: oracle: animation tables animation_data identical (59360 bytes)\n'
                 + exit_lines({'animation tables': (173, 0), 'text banks': (7, 0)}))
ONE_DIFFERS = ('ge: lp64: oracle: text banks LtitleE DIFFERS at +0x1C (in-place 00, converter 04)\n'
               + exit_lines({'animation tables': (173, 0), 'text banks': (7, 1)}))
NO_FILE = exit_lines({'animation tables': (173, 0)})


class Summing(unittest.TestCase):
    def test_all_identical_log_passes_for_its_classes(self):
        # Arrange
        totals = oracle.add_counts({}, oracle.summarize(ALL_IDENTICAL))

        # Act
        found = oracle.problems(totals, ['animation tables', 'text banks'])

        # Assert
        self.assertEqual((totals['animation tables'], totals['text banks'], found), ((173, 0), (7, 0), []))

    def test_one_differing_file_fails_its_class(self):
        # Arrange
        totals = oracle.add_counts({}, oracle.summarize(ONE_DIFFERS))

        # Act
        found = oracle.problems(totals, ['animation tables', 'text banks'])

        # Assert
        self.assertEqual(found, ['text banks: 1 of 7 files differ'])

    def test_a_class_with_no_file_fails(self):
        # Arrange
        totals = oracle.add_counts({}, oracle.summarize(NO_FILE))

        # Act
        found = oracle.problems(totals, ['animation tables', 'text banks'])

        # Assert
        self.assertEqual(found, ['text banks: no file compared'])

    def test_counts_add_up_over_runs(self):
        # Arrange
        runs = [oracle.summarize(ALL_IDENTICAL), oracle.summarize(ONE_DIFFERS)]

        # Act
        totals = {}
        for run in runs:
            totals = oracle.add_counts(totals, run)

        # Assert
        self.assertEqual(totals['text banks'], (14, 1))

    def test_file_lines_are_not_exit_lines(self):
        # Act
        counts = oracle.summarize('ge: lp64: oracle: models GtriggerZ identical (42648 bytes)\n')

        # Assert
        self.assertEqual(counts, {})


class CommandLine(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.rom = self.folder/'rom.z64'
        self.rom.write_bytes(b'not a rom')

    def fake(self, boot_log: str, mission_log: str, boot_status: int = 0, mission_status: int = 0) -> Path:
        """A stand-in executable. Its boot run (no GE_STAGE) exits 3 unless GE_ROMPROBE=1 and
        GE_LP64_ORACLE_SWEEP=1 are set, then prints BOOT_LOG; a mission run exits 3 if either is
        set, else prints what audit_buffers.py's smoke needs and MISSION_LOG."""
        (self.folder/'boot.txt').write_text(boot_log)
        (self.folder/'mission.txt').write_text(mission_log)
        path = self.folder/'IronSights'
        path.write_text(
            '#!/bin/sh\n'
            'if [ -z "$GE_STAGE" ]; then\n'
            '  [ "$GE_ROMPROBE" = 1 ] && [ "$GE_LP64_ORACLE_SWEEP" = 1 ] || exit 3\n'
            f'  cat "{self.folder/"boot.txt"}" >&2\n'
            f'  exit {boot_status}\n'
            'fi\n'
            '[ -z "$GE_ROMPROBE" ] && [ -z "$GE_LP64_ORACLE_SWEEP" ] || exit 3\n'
            'echo "GE_STAGE direct launch: $GE_STAGE"\n'
            'echo "ge: frame limit reached: frames=$GE_FRAMES gfxtasks=1"\n'
            f'cat "{self.folder/"mission.txt"}" >&2\n'
            f'exit {mission_status}\n')
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def run_main(self, classes: str, exe: Path) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = oracle.main(['--rom', str(self.rom), '--classes', classes, '--jobs', '4',
                                  '--logs', str(self.folder/'logs'), str(exe)])
        return status, out.getvalue()

    def test_clean_class_over_the_boot_and_20_missions_passes(self):
        # Arrange
        exe = self.fake(exit_lines({'text banks': (7, 0)}), exit_lines({'text banks': (2, 0)}))

        # Act
        status, out = self.run_main('text banks', exe)

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('text banks: 47 files, 0 differ', out)

    def test_no_classes_pass_when_every_run_reports(self):
        # Arrange
        exe = self.fake(exit_lines({}), exit_lines({}))

        # Act
        status, out = self.run_main('', exe)

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: oracle', out)

    def test_a_difference_in_one_run_fails(self):
        # Arrange
        exe = self.fake(exit_lines({'text banks': (7, 1)}), exit_lines({'text banks': (2, 0)}))

        # Act
        status, out = self.run_main('text banks', exe)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('text banks: 1 of 47 files differ', out)

    def test_a_named_class_never_compared_fails(self):
        # Arrange
        exe = self.fake(exit_lines({}), exit_lines({}))

        # Act
        status, out = self.run_main('models, text banks', exe)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('models: no file compared', out)
        self.assertIn('text banks: no file compared', out)

    def test_an_executable_without_the_oracle_fails(self):
        # Arrange
        exe = self.fake('ge: romdata: loaded\n', 'ge: romdata: loaded\n')

        # Act
        status, out = self.run_main('', exe)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('boot: no oracle exit lines', out)

    def test_a_failed_boot_fails(self):
        # Arrange
        exe = self.fake(exit_lines({}), exit_lines({}), boot_status=5)

        # Act
        status, out = self.run_main('', exe)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('boot: exit status 5', out)

    def test_a_failed_mission_fails(self):
        # Arrange
        exe = self.fake(exit_lines({}), exit_lines({}), mission_status=1)

        # Act
        status, out = self.run_main('', exe)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam: FAIL', out)

    def test_a_mission_log_the_smoke_never_wrote_fails_by_name(self):
        # Arrange: a smoke that fails before any mission writes its log.
        exe = self.fake(exit_lines({}), exit_lines({}))

        # Act
        with mock.patch.object(oracle.audit_buffers, 'mission_smoke', return_value=1):
            status, out = self.run_main('', exe)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn(f'dam: no log at {self.folder/"logs"/"dam.log"}', out)


if __name__ == '__main__':
    unittest.main()
