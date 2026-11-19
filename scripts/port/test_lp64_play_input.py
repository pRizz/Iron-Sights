"""ROM-free tests for lp64_play.py's `input` set (LP64-005 Task 8): the routes written as GE_INPUTPLAY
files, the ticks a route of steps finds by running, the ordered play lines and a torn line, through
the stand-in executables of test_lp64_play.py."""
from pathlib import Path
import json
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lp64_play_route import START_BUTTON, Z_TRIG, Press, route_text  # noqa: E402
from test_lp64_play import BOOT, StandIns, frames  # noqa: E402


def play(tick: int, event: str, value: int) -> str:
    return f'ge: play: tick {tick}\nge: play: {event} {value}\n'


# The front end as the 64-bit build walks it with presses 60 ticks after each menu (measured
# 2026-11-18): each menu appears on the tick its press goes down, the file select one tick later.
FRONT_END = (BOOT + play(0, 'stage-load', 90) + play(245, 'menu', 1) + play(305, 'menu', 2)
             + play(365, 'menu', 3) + play(425, 'menu', 4) + play(486, 'menu', 5) + play(546, 'menu', 6)
             + play(606, 'menu', 7) + play(666, 'menu', 8) + play(726, 'menu', 10) + play(786, 'menu', 11)
             + play(791, 'stage-unload', 90) + play(791, 'stage-load', 33) + frames(2400))
DAM = (BOOT + play(5, 'stage-load', 33) + 'ge: GE_STAGE direct launch: levelid=33 diff=0\n'
       'ge: load_bg_file levelid=33 stan=Tbg_dam_all_p_stanZ file=0x1 record=0x2 first=0x3\n')
PAUSE_LINES = ('pausebox: STATE -1 -> 0 at frame 13 raw=0.000 eased=0.000 pausing=0\n'
               'pausebox: STATE 4 -> 5 at frame 1317 raw=1.000 eased=1.000 pausing=1\n')
UNPAUSE_LINE = 'pausebox: STATE 8 -> 0 at frame 1915 raw=0.000 eased=0.000 pausing=0\n'
# The abort as the 64-bit build runs it: the confirm unloads Dam and loads the title stage on one
# tick, and the START after it gives the next menu.
BACK_TO_TITLE = play(1430, 'stage-load', 90) + play(1550, 'menu', 13)
KILL = play(726, 'kill', 600)
TEMPLE = (BOOT + 'ge: GE_MP direct launch: arena=1 levelid=38 scenario=0 players=2\n'
          'ge: load_bg_file levelid=38 stan=Tbg_tem_all_p_stanZ file=0x1 record=0x2 first=0x3\n'
          'ge: mp spawn p0 startpad=4 (p180a2) stanroom=11 pos=(-189 ? 844)\n')


class Routes(unittest.TestCase):
    def test_a_press_is_let_go_after_its_hold(self):
        # Act
        text = route_text([(3, Press(START_BUTTON))])

        # Assert
        self.assertEqual(text, 'P 3 1000 0 0\nP 8 0 0 0\n')

    def test_a_press_that_starts_as_the_last_ends_is_not_let_go_between(self):
        # Act
        text = route_text([(0, Press(0, stick_y=70, hold=300)), (300, Press(Z_TRIG, hold=60))])

        # Assert
        self.assertEqual(text, 'P 0 0 0 46\nP 300 2000 0 0\nP 360 0 0 0\n')

    def test_a_negative_stick_is_written_as_its_unsigned_byte(self):
        # Act
        text = route_text([(10, Press(0, stick_x=-70, hold=2))])

        # Assert
        self.assertEqual(text, 'P 10 0 ba 0\nP 12 0 0 0\n')


class InputCases(StandIns):
    def route_of(self, case: str) -> str:
        return (self.output/f'{case}.route').read_text()

    def test_front_end_presses_start_60_ticks_after_each_menu_and_passes(self):
        # Arrange
        exe = self.fake(FRONT_END)

        # Act
        status, out = self.play(exe, 'input', 'front-end')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertTrue(self.route_of('front-end').startswith('P 305 1000 0 0\nP 310 0 0 0\nP 365 1000 0 0\n'))
        ticks = json.loads((self.output/'play.json').read_text())['cases'][0]['route_ticks']
        self.assertEqual(ticks, [305, 365, 425, 485, 546, 606, 666, 726, 786])

    def test_abort_with_dam_unloaded_before_it_loads_fails_as_out_of_order(self):
        # Arrange
        exe = self.fake(BOOT + play(1, 'stage-unload', 33) + DAM[len(BOOT):] + BACK_TO_TITLE + frames(2400))

        # Act
        status, out = self.play(exe, 'input', 'abort')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('abort: FAIL (out of order: ge: play: stage-unload 33 only before ge: play: stage-load 33)',
                      out)

    def test_a_step_whose_line_never_appears_fails_naming_it(self):
        # Arrange
        exe = self.fake(BOOT + play(0, 'stage-load', 90) + frames(2400))

        # Act
        status, out = self.play(exe, 'input', 'front-end')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('front-end: FAIL (route step 1: ge: play: menu 1 did not appear at or after tick 0)', out)

    def test_a_torn_play_line_is_named_as_torn(self):
        # Arrange: another stream's line cut the abort's unload line in two
        exe = self.fake(DAM + 'ge: play: tick 1430\nge: play: stage-unl' + 'ge: snd: stage unload\noad 33\n'
                        + BACK_TO_TITLE + frames(2400))

        # Act
        status, out = self.play(exe, 'input', 'abort')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('abort: FAIL (torn line (another stream cut it): ge: play: stage-unlge: snd: stage unload)',
                      out)

    def test_abort_unloading_dam_then_changing_menu_passes(self):
        # Arrange
        exe = self.fake(DAM + play(1430, 'stage-unload', 33) + BACK_TO_TITLE + frames(2400))

        # Act
        status, out = self.play(exe, 'input', 'abort')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertEqual(self.route_of('abort').splitlines()[0], 'P 1200 1000 0 0')
        self.assertEqual(self.route_of('abort').splitlines()[-2:], ['P 1550 1000 0 0', 'P 1555 0 0 0'])

    def test_abort_without_the_menu_after_the_title_loads_fails(self):
        # Arrange
        exe = self.fake(DAM + play(1430, 'stage-unload', 33) + play(1430, 'stage-load', 90) + frames(2400))

        # Act
        status, out = self.play(exe, 'input', 'abort')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('abort: FAIL (missing line: ge: play: menu N)', out)

    def test_the_final_run_replays_the_route_file(self):
        # Arrange: the stand-in logs its respawn only when GE_INPUTPLAY holds the first START
        exe = self.fake(TEMPLE + frames(1800), script=(
            'grep -q "^P 900 1000 0 0$" "$GE_INPUTPLAY" && '
            'printf "ge: play: tick 726\\nge: play: kill 600\\n'
            'ge: mp spawn p0 pad=1 (p224a1) stanroom=15 pos=(-2189 175 -2802)\\n" >&2'))

        # Act
        status, out = self.play(exe, 'input', 'respawn-temple')

        # Assert
        self.assertEqual(status, 0, out)

    def test_respawn_with_only_the_first_spawn_fails(self):
        # Arrange
        exe = self.fake(TEMPLE + KILL + frames(1800))

        # Act
        status, out = self.play(exe, 'input', 'respawn-temple')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('respawn-temple: FAIL (missing line: ge: mp spawn p0 pad= (the respawn))', out)

    def test_respawn_without_the_kill_fails(self):
        # Arrange
        exe = self.fake(TEMPLE + 'ge: mp spawn p0 pad=1 (p224a1) stanroom=15 pos=(-2189 175 -2802)\n' + frames(1800))

        # Act
        status, out = self.play(exe, 'input', 'respawn-temple')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('respawn-temple: FAIL (missing line: ge: play: kill 600)', out)

    def test_pause_on_and_off_passes(self):
        # Arrange
        exe = self.fake(DAM + PAUSE_LINES + UNPAUSE_LINE + frames(2400))

        # Act
        status, out = self.play(exe, 'input', 'pause-dam')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertEqual(self.route_of('pause-dam'), 'P 1200 1000 0 0\nP 1205 0 0 0\nP 1800 1000 0 0\nP 1805 0 0 0\n')

    def test_pause_never_put_away_fails(self):
        # Arrange
        exe = self.fake(DAM + PAUSE_LINES + frames(2400))

        # Act
        status, out = self.play(exe, 'input', 'pause-dam')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('pause-dam: FAIL (missing line: pausebox: STATE N -> 0 (the watch put away))', out)

    def test_briefing_without_its_menu_line_fails(self):
        # Arrange
        exe = self.fake(BOOT + 'ge: GE_MENU harness: menu=10\n' + frames(900))

        # Act
        status, out = self.play(exe, 'input', 'briefing')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('briefing: FAIL (missing line: ge: play: menu 10)', out)


if __name__ == '__main__':
    unittest.main()
