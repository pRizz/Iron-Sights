"""ROM-free tests for lp64_play.py's `peaks` command (LP64-005 Task 13) and its `--by-stage` and
`--budget` modes (LP64-006 Task 11), on run folders of logs written here from invented numbers."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_play  # noqa: E402


def peak(pool: str, used: int, size: int) -> str:
    """port/src/port_lp64.c's peak line."""
    return f'ge: lp64: peak {pool} {used} of {size} bytes\n'


def stage(level: int) -> str:
    """src/game/bg.c's line as a stage's background loads."""
    return f'ge: load_bg_file levelid={level} stan=Tbg_x_all_p_stanZ file=0x1 record=0x2 first=0x3\n'


class Folders(unittest.TestCase):
    """Run folders of invented logs, and the tool run on them."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def folder(self, name: str, logs: dict[str, str]) -> Path:
        path = self.root / name
        path.mkdir()
        for case, text in logs.items():
            (path / f'{case}.log').write_text(text)
        return path

    def run_peaks(self, *args) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = lp64_play.main(['peaks', *map(str, args)])
        return status, out.getvalue()

    def budget(self, text: str) -> Path:
        path = self.root / 'budgets.txt'
        path.write_text(text)
        return path


class Peaks(Folders):

    def test_largest_peak_over_folders_names_its_case(self):
        # Arrange
        plain = self.folder('plain', {'stage-dam': peak('stage bank', 500, 1000) + peak('stage bank', 300, 1000),
                                      'stage-cave': peak('stage bank', 700, 1000)})
        video = self.folder('video', {'stage-dam': peak('stage bank', 600, 1000)})

        # Act
        status, out = self.run_peaks(plain, video)

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('stage bank: 700 of 1000 bytes (70.0%) in plain/stage-cave\n', out)

    def test_pool_name_with_spaces_is_read_whole(self):
        # Arrange
        plain = self.folder('plain', {'tour-dam': peak('room memory largest request', 90, 600)})

        # Act
        status, out = self.run_peaks(plain)

        # Assert
        self.assertIn('room memory largest request: 90 of 600 bytes (15.0%) in plain/tour-dam\n', out)

    def test_pools_of_other_sizes_keep_the_largest_share(self):
        # Arrange: a small pool nearly full beats a large one with more bytes in use
        plain = self.folder('plain', {'a': peak('texture pool', 5000, 100000), 'b': peak('texture pool', 900, 1000)})

        # Act
        status, out = self.run_peaks(plain)

        # Assert
        self.assertIn('texture pool: 900 of 1000 bytes (90.0%) in plain/b ABOVE 80%\n', out)

    def test_share_at_eighty_percent_is_not_marked(self):
        # Arrange
        plain = self.folder('plain', {'a': peak('music heap', 800, 1000)})

        # Act
        status, out = self.run_peaks(plain)

        # Assert
        self.assertIn('music heap: 800 of 1000 bytes (80.0%) in plain/a\n', out)

    def test_exhaustion_lines_are_counted_beside_the_peak(self):
        # Arrange
        plain = self.folder('plain', {'a': peak('room memory', 10, 100)
                                      + 'ge: lp64: pool "room memory" exhausted (request 64 bytes)\n' * 2})

        # Act
        status, out = self.run_peaks(plain)

        # Assert
        self.assertIn('room memory: 10 of 100 bytes (10.0%) in plain/a; 2 exhaustion lines\n', out)

    def test_folders_without_peak_lines_fail(self):
        # Arrange
        plain = self.folder('plain', {'a': 'ge: frame limit reached: frames=600 gfxtasks=600\n'})

        # Act
        status, out = self.run_peaks(plain)

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('no `ge: lp64: peak` line in 1 logs', out)


class ByStage(Folders):
    def test_by_stage_attributes_a_peak_to_the_level_before_it(self):
        # Arrange
        plain = self.folder('plain', {'tour-aztec': peak('music heap', 300, 1000) + stage(28)
                                      + 'ge: memstats: stage lap=0 stage=28\n' + peak('room memory', 700, 1000)})

        # Act
        status, out = self.run_peaks('--by-stage', plain)

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('music heap 0 1000 300 plain/tour-aztec\n', out)
        self.assertIn('room memory 28 1000 700 plain/tour-aztec ABOVE HALF\n', out)

    def test_by_stage_gives_a_print_without_a_load_since_the_last_to_the_front_end(self):
        # Arrange: the front end's pools print at the next stage's unload, with no load of their own
        plain = self.folder('plain', {'soak': stage(33) + peak('room memory', 100, 1000) + 'ge: memstats: front\n'
                                      + peak('graphics buffer', 200, 1000)})

        # Act
        status, out = self.run_peaks('--by-stage', plain)

        # Assert
        self.assertIn('graphics buffer 0 1000 200 plain/soak\n', out)

    def test_by_stage_reads_run_subfolders(self):
        # Arrange: lp64_equiv.py's folder keeps each repeat's logs in runK/
        equiv = self.folder('a64', {})
        for run, used in (('run1', 400), ('run2', 600)):
            (equiv / run).mkdir()
            (equiv / run / 'dam.log').write_text(stage(33) + peak('texture pool', used, 1000))

        # Act
        status, out = self.run_peaks('--by-stage', equiv)

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('texture pool 33 1000 600 a64/run2/dam ABOVE HALF\n', out)


class Budget(Folders):
    def logs(self, used: int, size: int = 1000) -> Path:
        return self.folder('plain', {'tour-dam': stage(33) + peak('room memory', used, size)})

    def test_budget_fails_a_pool_past_half(self):
        # Arrange
        plain = self.logs(501)
        budgets = self.budget('room memory 33 1000 400 plain/tour-dam\n')

        # Act
        status, out = self.run_peaks('--budget', budgets, plain)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('room memory in level 33: peak 501 of 1000 bytes is above half its size', out)

    def test_budget_fails_an_unlisted_pair(self):
        # Arrange
        plain = self.logs(100)
        budgets = self.budget('room memory 34 1000 100 plain/tour-cave\n')

        # Act
        status, out = self.run_peaks('--budget', budgets, plain)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('room memory in level 33: 1000 bytes, not in', out)

    def test_budget_fails_a_changed_size(self):
        # Arrange
        plain = self.logs(100, size=2000)
        budgets = self.budget('room memory 33 1000 100 plain/tour-dam\n')

        # Act
        status, out = self.run_peaks('--budget', budgets, plain)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('room memory in level 33: 2000 bytes, but the budget lists 1000', out)

    def test_budget_passes_at_exactly_half(self):
        # Arrange
        plain = self.logs(500)
        budgets = self.budget('# pool level size peak case\nroom memory 33 1000 500 plain/tour-dam\n')

        # Act
        status, out = self.run_peaks('--budget', budgets, plain)

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: 1 pool and stage pairs within budget\n', out)

    def test_budget_accepts_a_withheld_pair_past_half_by_name(self):
        # Arrange
        plain = self.logs(700)
        budgets = self.budget('room memory 33 1000 700 plain/tour-dam withheld: its raise changed a picture\n')

        # Act
        status, out = self.run_peaks('--budget', budgets, plain)

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('withheld: room memory in level 33: peak 700 of 1000 bytes (its raise changed a picture)\n', out)

    def test_budget_fails_an_exhausted_pool(self):
        # Arrange
        plain = self.folder('plain', {'tour-dam': stage(33)
                                      + 'ge: lp64: pool "room memory" exhausted (request 64 bytes)\n'
                                      + peak('room memory', 100, 1000)})
        budgets = self.budget('room memory 33 1000 100 plain/tour-dam\n')

        # Act
        status, out = self.run_peaks('--budget', budgets, plain)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('room memory in level 33: 1 exhaustion lines', out)


if __name__ == '__main__':
    unittest.main()
