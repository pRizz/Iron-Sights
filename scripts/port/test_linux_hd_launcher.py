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
    def test_readonly_bundle_and_writable_root_stay_separate(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            app = root / "bundle with spaces"
            (app / "Data/TexturePacks").mkdir(parents=True)
            shutil.copy2(HERE / "linux/AppRun", app / "AppRun")
            for name in ("gamecontrollerdb.txt", "gamecontrollerdefault.txt", "midas_ini_help.txt", "Data/shaders_base.txt"):
                (app / name).write_text("packaged default")
            (app / "Data/TexturePacks/hd.istex").write_text("bundled artwork")
            exe = app / "IronSights"
            exe.write_text('#!/bin/sh\nprintf \'%s\\n\' "$GE_DATA_ROOT" "$GE_BUNDLED_TEXTURE_ROOT" "$@"\n')
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
                    (data / "midas.ini").write_text("[display]\ntexture_pack=0\n")
                    env["GE_BUNDLED_TEXTURE_ROOT"] = "/stale-bundle"
                    result = subprocess.check_output(["sh", str(app / "AppRun"), "--argument with spaces"], cwd=root, env=env, text=True)
                    self.assertEqual(result.splitlines(), [str(data), str(app / "Data/TexturePacks"), "--argument with spaces"])
                    self.assertFalse((data / "Data/TexturePacks").exists())
                    self.assertEqual((data / "midas.ini").read_text(), "[display]\ntexture_pack=0\n")
                env["GE_DATA_ROOT"] = "relative"
                self.assertNotEqual(subprocess.run(["sh", str(app / "AppRun")], env=env, capture_output=True).returncode, 0)
            finally:
                app.chmod(0o755)


if __name__ == "__main__":
    unittest.main()
