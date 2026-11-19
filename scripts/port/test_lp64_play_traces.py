"""ROM-free tests for lp64_play.py traces and callers, on sample logs written by the test and a
stand-in addr2line."""
import contextlib
import io
from pathlib import Path
import stat
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_play  # noqa: E402
from lp64_play_cases import DEMO_END_TICKS, DEMOS  # noqa: E402

# The game goes on ticking a while after a demo's time ends (the fade), so a whole trace runs
# past the demo's end tick.
AFTER_END = 20


def trace(name: str, ticks: int | None = None, differ_from: int | None = None,
          end_reason: str = 'totaltime end (timer/limit)') -> str:
    """NAME's log with TICKS tick lines (by default to AFTER_END past its measured end) and, if
    the trace reaches the end tick, its exit line there; from tick DIFFER_FROM on, the live seed
    is another."""
    end = DEMO_END_TICKS[name]
    lines = ['ge: demo start: stage=33 diff=0 pads=2 totaltime=1200 vsyncs (~20s)',
             f'ge: GE_DEMO direct playback: {name} (index {DEMOS.index(name)})']
    for tick in range(1, (end + AFTER_END if ticks is None else ticks) + 1):
        live = 0x10 + tick % 200 if differ_from is None or tick < differ_from else 0xEE
        lines.append(f'ge: demo tick {tick} draws=4 live={live:02x} rec={0x10 + tick % 200:02x} p0=0,0 p1=0,0 b=0000,0000')
        if tick == end:
            lines.append(f'ge: demo exit: {end_reason} at tick {end}, gametime 1300 vsyncs (~21s), detail 1/1')
    return '\n'.join(lines) + '\n'


OTHER_STREAM = 'ge: romdata: model GtriggerZ converted'


def torn(text: str, tick: int, cut: int) -> str:
    """TEXT with tick TICK's line cut CUT characters in by a whole line of the other output stream,
    the line's remainder following on a line of its own (seen in an i686 run: facility1, tick 331)."""
    head = f'ge: demo tick {tick} '
    start = text.index(head)
    end = text.index('\n', start)
    line = text[start:end]
    return text[:start] + line[:cut] + OTHER_STREAM + '\n' + line[cut:] + text[end:]


class Folders(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)

    def run_folder(self, name: str, logs: dict[str, str]) -> Path:
        """A run folder with a whole log for every demo, LOGS replacing some."""
        folder = self.folder/name
        folder.mkdir()
        for demo in DEMOS:
            (folder/f'{demo}.log').write_text(logs.get(demo, trace(demo)))
        return folder

    def expectations(self, lines: dict[str, str]) -> Path:
        path = self.folder/'expect.txt'
        path.write_text('# a comment\n\n' + ''.join(f'{demo} {lines.get(demo, "identical")}\n' for demo in DEMOS))
        return path

    def main(self, *args: str) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = lp64_play.main(list(args))
        return status, out.getvalue()


class Traces(Folders):
    def test_identical_runs_recorded_identical_pass(self):
        # Arrange
        ref, new = self.run_folder('ref', {}), self.run_folder('new', {})

        # Act
        status, out = self.main('traces', '--expect', str(self.expectations({})), str(ref), str(new))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('dam1: identical (458 ticks)', out)
        self.assertIn('OK: 14 demos, 14 identical, 0 differing as recorded', out)

    def test_difference_at_35_recorded_as_such_passes(self):
        # Arrange
        ref, new = self.run_folder('ref', {}), self.run_folder('new', {'facility2': trace('facility2', differ_from=35)})
        expect = self.expectations({'facility2': 'differs-from 35 a cause (Task 5)'})

        # Act
        status, out = self.main('traces', '--expect', str(expect), str(ref), str(new))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('facility2: first differs at tick 35', out)
        self.assertIn('OK: 14 demos, 13 identical, 1 differing as recorded', out)

    def test_difference_at_35_recorded_identical_fails(self):
        # Arrange
        ref, new = self.run_folder('ref', {}), self.run_folder('new', {'facility2': trace('facility2', differ_from=35)})

        # Act
        status, out = self.main('traces', '--expect', str(self.expectations({})), str(ref), str(new))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: 1 of 14 demos not as recorded: facility2', out)

    def test_recorded_difference_that_now_matches_fails(self):
        # Arrange
        ref, new = self.run_folder('ref', {}), self.run_folder('new', {})
        expect = self.expectations({'dam1': 'differs-from 2 weaponIsOneHanded (Task 4)'})

        # Act
        status, out = self.main('traces', '--expect', str(expect), str(ref), str(new))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('not as recorded: expect.txt has "dam1 differs-from 2"', out)

    def test_shorter_trace_differs_at_its_first_missing_tick(self):
        # Arrange
        ref, new = self.run_folder('ref', {}), self.run_folder('new', {'train': trace('train', 470)})
        expect = self.expectations({'train': 'differs-from 471 the run ended early'})

        # Act
        status, out = self.main('traces', '--expect', str(expect), str(ref), str(new))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('train: first differs at tick 471', out)

    def test_log_without_tick_lines_fails(self):
        # Arrange
        ref, new = self.run_folder('ref', {}), self.run_folder('new', {'silo1': 'ge: demo start: x\n'})

        # Act
        status, out = self.main('traces', '--expect', str(self.expectations({})), str(ref), str(new))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('silo1: no "ge: demo tick" lines in', out)

    def test_second_playback_is_not_part_of_the_trace(self):
        # Arrange
        ref = self.run_folder('ref', {})
        new = self.run_folder('new', {'dam2': trace('dam2') + trace('dam1', 50, 1)})

        # Act
        status, out = self.main('traces', '--expect', str(self.expectations({})), str(ref), str(new))

        # Assert
        self.assertEqual(status, 0, out)

    def test_expectation_file_without_a_demo_is_an_error(self):
        # Arrange
        ref, new = self.run_folder('ref', {}), self.run_folder('new', {})
        expect = self.folder/'short.txt'
        expect.write_text('dam1 identical\n')

        # Act
        status, out = self.main('traces', '--expect', str(expect), str(ref), str(new))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('no line for dam2', out)

    def test_reference_check_prints_unstable_for_a_pair_that_differs(self):
        # Arrange
        a, b = self.run_folder('a', {}), self.run_folder('b', {'bunker2': trace('bunker2', differ_from=120)})

        # Act
        status, out = self.main('traces', '--reference-check', str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('bunker2: unstable (first differs at tick 120)', out)
        self.assertIn('dam1: identical (458 ticks)', out)

    def test_reference_check_refuses_two_equally_truncated_traces(self):
        # Arrange
        a = self.run_folder('a', {'silo2': trace('silo2', 100)})
        b = self.run_folder('b', {'silo2': trace('silo2', 100)})

        # Act
        status, out = self.main('traces', '--reference-check', str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('silo2: refused: missing line: ge: demo exit: totaltime end (timer/limit) at tick 503; '
                      'last tick 100 in', out)
        self.assertNotIn('silo2: identical', out)
        self.assertIn('FAIL: 1 of 14 demos unstable or missing: silo2', out)

    def test_expect_refuses_a_truncated_run_recorded_as_differing(self):
        # Arrange
        ref = self.run_folder('ref', {})
        new = self.run_folder('new', {'dam1': trace('dam1', 100, differ_from=2)})
        expect = self.expectations({'dam1': 'differs-from 2 weaponIsOneHanded (Task 4)'})

        # Act
        status, out = self.main('traces', '--expect', str(expect), str(ref), str(new))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: refused: missing line: ge: demo exit: totaltime end (timer/limit) at tick 438; '
                      'last tick 100 in', out)
        self.assertIn('FAIL: 1 of 14 demos not as recorded: dam1', out)

    def test_expect_refuses_a_reference_whose_first_exit_is_a_checksum_mismatch(self):
        # Arrange
        ref = self.run_folder('ref', {'frigate1': trace('frigate1', end_reason='CHECKSUM mismatch (stored/computed)')})
        new = self.run_folder('new', {})

        # Act
        status, out = self.main('traces', '--expect', str(self.expectations({})), str(ref), str(new))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('frigate1: refused: missing line: ge: demo exit: totaltime end (timer/limit) at tick 328; '
                      'the log has: ge: demo exit: CHECKSUM mismatch', out)

    def test_reference_check_refuses_a_missing_log(self):
        # Arrange
        a, b = self.run_folder('a', {}), self.run_folder('b', {})
        (b/'train.log').unlink()

        # Act
        status, out = self.main('traces', '--reference-check', str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('train: no log', out)

    def test_reference_check_refuses_an_empty_log(self):
        # Arrange
        a, b = self.run_folder('a', {'dam2': ''}), self.run_folder('b', {})

        # Act
        status, out = self.main('traces', '--reference-check', str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam2: no "ge: demo tick" lines in', out)

    def test_torn_tick_line_is_named_and_fails_where_it_would_differ(self):
        # Arrange: the remainder carries the same values, so only the tear separates the traces.
        ref = self.run_folder('ref', {})
        new = self.run_folder('new', {'facility1': torn(trace('facility1'), 331, 40)})

        # Act
        status, out = self.main('traces', '--expect', str(self.expectations({})), str(ref), str(new))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('facility1: torn line at tick 331 in', out)
        self.assertNotIn('facility1: first differs', out)
        self.assertIn('FAIL: 1 of 14 demos not as recorded: facility1', out)

    def test_torn_tick_line_does_not_pass_where_a_difference_is_recorded(self):
        # Arrange: a torn line must not hide a recorded difference either.
        ref = self.run_folder('ref', {})
        new = self.run_folder('new', {'facility1': torn(trace('facility1', differ_from=331), 331, 40)})
        expect = self.expectations({'facility1': 'differs-from 331 a cause'})

        # Act
        status, out = self.main('traces', '--expect', str(expect), str(ref), str(new))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('facility1: torn line at tick 331 in', out)

    def test_tear_before_the_tick_number_is_named_at_the_tick_it_hides(self):
        # Arrange: the cut falls inside "demo tick", so neither part reads as tick 331's line.
        a = self.run_folder('a', {'facility1': torn(trace('facility1'), 331, 9)})
        b = self.run_folder('b', {})

        # Act
        status, out = self.main('traces', '--reference-check', str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('facility1: torn line at tick 331 in', out)
        self.assertNotIn('facility1: unstable', out)

    def test_tear_inside_the_tick_number_is_named_at_its_tick(self):
        # Arrange: "ge: demo tick 33" then "1 draws=...": the first part's number is a smaller one.
        ref = self.run_folder('ref', {})
        new = self.run_folder('new', {'facility1': torn(trace('facility1'), 331, 16)})

        # Act
        status, out = self.main('traces', '--expect', str(self.expectations({})), str(ref), str(new))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('facility1: torn line at tick 331 in', out)

    def test_tear_in_a_later_playback_is_not_part_of_the_trace(self):
        # Arrange
        ref = self.run_folder('ref', {})
        new = self.run_folder('new', {'dam2': trace('dam2') + torn(trace('dam1', 50), 30, 40)})

        # Act
        status, out = self.main('traces', '--expect', str(self.expectations({})), str(ref), str(new))

        # Assert
        self.assertEqual(status, 0, out)

    def test_tear_in_the_next_playbacks_first_tick_line_is_named_after_the_first_playback(self):
        # Arrange: only a whole tick line of a lower number ends the first playback, so a tear
        # before it counts against the first playback, at the tick after its last (it fails safe).
        ref = self.run_folder('ref', {})
        new = self.run_folder('new', {'dam2': trace('dam2') + torn(trace('dam1', 50), 1, 9)})
        after_last = DEMO_END_TICKS['dam2'] + AFTER_END + 1

        # Act
        status, out = self.main('traces', '--expect', str(self.expectations({})), str(ref), str(new))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn(f'dam2: torn line at tick {after_last} in', out)

    def test_reference_check_of_agreeing_runs_passes(self):
        # Arrange
        a, b = self.run_folder('a', {}), self.run_folder('b', {})

        # Act
        status, out = self.main('traces', '--reference-check', str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: 14 demos agree with themselves', out)


RNG_LOG = ('ge: lp64: address space OK: memp arena 0xffff9f400000, executable image 0xaaaabaf60000, '
           'stack 0xfffff921277c\n'
           'ge: rng draw 440 from 0xaaaabafca268\n'
           'ge: demo tick 1 draws=1 live=f2 rec=f2 p0=0,0 p1=0,0 b=0000,0000\n'
           'ge: rng draw 441 from 0xaaaabb055214\n'
           'ge: rng draw 442 from 0xaaaabafca268\n'
           'ge: demo tick 2 draws=2 live=b1 rec=a3 p0=2,-3 p1=0,1 b=0000,0000\n'
           'ge: rng draw 443 from 0xaaaabafca268\n')


class Callers(Folders):
    def executable(self, elf_type: int) -> Path:
        path = self.folder/'IronSights'
        path.write_bytes(b'\x7fELF\x02\x01\x01' + bytes(9) + elf_type.to_bytes(2, 'little') + bytes(46))
        return path

    def addr2line(self) -> Path:
        """A stand-in addr2line that logs its arguments and names each address it is given."""
        path = self.folder/'addr2line'
        path.write_text(f'#!/bin/sh\necho "$@" > "{self.folder}/args"\nshift 3\n'
                        'for a in "$@"; do echo "fn_$a"; echo "/home/dev/project/src/game/x.c:${#a} (discriminator 2)"; done\n')
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def test_draws_become_tick_function_file_line_relative_to_the_image(self):
        # Arrange
        log = self.folder/'rng.log'
        log.write_text(RNG_LOG)
        exe, tool = self.executable(3), self.addr2line()

        # Act
        status, out = self.main('callers', '--tick', '2', '--addr2line', str(tool), str(log), str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertEqual(out.splitlines(), ['1 fn_0x6a267 src/game/x.c:7', '2 fn_0xf5213 src/game/x.c:7',
                                           '2 fn_0x6a267 src/game/x.c:7'])
        self.assertEqual((self.folder/'args').read_text().split(),
                         ['-f', '-e', str(exe), '0x6a267', '0xf5213', '0x6a267'])

    def test_a_fixed_address_executable_keeps_its_addresses(self):
        # Arrange
        log = self.folder/'rng.log'
        log.write_text(RNG_LOG.replace('0xaaaabb055214', '0x8123456').replace('0xaaaabafca268', '0x8000010'))
        exe, tool = self.executable(2), self.addr2line()

        # Act
        status, out = self.main('callers', '--tick', '2', '--addr2line', str(tool), str(log), str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertEqual(out.splitlines(), ['1 fn_0x800000f src/game/x.c:9', '2 fn_0x8123455 src/game/x.c:9',
                                           '2 fn_0x800000f src/game/x.c:9'])

    def test_position_independent_executable_without_the_address_line_is_an_error(self):
        # Arrange
        log = self.folder/'rng.log'
        log.write_text('\n'.join(RNG_LOG.splitlines()[1:]) + '\n')
        exe, tool = self.executable(3), self.addr2line()

        # Act
        status, out = self.main('callers', '--tick', '2', '--addr2line', str(tool), str(log), str(exe))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('no "ge: lp64: address space OK" line', out)


if __name__ == '__main__':
    unittest.main()
