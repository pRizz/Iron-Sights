"""ROM-free subprocess checks for the native diagnostic and Deck launchers."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def executable(path, body):
    path.write_text("#!/bin/sh\n" + body + "\n", encoding="utf-8")
    path.chmod(0o755)


@unittest.skipUnless(os.name == "posix" and shutil.which("bash"), "Requires POSIX shell")
class NativeToolLaunchers(unittest.TestCase):
    def natalya(self, *, windows=False, running=False, game_status=0, missing=False):
        # Arrange: fake the game and process query; never launch an actual game.
        with tempfile.TemporaryDirectory(prefix="native tools ") as folder:
            work = Path(folder)
            executable(work / ("tasklist" if windows else "pgrep"),
                       "echo 'IronSights.exe 123 Console'" if running else "exit 1")
            game = work / "IronSights"
            marker = work / "launched"
            if not missing:
                executable(game, 'printf "%s\\n" "$GE_STAGE" "$GE_SPAWNXYZ" "$GE_FRAMES" > '
                           '"$LAUNCH_MARKER"\nexit ' + str(game_status))
            env = dict(os.environ, PATH=str(work) + os.pathsep + os.environ["PATH"],
                       GE_TEST_EXE=str(game), LAUNCH_MARKER=str(marker))

            # Act: invoke from outside the checkout, including spaces in its path.
            result = subprocess.run(
                ["bash", str(ROOT / "scripts/port/nat_test.sh"), "bunker1", "1,2,3,4,5", "4"],
                cwd=work, env=env, text=True, capture_output=True, timeout=15)
            maybe_launched = marker.read_text() if marker.exists() else None

            # Assert: callers inspect the outcome and whether the game ran.
            return result, maybe_launched

    def test_natalya_passes_arguments_from_any_directory(self):
        result, maybe_launched = self.natalya()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(maybe_launched, "bunker1\n1,2,3,4,5\n4\n")

    def test_natalya_propagates_game_failure(self):
        result, maybe_launched = self.natalya(game_status=17)
        self.assertIsNotNone(maybe_launched)
        self.assertEqual(result.returncode, 17)

    def test_natalya_checks_windows_process_before_launch(self):
        result, maybe_launched = self.natalya(windows=True, running=True)
        self.assertEqual(result.returncode, 1)
        self.assertIsNone(maybe_launched)
        self.assertIn("already running", result.stdout)

    def test_natalya_checks_linux_process_before_launch(self):
        result, maybe_launched = self.natalya(running=True)
        self.assertEqual(result.returncode, 1)
        self.assertIsNone(maybe_launched)
        self.assertIn("already running", result.stdout)

    def test_natalya_reports_missing_build(self):
        result, maybe_launched = self.natalya(missing=True)
        self.assertEqual(result.returncode, 1)
        self.assertIsNone(maybe_launched)
        self.assertIn("Game executable not found", result.stderr)

    def deck(self, missing_library):
        # Arrange: copy the exact shipped launcher beside a fake native player.
        with tempfile.TemporaryDirectory(prefix="deck tools ") as folder:
            work = Path(folder)
            bundle = work / "bundle with spaces"
            bundle.mkdir()
            shutil.copyfile(ROOT / "scripts/port/steamdeck_run.sh", bundle / "run.sh")
            executable(bundle / "IronSights",
                       'printf "%s\\n" "$PWD" "${LD_LIBRARY_PATH:-system}" "$@"\nexit 7')
            executable(work / "ldd", "echo 'libSDL2 => not found'" if missing_library
                       else "echo 'libSDL2 => ./libSDL2-2.0.so.0'")
            env = dict(os.environ, PATH=str(work) + os.pathsep + os.environ["PATH"])
            env.pop("LD_LIBRARY_PATH", None)

            # Act: start outside the bundle and forward an argument with spaces.
            result = subprocess.run(["sh", str(bundle / "run.sh"), "a b", "--flag"],
                                    cwd=work, env=env, text=True, capture_output=True,
                                    timeout=15)

            # Assert: the renamed player runs with the chosen SDL lookup path.
            self.assertEqual(result.returncode, 7, result.stderr)
            self.assertEqual(result.stdout.splitlines(),
                             [str(bundle), "system" if missing_library else ".", "a b", "--flag"])
            return result

    def test_deck_uses_bundled_sdl_and_forwards_arguments(self):
        self.deck(False)

    def test_deck_falls_back_to_system_sdl(self):
        result = self.deck(True)
        self.assertIn("using the system SDL2", result.stderr)


if __name__ == "__main__":
    unittest.main()
