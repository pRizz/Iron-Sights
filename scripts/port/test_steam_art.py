"""ROM-free checks for the Steam Deck library artwork and set-steam-art.py."""
import importlib.util
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

HERE = Path(__file__).resolve().parent
SCRIPT = HERE / "linux/set-steam-art.py"
ART = HERE / "linux/steam-art"
SPEC = importlib.util.spec_from_file_location("set_steam_art", SCRIPT)
steam_art = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(steam_art)


def encode_vdf(mapping):
    """Minimal binary KeyValues writer matching Steam's shortcuts.vdf layout."""
    out = bytearray()
    for key, value in mapping.items():
        name = key.encode() + b"\0"
        if isinstance(value, dict):
            out += b"\x00" + name + encode_vdf(value)
        elif isinstance(value, int):
            out += b"\x02" + name + struct.pack("<i", value)
        else:
            out += b"\x01" + name + str(value).encode() + b"\0"
    return bytes(out) + b"\x08"


def shortcut(exe, name="Iron Sights", appid=None, start_dir=""):
    entry = {"AppName": name, "Exe": f'"{exe}"', "StartDir": f'"{start_dir}"', "icon": "", "tags": {}}
    if appid is not None:
        entry["appid"] = appid
    return entry


def png_size(path):
    header = path.read_bytes()[:24]
    assert header[:8] == b"\x89PNG\r\n\x1a\n"
    return struct.unpack(">II", header[16:24])


class Artwork(unittest.TestCase):
    def test_images_match_steam_library_sizes(self):
        self.assertEqual(png_size(ART / "cover.png"), (600, 900))
        self.assertEqual(png_size(ART / "banner.png"), (920, 430))
        self.assertEqual(png_size(ART / "hero.png"), (1920, 620))

    def test_logo_fits_steam_logo_bounds(self):
        width, height = png_size(ART / "logo.png")
        self.assertLessEqual(width, 1280)
        self.assertLessEqual(height, 720)

    def test_script_ships_every_image_it_copies(self):
        self.assertEqual(sorted(p.name for p in ART.glob("*.png")), sorted(image for image, _ in steam_art.ARTWORK))


class ParseVdf(unittest.TestCase):
    def test_reads_nested_strings_and_signed_ids(self):
        data = encode_vdf({"shortcuts": {"0": shortcut("/games/run.sh", appid=-123)}}) + b"\x08"
        entry = steam_art.parse_vdf(data)["shortcuts"]["0"]
        self.assertEqual((entry["AppName"], entry["appid"], entry["tags"]), ("Iron Sights", -123, {}))

    def test_rejects_unknown_field_types(self):
        with self.assertRaises(ValueError):
            steam_art.parse_vdf(b"\x00shortcuts\0\x09bad\0")


class ShortcutMatching(unittest.TestCase):
    def test_matches_run_sh_in_folder(self):
        folder = Path(tempfile.gettempdir()) / "deck"
        self.assertTrue(steam_art.launches_folder(shortcut(folder / "run.sh"), folder))

    def test_matches_start_dir_in_folder(self):
        folder = Path(tempfile.gettempdir()) / "deck"
        self.assertTrue(steam_art.launches_folder(shortcut("/usr/bin/env", start_dir=folder), folder))

    def test_ignores_other_games(self):
        folder = Path(tempfile.gettempdir()) / "deck"
        other = Path(tempfile.gettempdir()) / "other"
        self.assertFalse(steam_art.launches_folder(shortcut(other / "run.sh", start_dir=other), folder))

    def test_field_names_are_case_insensitive(self):
        self.assertEqual(steam_art.field({"appname": "x"}, "AppName"), "x")


class AppId(unittest.TestCase):
    def test_signed_stored_id_becomes_unsigned(self):
        self.assertEqual(steam_art.app_id({"appid": -2}), 0xFFFFFFFE)

    def test_missing_id_uses_legacy_crc(self):
        entry = {"exe": '"/g/run.sh"', "appname": "Iron Sights"}
        self.assertEqual(steam_art.app_id(entry), zlib.crc32(b'"/g/run.sh"Iron Sights') | 0x80000000)


class GridPlan(unittest.TestCase):
    def test_names_follow_steam_grid_convention(self):
        targets = [target for _, target, _ in steam_art.grid_plan(42, [], False)]
        self.assertEqual(targets, ["42p.png", "42.png", "42_hero.png", "42_logo.png"])

    def test_keeps_existing_custom_artwork(self):
        plan = dict((target, reason) for _, target, reason in steam_art.grid_plan(42, ["42P.jpg", "421.png"], False))
        self.assertIn("42P.jpg", plan["42p.png"])
        self.assertIsNone(plan["42.png"])

    def test_force_replaces_existing_artwork(self):
        self.assertTrue(all(reason is None for _, _, reason in steam_art.grid_plan(42, ["42p.png"], True)))


class DesktopEntry(unittest.TestCase):
    def test_plain_path_is_unquoted(self):
        self.assertEqual(steam_art.desktop_exec("/home/deck/Iron/run.sh"), "/home/deck/Iron/run.sh")

    def test_reserved_characters_are_quoted_and_escaped(self):
        self.assertEqual(steam_art.desktop_exec('/home/a b/$x"100%/run.sh'), '"/home/a b/\\\\$x\\\\"100%%/run.sh"')

    def test_entry_points_at_run_sh_and_icon(self):
        text = steam_art.desktop_entry(PurePosixPath("/home/deck/Iron"))
        self.assertIn("Exec=/home/deck/Iron/run.sh\n", text)
        self.assertIn("Icon=/home/deck/Iron/iron-sights.png\n", text)


class Script(unittest.TestCase):
    def run_script(self, root, *args):
        env = dict(os.environ, HOME=str(root / "home"), XDG_DATA_HOME=str(root / "xdg"))
        return subprocess.run([sys.executable, str(root / "app with space/set-steam-art.py"), *args],
                              env=env, capture_output=True, text=True, check=True).stdout

    def make_layout(self, root, entries):
        app = root / "app with space"
        (app / "steam-art").mkdir(parents=True)
        shutil.copy2(SCRIPT, app / "set-steam-art.py")
        for image in ART.glob("*.png"):
            shutil.copy2(image, app / "steam-art" / image.name)
        (app / "run.sh").write_text("#!/bin/sh\n")
        config = root / "home/.local/share/Steam/userdata/1234/config"
        config.mkdir(parents=True)
        (config / "shortcuts.vdf").write_bytes(encode_vdf({"shortcuts": entries(app)}) + b"\x08")
        return app, config / "grid"

    def test_copies_artwork_for_matching_shortcut_only(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            app, grid = self.make_layout(root, lambda app: {
                "0": shortcut("/other/game", appid=7, start_dir="/other"),
                "1": shortcut(app / "run.sh", appid=-5)})
            self.run_script(root)
            unsigned = 0xFFFFFFFB
            self.assertEqual(sorted(os.listdir(grid)), sorted(f"{unsigned}{suffix}.png" for suffix in ("p", "", "_hero", "_logo")))
            self.assertEqual((grid / f"{unsigned}p.png").read_bytes(), (ART / "cover.png").read_bytes())

    def test_installs_menu_entry_and_leaves_shortcuts_unchanged(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            app, grid = self.make_layout(root, lambda app: {"0": shortcut(app / "run.sh", appid=9)})
            vdf = grid.parent / "shortcuts.vdf"
            before = vdf.read_bytes()
            self.run_script(root)
            entry = (root / "xdg/applications" / steam_art.DESKTOP_NAME).read_text()
            self.assertIn("Exec=" + steam_art.desktop_exec(app.resolve() / "run.sh"), entry)
            self.assertEqual(vdf.read_bytes(), before)

    def test_reports_when_no_shortcut_exists(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            _, grid = self.make_layout(root, lambda app: {})
            self.assertIn("No Steam shortcut", self.run_script(root))
            self.assertFalse(grid.exists())


if __name__ == "__main__":
    unittest.main()
