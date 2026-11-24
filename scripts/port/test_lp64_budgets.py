"""ROM-free tests for lp64_budgets.py, the writer of the 64-bit pool budgets, with hand-written logs."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_budgets  # noqa: E402

HEADER = '# The 64-bit pool budgets\n'
DAM = 'ge: load_bg_file levelid=33 stan=Tbg_dam_all_p_stanZ file=0x1 record=0x2 first=0x3\n'
JUNGLE = 'ge: load_bg_file levelid=37 stan=Tbg_jun_all_p_stanZ file=0x1 record=0x2 first=0x3\n'


def peak(pool: str, used: int, size: int) -> str:
    return f'ge: lp64: peak {pool} {used} of {size} bytes\n'


class Budgets(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.budgets = self.folder/'budgets.txt'

    def run_folder(self, *logs: str) -> Path:
        """A run folder holding one log a text of LOGS, named case0, case1..."""
        folder = self.folder/'run'
        folder.mkdir()
        for number, text in enumerate(logs):
            (folder/f'case{number}.log').write_text(text)
        return folder

    def write(self, budgets: str, *argv: str) -> tuple[int, str, str]:
        self.budgets.write_text(HEADER + budgets)
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = lp64_budgets.main(['--budgets', str(self.budgets), *argv])
        return status, out.getvalue(), err.getvalue()

    def test_a_larger_peak_replaces_the_listed_one_and_a_new_size_is_added(self):
        # Arrange
        run = self.run_folder(DAM + peak('vertex pool', 300, 1000) + JUNGLE + peak('vertex pool', 100, 2000))

        # Act
        status, out, err = self.write('vertex pool 33 1000 200 old/case\n', str(run))

        # Assert
        self.assertEqual(status, 0, err)
        self.assertEqual(out, HEADER + 'vertex pool 33 1000 300 run/case0\nvertex pool 37 2000 100 run/case0\n')

    def test_a_smaller_peak_keeps_the_listed_line(self):
        # Arrange
        run = self.run_folder(DAM + peak('vertex pool', 100, 1000))

        # Act
        status, out, _ = self.write('vertex pool 33 1000 200 old/case\n', str(run))

        # Assert
        self.assertEqual((status, out), (0, HEADER + 'vertex pool 33 1000 200 old/case\n'))

    def test_a_line_past_half_is_refused_and_nothing_is_written(self):
        # Arrange
        run = self.run_folder(DAM + peak('vertex pool', 600, 1000))

        # Act
        status, out, err = self.write('vertex pool 33 1000 200 old/case\n', str(run), '--write')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('FAIL: vertex pool 33 1000: peak 600 (run/case0) is past half', err)
        self.assertEqual(self.budgets.read_text(), HEADER + 'vertex pool 33 1000 200 old/case\n')

    def test_a_withheld_line_keeps_its_reason_past_half(self):
        # Arrange
        run = self.run_folder(DAM + peak('vertex pool', 600, 1000))

        # Act
        status, out, _ = self.write('vertex pool 33 1000 200 old/case withheld: the owner decides\n', str(run))

        # Assert
        self.assertEqual((status, out), (0, HEADER + 'vertex pool 33 1000 600 run/case0 withheld: the owner decides\n'))

    def test_retire_and_withhold_move_a_pool_to_its_restored_size(self):
        # Arrange: the pool went back from 2000 to 1000 bytes and fills in level 33
        run = self.run_folder(DAM + peak('synthesizer parameters', 1000, 1000))

        # Act
        status, out, err = self.write('synthesizer parameters 33 2000 900 old/case\n', str(run),
                                      '--retire', 'synthesizer parameters:2000',
                                      '--withhold', 'synthesizer parameters:33:1000:raise withheld')

        # Assert
        self.assertEqual(status, 0, err)
        self.assertEqual(out, HEADER + 'synthesizer parameters 33 1000 1000 run/case0 withheld: raise withheld\n')

    def test_pool_limits_the_peaks_folded_in(self):
        # Arrange
        run = self.run_folder(DAM + peak('vertex pool', 300, 1000) + peak('graphics buffer', 10, 100))

        # Act
        status, out, _ = self.write('', str(run), '--pool', 'graphics buffer')

        # Assert
        self.assertEqual((status, out), (0, HEADER + 'graphics buffer 33 100 10 run/case0\n'))

    def test_a_persistent_pool_is_listed_in_every_level_with_its_largest_peak(self):
        # Arrange
        size = 64 * 1024 * 1024
        run = self.run_folder(DAM + peak('high arena', 7, size), JUNGLE + peak('high arena', 9, size))

        # Act
        status, out, _ = self.write('vertex pool 0 1000 1 old/menu\n', str(run))

        # Assert
        self.assertEqual(status, 0)
        self.assertEqual(out, HEADER + f'high arena 0 {size} 9 run/case1\nhigh arena 33 {size} 9 run/case1\n'
                         f'high arena 37 {size} 9 run/case1\nvertex pool 0 1000 1 old/menu\n')

    def test_an_exhaustion_line_is_refused(self):
        # Arrange
        run = self.run_folder(DAM + 'ge: lp64: pool "vertex pool" exhausted: 40 bytes\n')

        # Act
        status, _, err = self.write('', str(run))

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('FAIL: vertex pool in level 33: 1 exhaustion lines', err)

    def test_write_replaces_the_file_and_keeps_its_comments(self):
        # Arrange
        run = self.run_folder(DAM + peak('vertex pool', 300, 1000))

        # Act
        status, _, _ = self.write('# kept\nvertex pool 33 1000 200 old/case\n', str(run), '--write')

        # Assert
        self.assertEqual(status, 0)
        self.assertEqual(self.budgets.read_text(), HEADER + '# kept\nvertex pool 33 1000 300 run/case0\n')


if __name__ == '__main__':
    unittest.main()
