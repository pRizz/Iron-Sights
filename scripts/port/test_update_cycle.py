"""ROM-free checks of update_cycle_checks.py (FORK-095, UPD-004): layouts and verdicts."""
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import package_identity  # noqa: E402
import update_cycle  # noqa: E402
import update_cycle_checks as checks  # noqa: E402

ROM = b"not a rom" * 10
TAG = "build-500.1-0123456789ab"


def files(flavor, build):
    names = {"IronSights": b"exe %d" % build, checks.EDITED: b"controls\n", "Data/README.txt": b"readme\n",
             "build-info.json": json.dumps({"build_number": build}).encode(),
             package_identity.NAME: package_identity.text(flavor)}
    names[package_identity.SUMS] = package_identity.sums(names).encode()
    return names


def zip_of(entries, top=""):
    out = io.BytesIO()
    with zipfile.ZipFile(out, "w") as archive:
        for name, data in entries.items():
            archive.writestr(top + name, data)
    return out.getvalue()


def tar_of(entries, top):
    out = io.BytesIO()
    with tarfile.open(fileobj=out, mode="w:gz") as archive:
        for name, data in entries.items():
            info = tarfile.TarInfo(top + name)
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))
    return out.getvalue()


def install(game, entries):
    """What a confirmed install leaves: the new package's files."""
    for name, data in entries.items():
        (game / name).parent.mkdir(parents=True, exist_ok=True)
        (game / name).write_bytes(data)


AUTOMATIC_LOG = (f"1 build 100: looking for a build to install at launch\n"
                 f"2 build 100: installed (installed: {TAG})\n3 build 100: restarted into the installed build\n"
                 f"4 build 101: start 1 of build 101 after its install\n"
                 f"5 build 101: build 101 reached the legal screen; the previous build's files are removed\n")
NOTIFY_LOG = ("1 build 100: checking for a newer build at launch\n"
              "2 build 100: available (a newer build is available: 101)\n")


class Layout(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.case = Path(self.temp.name) / "case"

    def tearDown(self):
        self.temp.cleanup()

    def test_kinds(self):
        self.assertEqual(checks.kind("windows-x64-zip"), "zip")
        self.assertEqual(checks.kind("linux-i686-tar"), "tar")
        self.assertEqual(checks.kind("linux-x86_64-appimage"), "appimage")
        self.assertEqual(checks.kind("steamdeck-i686"), "deck")
        with self.assertRaises(ValueError):
            checks.kind("macos-app")

    def test_tarball_loses_its_top_folder_and_gets_player_data(self):
        data = tar_of(files("linux-i686-tar", 100), "Iron-Sights-linux-i686/")
        before = checks.lay_out(self.case, "linux-i686-tar", "automatic", "x.tar.gz", data, ROM, 100)
        game = self.case / "game"
        self.assertTrue((game / "IronSights").is_file())
        self.assertEqual((game / checks.ROM).read_bytes(), ROM)
        self.assertIn("mode=automatic", (game / "midas.ini").read_text())
        self.assertTrue((game / checks.EDITED).read_bytes().endswith(checks.EDIT_MARK))
        self.assertEqual(before["build"], 100)
        self.assertEqual(json.loads((self.case / "before.json").read_text()), before)

    def test_appimage_data_goes_to_its_xdg_folder(self):
        checks.lay_out(self.case, "linux-x86_64-appimage", "notify", "a.AppImage", b"image", ROM, 100)
        self.assertEqual((self.case / "game/a.AppImage").read_bytes(), b"image")
        self.assertTrue((self.case / "xdg/iron-sights" / checks.ROM).is_file())
        self.assertFalse((self.case / "game/Data").exists())

    def test_tar_links_are_refused(self):
        out = io.BytesIO()
        with tarfile.open(fileobj=out, mode="w:gz") as archive:
            link = tarfile.TarInfo("top/lib/libx.so")
            link.type, link.linkname = tarfile.SYMTYPE, "libx.so.1"
            archive.addfile(link)
        with self.assertRaises(ValueError):
            checks.unpack("p.tar.gz", out.getvalue(), self.case)


class Verdicts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.case = Path(self.temp.name) / "case"

    def tearDown(self):
        self.temp.cleanup()

    def zip_case(self, mode):
        return checks.lay_out(self.case, "windows-x86-zip", mode, "w.zip", zip_of(files("windows-x86-zip", 100)),
                              ROM, 100)

    def test_finished(self):
        self.assertFalse(checks.finished(AUTOMATIC_LOG.splitlines(True)[1], "automatic"))
        self.assertTrue(checks.finished(AUTOMATIC_LOG, "automatic"))
        self.assertTrue(checks.finished("1 build 100: failed (no network)\n", "automatic"))
        self.assertTrue(checks.finished(NOTIFY_LOG, "notify"))
        self.assertFalse(checks.finished("", "notify"))

    def test_automatic_install_passes(self):
        before = self.zip_case("automatic")
        game = self.case / "game"
        edited = (game / checks.EDITED).read_bytes()
        install(game, files("windows-x86-zip", 101))
        (game / (checks.EDITED + ".edited")).write_bytes(edited)
        results = checks.verdict(self.case, before, AUTOMATIC_LOG)
        self.assertTrue(all(results.values()), results)

    def test_automatic_catches_a_left_old_file_and_changed_save(self):
        before = self.zip_case("automatic")
        game = self.case / "game"
        install(game, files("windows-x86-zip", 101))
        (game / "IronSights.old").write_bytes(b"old")
        (game / "Data/Saves/cycle-check.txt").write_bytes(b"lost")
        results = checks.verdict(self.case, before, AUTOMATIC_LOG)
        self.assertFalse(results["no .old file is left"])
        self.assertFalse(results["player data is unchanged (ROM, save, packs)"])
        self.assertFalse(results["the edited file was kept as .edited"])

    def test_automatic_without_install_fails(self):
        before = self.zip_case("automatic")
        results = checks.verdict(self.case, before, "1 build 100: failed (no network)\n")
        self.assertFalse(results["the launch installed a newer release"])
        self.assertFalse(results["the program files are the new package's"])

    def test_notify_passes_and_catches_an_install(self):
        before = self.zip_case("notify")
        self.assertTrue(all(checks.verdict(self.case, before, NOTIFY_LOG).values()))
        install(self.case / "game", files("windows-x86-zip", 101))
        self.assertFalse(checks.verdict(self.case, before, NOTIFY_LOG)["nothing was installed"])

    def test_appimage_automatic_compares_the_image(self):
        before = checks.lay_out(self.case, "linux-i686-appimage", "automatic", "a.AppImage", b"old", ROM, 100)
        (self.case / "game/a.AppImage").write_bytes(b"new")
        new_sha = checks.sha256(b"new")
        self.assertTrue(all(checks.verdict(self.case, before, AUTOMATIC_LOG, new_sha).values()))
        self.assertFalse(checks.verdict(self.case, before, AUTOMATIC_LOG, checks.sha256(b"other"))[
            "the AppImage is the new release's"])


class Tool(unittest.TestCase):
    def test_listed_sha_and_commands(self):
        sums = b"aa" * 32 + b"  Iron-Sights-linux-i686.tar.gz\n" + b"bb" * 32 + b" *x.AppImage\n"
        self.assertEqual(update_cycle.listed_sha(sums, "Iron-Sights-linux-i686.tar.gz"), "aa" * 32)
        self.assertIsNone(update_cycle.listed_sha(sums, "missing.zip"))
        self.assertIn("exec sh run.sh", update_cycle.linux_command("steamdeck-x86_64", "p.zip"))
        self.assertIn("./a.AppImage", update_cycle.linux_command("linux-i686-appimage", "a.AppImage"))

    def test_selected_refuses_unknown_names(self):
        self.assertEqual(update_cycle.selected(None, checks.MODES, "mode"), list(checks.MODES))
        with self.assertRaises(SystemExit):
            update_cycle.selected("automatic,sometimes", checks.MODES, "mode")


if __name__ == "__main__":
    unittest.main()
