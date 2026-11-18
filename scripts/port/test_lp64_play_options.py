"""ROM-free tests for lp64_play.py run's options: the environment a case gets, the refused
variables, --only and --set, play.json and the sets' sizes, through the stand-in executables of
test_lp64_play.py."""
import contextlib
import io
import json
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_play  # noqa: E402
import lp64_play_cases  # noqa: E402
from test_lp64_play import BOOT, DAM1_GOOD, STAGE_GOOD, StandIns, frames  # noqa: E402


class Environment(StandIns):
    def test_env_ge_nosleep_is_refused(self):
        # Arrange
        exe = self.fake(DAM1_GOOD)

        # Act
        status, out = self.play(exe, 'demos', 'dam1', '--env', 'GE_NOSLEEP=1')

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('--env GE_NOSLEEP is refused', out)
        self.assertFalse(self.output.exists())

    def test_env_ge_fixedtick_is_refused(self):
        # Arrange
        exe = self.fake(DAM1_GOOD)

        # Act
        status, out = self.play(exe, 'demos', 'dam1', '--env', 'GE_FIXEDTICK=1')

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('--env GE_FIXEDTICK is refused', out)

    def test_caller_ge_variables_are_dropped_and_the_case_gets_its_own(self):
        # Arrange
        exe = self.fake(DAM1_GOOD, script='env | grep -E "^(GE_|ASAN_OPTIONS)" | sort >&2')

        # Act
        with mock.patch.dict('os.environ', GE_NOSLEEP='1', GE_STAGE='dam'):
            status, out = self.play(exe, 'demos', 'dam1', '--env', 'GE_EXTRA=yes')

        # Assert
        log = (self.output/'dam1.log').read_text()
        self.assertEqual(status, 0, out)
        self.assertEqual([line for line in log.splitlines() if line.startswith(('GE_', 'ASAN'))],
                         ['ASAN_OPTIONS=detect_leaks=0', 'GE_DEMO=dam1', 'GE_DEMODIAG=1', 'GE_EXTRA=yes',
                          'GE_FRAMES=3000', 'GE_NOINISAVE=1', f'GE_ROM={self.rom.resolve()}', 'GE_SEED=1'])

    def test_play_json_records_each_case(self):
        # Arrange
        exe = self.fake(BOOT + frames(3000))

        # Act
        self.play(exe, 'demos', 'dam1,dam2')

        # Assert
        record = json.loads((self.output/'play.json').read_text())
        self.assertEqual([(case['name'], case['passed'], case['status'], case['problems'][0])
                          for case in record['cases']],
                         [('dam1', False, 0, 'missing line: ge: demo start:'),
                          ('dam2', False, 0, 'missing line: ge: demo start:')])

    def test_unknown_case_is_refused(self):
        # Arrange
        exe = self.fake(DAM1_GOOD)

        # Act
        status, out = self.play(exe, 'demos', 'dam3')

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('--only names no case of the sets: dam3', out)

    def test_bare_name_selects_the_one_case_it_names(self):
        # Arrange
        exe = self.fake(STAGE_GOOD)

        # Act
        status, out = self.play(exe, 'stages', 'dam')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('stage-dam: PASS', out)
        self.assertIn('OK: 1 of 1 cases', out)

    def test_full_name_still_selects_its_case(self):
        # Arrange
        exe = self.fake(STAGE_GOOD)

        # Act
        status, out = self.play(exe, 'stages', 'stage-dam')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('stage-dam: PASS', out)

    def test_bare_name_matching_two_cases_is_refused_naming_them(self):
        # Arrange
        exe = self.fake(STAGE_GOOD)

        # Act
        status, out = self.play(exe, 'demos,stages', 'train')

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('--only train names more than one case: train, stage-train', out)
        self.assertFalse(self.output.exists())

    def test_empty_set_list_is_refused(self):
        # Arrange
        exe = self.fake(DAM1_GOOD)
        out = io.StringIO()

        # Act
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            with self.assertRaises(SystemExit) as stop:
                lp64_play.main(['run', '--rom', str(self.rom), '--set', ',', '--output', str(self.output), str(exe)])

        # Assert
        self.assertEqual(stop.exception.code, 2, out.getvalue())
        self.assertIn('--set names no set', out.getvalue())


class SetSizes(unittest.TestCase):
    def test_each_set_has_the_planned_number_of_cases(self):
        # Act
        sizes = {name: len(build()) for name, build in lp64_play_cases.SETS.items()}

        # Assert
        self.assertEqual(sizes, {'demos': 14, 'stages': 54, 'stops': 12, 'menus': 7, 'tours': 26, 'attract': 4})

    def test_case_names_are_unique(self):
        # Act
        names = [case.name for case in lp64_play_cases.cases_of(['demos', 'stages', 'stops', 'menus', 'tours', 'attract'])]

        # Assert
        self.assertEqual(len(names), len(set(names)))


if __name__ == '__main__':
    unittest.main()
