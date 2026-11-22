"""ROM-free tests for lp64_equiv_streams.py (LP64-006 Task 3): the first
difference between two simulation traces, on trace files written here from
invented values."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_equiv_streams  # noqa: E402


def sim_line(n: int, seed: str = '00000000deadbeef', chrs: str = '3:0a1b2466', demo: int | None = None,
             timer: int | None = None) -> str:
    """One trace line as port_simtrace.c writes it; DEMO, while a recording replays, its tick."""
    tail = '' if demo is None else f' demo={demo}'
    return (f'T n={n} timer={n if timer is None else timer} seed={seed} draws=2 players=1 '
            'p0=3f800000,00000000,c2c80000,40490fdb,3f800000 '
            f'chrs={chrs} props=12:01234567 rooms=4:89abcdef{tail}\n')


def playback(first_n: int, ticks: range, timer_shift: int = 0, changes: dict[int, dict] | None = None) -> str:
    """Boot lines without a replay up to FIRST_N, then a replay of demo ticks TICKS, then two more
    lines without one: a paced demo's trace. CHANGES gives a demo tick's own field values."""
    lines = [sim_line(n, timer=n + timer_shift) for n in range(1, first_n)]
    n = first_n
    for tick in ticks:
        lines.append(sim_line(n, demo=tick, timer=n + timer_shift, **(changes or {}).get(tick, {})))
        n += 1
    lines += [sim_line(n, timer=n + 7), sim_line(n + 1, timer=n + 9)]
    return ''.join(lines)


class TraceFiles(unittest.TestCase):
    """A temporary folder for trace files."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def trace(self, name: str, text: str) -> Path:
        path = self.root / name
        path.write_text(text)
        return path


class SimDifference(TraceFiles):
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

    def test_player_in_another_slot_is_a_difference(self):
        # Arrange
        a = self.trace('a.txt', sim_line(1))
        b = self.trace('b.txt', sim_line(1).replace(' p0=', ' p1='))

        # Act
        difference = lp64_equiv_streams.first_difference('sim', a, b)

        # Assert
        self.assertEqual((difference.index, difference.field, difference.a[:3], difference.b[:3]),
                         (1, 'p0', 'p0=', 'p1='))

    def test_record_with_an_extra_field_is_a_difference(self):
        # Arrange: records the file reader cannot make, one field longer.
        short = lp64_equiv_streams.Record('T', 1, (('n', '1'),))
        long = lp64_equiv_streams.Record('T', 1, (('n', '1'), ('timer', '1')))
        readers = {'sim': lambda path: [short] if str(path) == 'a' else [long]}

        # Act
        with mock.patch.dict(lp64_equiv_streams.READERS, readers):
            difference = lp64_equiv_streams.first_difference('sim', Path('a'), Path('b'))

        # Assert
        self.assertEqual((difference.field, difference.a, difference.b), ('timer', '(none)', 'timer=1'))

    def test_detail_line_difference_names_its_record_field(self):
        # Arrange
        guard = 'G n=2 slot=0 chrnum=7 action=1 flags=00000010 pos=41200000,00000000,41a00000 damage={}\n'
        a = self.trace('a.txt', sim_line(1) + sim_line(2) + guard.format('00000000'))
        b = self.trace('b.txt', sim_line(1) + sim_line(2) + guard.format('3f000000'))

        # Act
        difference = lp64_equiv_streams.first_difference('sim', a, b)

        # Assert
        self.assertEqual((difference.index, difference.field), (2, 'G.damage'))



class DemoDifference(TraceFiles):
    def test_playback_ignores_n_and_timer_and_lines_outside_it(self):
        # Arrange: the replay starts at another line and with another timer
        a = self.trace('a.txt', playback(4, range(1, 6)))
        b = self.trace('b.txt', playback(6, range(1, 6), timer_shift=3))

        # Act
        difference = lp64_equiv_streams.first_difference('sim-demo', a, b)

        # Assert
        self.assertIsNone(difference)

    def test_names_the_demo_tick_and_field(self):
        # Arrange
        a = self.trace('a.txt', playback(4, range(1, 6)))
        b = self.trace('b.txt', playback(2, range(1, 6), changes={3: {'seed': '0000000000000001'}}))

        # Act
        difference = lp64_equiv_streams.first_difference('sim-demo', a, b)

        # Assert
        self.assertEqual((difference.index, difference.field), (3, 'seed'))

    def test_trace_without_a_replay_is_unreadable(self):
        # Arrange
        a = self.trace('a.txt', ''.join(sim_line(n) for n in range(1, 4)))
        b = self.trace('b.txt', playback(2, range(1, 4)))

        # Act, Assert
        with self.assertRaisesRegex(lp64_equiv_streams.Unreadable, 'no line of a replay'):
            lp64_equiv_streams.first_difference('sim-demo', a, b)

    def test_shorter_playback_differs_at_the_first_tick_it_lacks(self):
        # Arrange
        a = self.trace('a.txt', playback(3, range(1, 6)))
        b = self.trace('b.txt', playback(3, range(1, 4)))

        # Act
        difference = lp64_equiv_streams.first_difference('sim-demo', a, b)

        # Assert
        self.assertEqual((difference.index, difference.field, difference.a, difference.b), (4, 'demo', '4', '(none)'))

    def test_plain_sim_still_compares_the_demo_field(self):
        # Arrange
        a = self.trace('a.txt', sim_line(1, demo=1))
        b = self.trace('b.txt', sim_line(1, demo=2))

        # Act
        difference = lp64_equiv_streams.first_difference('sim', a, b)

        # Assert
        self.assertEqual((difference.index, difference.field), (1, 'demo'))


if __name__ == '__main__':
    unittest.main()
