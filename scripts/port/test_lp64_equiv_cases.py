"""ROM-free tests of lp64_equiv.py's cases and profiles as data (LP64-006 Task 5)."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_equiv  # noqa: E402
import lp64_equiv_cases  # noqa: E402
import lp64_equiv_profile  # noqa: E402
import lp64_play_run  # noqa: E402
from lp64_play_route import Z_TRIG, route_text  # noqa: E402
import settings_placement  # noqa: E402


class Cases(unittest.TestCase):
    def test_route_uses_pad_records_only(self):
        # Arrange
        case = lp64_equiv_cases.solo_case('dam')
        side = lp64_equiv_cases.STRAIGHT_TICKS  # the first side leg's first tick

        # Act
        lines = route_text(case.route.presses).splitlines()

        # Assert
        self.assertFalse([line for line in lines if not line.startswith('P ')], 'no M record')
        self.assertFalse([line for line in lines if int(line.split()[2], 16) & ~Z_TRIG], 'no C button')
        self.assertIn(f'P {side} 0 {-lp64_equiv_cases.SIDE_STICK_X & 0xFF:x} 0', lines)

    def test_unarmed_and_uncontrolled_cases_need_less_than_the_floor(self):
        # Act
        bunker2 = [item.label for item in lp64_equiv_cases.solo_case('bunker2').required]
        cuba = [item.label for item in lp64_equiv_cases.solo_case('cuba').required]
        dam = [item.label for item in lp64_equiv_cases.solo_case('dam').required]

        # Assert
        self.assertIn('ge: play: moved N (Bond walked)', bunker2)
        self.assertFalse([label for label in bunker2 + cuba if 'shots' in label])
        self.assertNotIn('ge: play: moved N (Bond walked)', cuba)
        self.assertIn(f'ge: play: shots N (at least {lp64_equiv_cases.SHOT_FLOOR})', dam)

    def test_no_armed_case_has_a_floor_of_zero(self):
        # Assert
        self.assertFalse([name for name, (floor, _) in lp64_equiv_cases.LOW_FLOORS.items() if floor < 1])

    def test_demo_cases_never_get_lockstep(self):
        # Arrange
        demos = lp64_equiv_cases.SETS['demos']()

        # Act
        envs = [lp64_play_run.case_environment(case, Path('rom'), {}, base={}) for case in demos]

        # Assert
        self.assertEqual(len(demos), 14)
        self.assertFalse([case.name for case in demos if case.lockstep])
        self.assertFalse([env for env in envs if 'GE_LOCKSTEP' in env])

    def test_there_are_38_cases(self):
        # Act
        cases = lp64_equiv.cases_of(['demos', 'solo', 'mp', 'menus', 'reload'])

        # Assert
        self.assertEqual(len(cases), 38)
        self.assertEqual(len({case.name for case in cases}), 38)

    def test_pinned_routes_match_their_steps(self):
        # Assert
        self.assertEqual(len(lp64_equiv_cases.WALK_TICKS), len(lp64_equiv_cases.WALK_STEPS))
        self.assertEqual(len(lp64_equiv_cases.DAM_TWICE_TICKS), len(lp64_equiv_cases.DAM_TWICE_STEPS))
        self.assertEqual(len(lp64_equiv_cases.SAVES_TICKS), len(lp64_equiv_cases.SAVES_STEPS))

    def test_the_saves_set_is_one_lockstep_case_outside_the_38(self):
        # Act
        cases = lp64_equiv.cases_of(['saves'])

        # Assert
        self.assertEqual([(case.name, case.lockstep) for case in cases], [('saves-depot', True)])
        self.assertNotIn('saves-depot', {case.name for case in lp64_equiv.cases_of(
            ['demos', 'solo', 'mp', 'menus', 'reload'])})


class Profiles(unittest.TestCase):
    def test_all_profile_covers_every_fork_key(self):
        # Arrange
        source = (settings_placement.ROOT/settings_placement.OPTIONS).read_text()
        fork = settings_placement.option_keys(source) - settings_placement.read_upstream()

        # Act
        covered = set(lp64_equiv_profile.ALL_ON) | set(lp64_equiv_profile.EXCLUDED)

        # Assert
        self.assertEqual(covered, fork)
        self.assertFalse(set(lp64_equiv_profile.ALL_ON) & set(lp64_equiv_profile.EXCLUDED))

    def test_all_profile_keeps_fonts_local_and_packs_out(self):
        # Act
        ini = lp64_equiv_profile.profile_ini('all')

        # Assert
        self.assertEqual(lp64_equiv_profile.ALL_ON['font_style'], 2)
        self.assertIn('texture_pack', lp64_equiv_profile.EXCLUDED)
        self.assertIn('font_style=2\n', ini)
        self.assertNotIn('texture_pack=', ini)

    def test_all_profile_lets_its_font_style_load(self):
        # Assert
        self.assertEqual(lp64_equiv_profile.profile_env('all'), {'GE_IRONSIGHTS_FONTS': '1'})
        self.assertEqual(lp64_equiv_profile.profile_env('default'), {})

    def test_default_profile_runs_windowed(self):
        # Assert
        self.assertEqual(lp64_equiv_profile.profile_ini('default'), '[display]\nfullscreen=windowed\n')


if __name__ == '__main__':
    unittest.main()
