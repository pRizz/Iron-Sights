"""Exercise the real AppRun without a ROM, display or native executable."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent


@unittest.skipUnless(os.name == "posix", "AppRun requires a POSIX shell")
class LinuxLauncher(unittest.TestCase):
    def test_steamdeck_launcher_finds_bundle_and_preserves_arguments(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            app = root / "Steam Deck bundle with spaces"
            (app / "lib").mkdir(parents=True)
            shutil.copy2(HERE / "linux/steamdeck-run.sh", app / "run.sh")
            exe = app / "IronSights"
            exe.write_text('#!/bin/sh\nprintf \'%s\\n\' "$PWD" "$LD_LIBRARY_PATH" "$@"\n')
            helper = app / "sentry-crash"
            helper.write_text("helper")
            exe.chmod(0o644)
            helper.chmod(0o644)
            env = {k: v for k, v in os.environ.items() if k != "LD_LIBRARY_PATH"}
            result = subprocess.check_output(["sh", str(app / "run.sh"), "two words"],
                                             cwd=root, env=env, text=True)
            self.assertEqual(result.splitlines(), [str(app), str(app / "lib"), "two words"])
            self.assertTrue(exe.stat().st_mode & 0o111)
            self.assertTrue(helper.stat().st_mode & 0o111)

    def test_readonly_bundle_and_writable_root_stay_separate(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            app = root / "bundle with spaces"
            (app / "Data").mkdir(parents=True)
            shutil.copy2(HERE / "linux/AppRun", app / "AppRun")
            for name in ("gamecontrollerdb.txt", "gamecontrollerdefault.txt", "midas_ini_help.txt", "Data/shaders_base.txt"):
                (app / name).write_text("packaged default")
            exe = app / "IronSights"
            exe.write_text('#!/bin/sh\nprintf \'%s\\n\' "$GE_DATA_ROOT" "$@"\n')
            exe.chmod(0o555)
            app.chmod(0o555)
            env = {k: v for k, v in os.environ.items() if not k.startswith("GE_") and k != "APPDIR"}
            env.update(HOME=str(root), XDG_DATA_HOME=str(root / "xdg"))
            try:
                for override in (None, str(root / "custom data")):
                    if override:
                        env["GE_DATA_ROOT"] = override
                    data = Path(override) if override else root / "xdg/iron-sights"
                    data.mkdir(parents=True)
                    (data / "midas.ini").write_text("[display]\nfullscreen=window\n")
                    result = subprocess.check_output(["sh", str(app / "AppRun"), "--argument with spaces"], cwd=root, env=env, text=True)
                    self.assertEqual(result.splitlines(), [str(data), "--argument with spaces"])
                    self.assertEqual((data / "Data/shaders_base.txt").read_text(), "packaged default")
                    self.assertEqual((data / "midas.ini").read_text(), "[display]\nfullscreen=window\n")
                env["GE_DATA_ROOT"] = "relative"
                self.assertNotEqual(subprocess.run(["sh", str(app / "AppRun")], env=env, capture_output=True).returncode, 0)
            finally:
                app.chmod(0o755)

    def test_an_updated_appimage_refreshes_its_own_files_but_not_the_controller_files(self):
        with tempfile.TemporaryDirectory() as work:
            # Arrange: a data root seeded by an older AppImage, one controller file edited
            root = Path(work)
            app = root / "bundle"
            (app / "Data").mkdir(parents=True)
            shutil.copy2(HERE / "linux/AppRun", app / "AppRun")
            for name in ("gamecontrollerdb.txt", "gamecontrollerdefault.txt", "midas_ini_help.txt", "Data/shaders_base.txt"):
                (app / name).write_text("new build")
            exe = app / "IronSights"
            exe.write_text("#!/bin/sh\n")
            exe.chmod(0o755)
            data = root / "data"
            (data / "Data").mkdir(parents=True)
            for name in ("gamecontrollerdb.txt", "midas_ini_help.txt", "Data/shaders_base.txt"):
                (data / name).write_text("old build")
            (data / "gamecontrollerdefault.txt").write_text("edited by the player")
            env = {k: v for k, v in os.environ.items() if not k.startswith("GE_") and k != "APPDIR"}
            env.update(HOME=str(root), GE_DATA_ROOT=str(data))
            # Act
            subprocess.check_call(["sh", str(app / "AppRun")], cwd=root, env=env)
            # Assert
            self.assertEqual((data / "midas_ini_help.txt").read_text(), "new build")
            self.assertEqual((data / "Data/shaders_base.txt").read_text(), "new build")
            self.assertEqual((data / "gamecontrollerdb.txt").read_text(), "old build")
            self.assertEqual((data / "gamecontrollerdefault.txt").read_text(), "edited by the player")


if __name__ == "__main__":
    unittest.main()
