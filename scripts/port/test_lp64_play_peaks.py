"""ROM-free tests for lp64_play.py's `peaks` command (LP64-005 Task 13), on run folders of logs
written here from invented numbers."""
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


class Peaks(unittest.TestCase):
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

    def run_peaks(self, *dirs: Path) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = lp64_play.main(['peaks', *map(str, dirs)])
        return status, out.getvalue()

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


if __name__ == '__main__':
    unittest.main()
