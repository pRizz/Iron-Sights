"""ROM-free tests for lp64_equiv_streams.py (LP64-006 Task 3): the first
difference between two simulation traces, on trace files written here from
invented values."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_equiv_streams  # noqa: E402


def sim_line(n: int, seed: str = '00000000deadbeef', chrs: str = '3:0a1b2466') -> str:
    """One trace line as port_simtrace.c writes it."""
    return (f'T n={n} timer={n} seed={seed} draws=2 players=1 '
            'p0=3f800000,00000000,c2c80000,40490fdb,3f800000 '
            f'chrs={chrs} props=12:01234567 rooms=4:89abcdef\n')


class SimDifference(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def trace(self, name: str, text: str) -> Path:
        path = self.root / name
        path.write_text(text)
        return path

    def test_identical_files_have_no_difference(self):
        # Arrange
        text = ''.join(sim_line(n) for n in range(1, 6))
        a = self.trace('a.txt', text)
        b = self.trace('b.txt', text)

        # Act
        difference = lp64_equiv_streams.first_difference('sim', a, b)

        # Assert
        self.assertIsNone(difference)

    def test_names_the_first_differing_tick_and_field(self):
        # Arrange
        a = self.trace('a.txt', ''.join(sim_line(n) for n in range(1, 6)))
        b = self.trace('b.txt', ''.join(
            sim_line(n, chrs='3:ffffffff' if n >= 3 else '3:0a1b2466',
                     seed='0000000000000001' if n >= 4 else '00000000deadbeef')
            for n in range(1, 6)))

        # Act
        difference = lp64_equiv_streams.first_difference('sim', a, b)

        # Assert
        self.assertEqual((difference.index, difference.field, difference.a, difference.b),
                         (3, 'chrs', '3:0a1b2466', '3:ffffffff'))

    def test_short_file_is_unreadable(self):
        # Arrange
        a = self.trace('a.txt', ''.join(sim_line(n) for n in range(1, 6)))
        b = self.trace('b.txt', ''.join(sim_line(n) for n in range(1, 4)))

        # Act, Assert
        with self.assertRaises(lp64_equiv_streams.Unreadable):
            lp64_equiv_streams.first_difference('sim', a, b)

    def test_file_with_no_line_is_unreadable(self):
        # Arrange
        a = self.trace('a.txt', sim_line(1))
        b = self.trace('b.txt', '')

        # Act, Assert
        with self.assertRaises(lp64_equiv_streams.Unreadable):
            lp64_equiv_streams.first_difference('sim', a, b)

    def test_line_cut_in_two_is_unreadable(self):
        # Arrange
        text = sim_line(1) + sim_line(2)
        a = self.trace('a.txt', text)
        b = self.trace('b.txt', text[:-20])

        # Act, Assert
        with self.assertRaises(lp64_equiv_streams.Unreadable):
            lp64_equiv_streams.first_difference('sim', a, b)

    def test_line_that_does_not_parse_is_unreadable(self):
        # Arrange
        a = self.trace('a.txt', sim_line(1))
        b = self.trace('b.txt', sim_line(1).replace('seed=00000000deadbeef', 'seed=0xdeadbeef'))

        # Act, Assert
        with self.assertRaises(lp64_equiv_streams.Unreadable):
            lp64_equiv_streams.first_difference('sim', a, b)

    def test_detail_line_difference_names_its_record_field(self):
        # Arrange
        guard = 'G n=2 slot=0 chrnum=7 action=1 flags=00000010 pos=41200000,00000000,41a00000 damage={}\n'
        a = self.trace('a.txt', sim_line(1) + sim_line(2) + guard.format('00000000'))
        b = self.trace('b.txt', sim_line(1) + sim_line(2) + guard.format('3f000000'))

        # Act
        difference = lp64_equiv_streams.first_difference('sim', a, b)

        # Assert
        self.assertEqual((difference.index, difference.field), (2, 'G.damage'))


if __name__ == '__main__':
    unittest.main()
