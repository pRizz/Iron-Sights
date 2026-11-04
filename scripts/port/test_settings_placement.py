#!/usr/bin/env python3
"""ROM-free checks for the Iron Sights settings placement rule."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import settings_placement as placement  # noqa: E402

OPTIONS = '''
    TOGGLE("sprint",      "[display]", "Sprint\\n",      PORTOPT_CAT_GAMEPLAY),
    INTR  ("music_volume", "[display]", "Music Volume\\n", PORTOPT_CAT_AUDIO, 0, 100, 5, "%"),
    ENUMR ("weapon_hud_icons", "[display]", "Weapon Strip Style\\n", PORTOPT_CAT_GAMEPLAY, a, b, NULL),
'''
FOLDER = '''
static const struct PortSheetRow port_sheet_rows_audio[] = {
    { SHROW_OPT, "music_volume", "Music\\n" },
};
static const struct PortSheetRow port_sheet_rows_is_aim[] = {
    { SHROW_OPT, "sprint" },
#if !defined(__EMSCRIPTEN__)
    { SHROW_OPT, "weapon_hud_icons" },
#endif
};
'''
WATCH = '''
static const struct PortWatchRowDef sAudio[] = {
    { PWROW_OPT,       "music_volume", "Music\\n", 0, 0 },
    { PWROW_BACK,      NULL, "Back\\n", 0, 0 },
};
static const struct PortWatchRowDef sIsAim[] = {
    { PWROW_OPT,       "sprint",           NULL, 0, 0 },
    { PWROW_OPT,       "weapon_hud_icons", NULL, 0, 0 },
};
'''
UPSTREAM = {'music_volume'}


def errors(folder=FOLDER, watch=WATCH, upstream=UPSTREAM):
    return placement.placement_errors(
        placement.option_keys(OPTIONS), upstream,
        placement.menu_tables(folder, placement.FOLDER_TABLE, placement.FOLDER_KEY),
        placement.menu_tables(watch, placement.WATCH_TABLE, placement.WATCH_KEY))


class PlacementTest(unittest.TestCase):
    def test_reads_every_option_macro(self):
        # Arrange / act / assert
        self.assertEqual(placement.option_keys(OPTIONS), {'sprint', 'music_volume', 'weapon_hud_icons'})

    def test_accepts_fork_settings_on_both_iron_sights_menus(self):
        self.assertEqual(errors(), [])

    def test_rejects_a_fork_setting_missing_from_the_watch(self):
        # Arrange: the watch forgets Sprint
        watch = WATCH.replace('    { PWROW_OPT,       "sprint",           NULL, 0, 0 },\n', '')

        # Act / assert
        self.assertEqual(errors(watch=watch),
                         ['sprint: Iron Sights setting missing from the watch Iron Sights pages '
                          '(port/src/port_watchpages.c)'])

    def test_rejects_a_fork_setting_on_an_upstream_page(self):
        folder = FOLDER.replace('"music_volume", "Music\\n" },', '"music_volume", "Music\\n" }, { SHROW_OPT, "sprint" },')

        self.assertEqual(errors(folder=folder),
                         ['sprint: Iron Sights setting on upstream page port_sheet_rows_audio (src/game/front.c)'])

    def test_rejects_an_upstream_setting_on_an_iron_sights_page(self):
        self.assertIn('weapon_hud_icons: upstream setting on an Iron Sights page (src/game/front.c)',
                      errors(upstream=UPSTREAM | {'weapon_hud_icons'}))

    def test_repository_menus_follow_the_rule(self):
        self.assertEqual(placement.check(), [])


if __name__ == '__main__':
    unittest.main()
