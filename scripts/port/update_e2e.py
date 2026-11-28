"""End-to-end check of the in-game updater on a real executable (FORK-095, UPD-002 and UPD-003).

    python scripts/port/update_e2e.py --exe build_win/upd-test/IronSights.exe --rom PATH
        [--work DIR] [--mode automatic|notify|both] [--slow SECONDS] [--shots N] [--keep]

EXE must be a GE_UPDATE_TEST build (`-DGE_UPDATE_TEST=ON`), which takes its
release address and build number from the environment. Each case lays EXE out
as an installed windows-x86-zip package of build 100 in WORK/MODE/install,
writes a release of build 200 (the same executable plus a marker file) to
WORK/MODE/release, serves it on 127.0.0.1, and launches the install once with
`[updates] mode=MODE`.

automatic: the game must find the release while the legal screen holds,
download and unpack it, swap its files while it runs, restart itself, and the
new process must reach the legal screen and confirm the install. notify: the
game must find the release in the background and install nothing.

Each case then checks the files, the player data and Data/Updates/log.txt. A
real window opens for a few seconds; sound is off. --slow spreads the package
over SECONDS, so the legal screen's indicator stays up, and --shots N keeps a
screenshot of every Nth frame (shot_*.bmp in the install folder; keep them with
--keep). Windows only for now (the zip flavor). WORK defaults to
build_win/update-e2e and is deleted on success unless --keep.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
import package_identity  # noqa: E402
import update_fixture  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
TAG = "build-200.1-0123456789ab"
FLAVOR = "windows-x86-zip"
PACKAGE = "Iron-Sights-windows-x86.zip"


def package_files(exe, extra):
    """The package's files: EXE and the SDL DLL beside it, package.txt, extra, SHA256SUMS.txt."""
    files = {"IronSights.exe": exe.read_bytes(), "SDL2.dll": (exe.parent / "SDL2.dll").read_bytes(),
             "Data/shaders_base.txt": (ROOT / "tools/shaders_base.txt").read_bytes(),
             package_identity.NAME: package_identity.text(FLAVOR)}
    files.update(extra)
    files[package_identity.SUMS] = package_identity.sums(files).encode()
    return files


def lay_out(work, exe, rom, mode):
    install, release = work / "install", work / "release"
    for name, data in package_files(exe, {}).items():
        update_fixture.write(install / name, data)
    update_fixture.write(install / "midas.ini", (f"[display]\nfullscreen=windowed\n[data]\nrom={rom}\n"
                                                 f"[updates]\nmode={mode}\n").encode())
    # The game rewrites eeprom.bin itself; a file it never touches stands for player data.
    update_fixture.write(install / "Data/Saves/updater-check.txt", b"player data")
    package = update_fixture.zip_bytes(package_files(exe, {"new-only.txt": b"from build 200"}))
    update_fixture.write(release / PACKAGE, package)
    update_fixture.write(release / "SHA256SUMS.txt", f"{hashlib.sha256(package).hexdigest()}  {PACKAGE}\n".encode())
    return install, release


def serve(release, slow):
    """Serves RELEASE so that DOWNLOADS/latest/download/X and DOWNLOADS/download/TAG/X are its files.

    With slow, the package goes out in pieces over that many seconds."""
    import functools
    import http.server

    class Handler(http.server.SimpleHTTPRequestHandler):
        def translate_path(self, path):
            return str(release / Path(path.split("?")[0]).name)

        def copyfile(self, source, outputfile):
            if not slow or not self.path.endswith(PACKAGE):
                return super().copyfile(source, outputfile)
            data = source.read()
            pieces = 40
            step = max(1, len(data) // pieces)
            for at in range(0, len(data), step):
                outputfile.write(data[at:at + step])
                outputfile.flush()
                time.sleep(slow / pieces)

        def log_message(self, *args):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Handler, directory=str(release)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    downloads = f"http://127.0.0.1:{server.server_address[1]}/releases"
    info = dict(schema=1, build_number=200, ci=True, dirty=False, release_url=f"{downloads}/tag/{TAG}")
    update_fixture.write(release / "build-info.json", (json.dumps(info) + "\n").encode())
    return server, downloads


def wait_for(condition, seconds):
    deadline = time.time() + seconds
    while time.time() < deadline:
        if condition():
            return True
        time.sleep(0.5)
    return condition()


def running(exe):
    """True while a process runs EXE (the restarted one is not our child)."""
    listing = subprocess.run(["powershell", "-NoProfile", "-Command",
                              "Get-Process IronSights -ErrorAction SilentlyContinue | ForEach-Object Path"],
                             capture_output=True, text=True).stdout
    return str(exe).lower() in listing.lower()


def state_of(install):
    path = install / "Data/Updates/state.txt"
    return path.read_text(encoding="utf-8") if path.is_file() else ""


def automatic_results(install, log):
    updates = install / "Data/Updates"
    state = state_of(install)
    return {
        "the launch's run started on the first frame": "looking for a build to install at launch" in log,
        "the new build's marker file is in place": (install / "new-only.txt").is_file(),
        "the old executable is gone after confirmation": not (install / "IronSights.exe.old").exists(),
        "the journal was confirmed and removed": not (updates / "journal.txt").exists(),
        "the staging folder is gone": not (updates / TAG).exists(),
        "the state records the install": f"installed: {TAG}" in state and "outcome=4" in state,
        "player data is untouched": (install / "Data/Saves/updater-check.txt").read_bytes() == b"player data",
        "midas.ini still asks for automatic updates": "mode=automatic" in (install / "midas.ini").read_text(),
        "the update log shows the install": f"installed (installed: {TAG})" in log,
        "the update log shows the restart": "restarted into the installed build" in log,
        "the new process counted its first start": "start 1 of build 200 after its install" in log,
        "the new process confirmed at the legal screen": "build 200 reached the legal screen" in log,
    }


def notify_results(install, log):
    state = state_of(install)
    return {
        "the launch's check started on the first frame": "checking for a newer build at launch" in log,
        "the check found build 200": "available (a newer build is available: 200)" in log,
        "the state names the new build and its tag": "newest=200" in state and f"newest_tag={TAG}" in state
                                                     and "outcome=5" in state,
        "nothing was installed": not (install / "new-only.txt").exists()
                                 and not (install / "Data/Updates/journal.txt").exists(),
        "the game did not restart": "restart" not in log,
        "player data is untouched": (install / "Data/Saves/updater-check.txt").read_bytes() == b"player data",
    }


def run_case(work, exe_source, rom, mode, slow, shots):
    install, release = lay_out(work, exe_source, rom, mode)
    server, downloads = serve(release, slow)
    env = dict(os.environ, GE_UPDATE_TEST_DOWNLOADS=downloads, GE_UPDATE_TEST_BUILD="100", GE_UPDATE_TEST_CI="1",
               SDL_AUDIODRIVER="dummy", GE_WINDOW="640x480",
               # 900 frames, plus room for a slowed download (frames count retraces, 120 a second)
               GE_FRAMES=str(900 + int(slow * 240)))
    env.pop("GE_NOUPDATE", None)
    if shots:
        env["GE_SHOT"] = str(shots)
    exe = install / "IronSights.exe"
    first = subprocess.run([str(exe)], cwd=install, env=env, timeout=240)
    print(f"{mode}: first process exited {first.returncode}")
    if mode == "automatic":
        wait_for(lambda: running(exe), 10)
        wait_for(lambda: not running(exe), 240)
    server.shutdown()
    log_file = install / "Data/Updates/log.txt"
    log = log_file.read_text(encoding="utf-8", errors="replace") if log_file.is_file() else ""
    print(log, end="")
    results = (automatic_results if mode == "automatic" else notify_results)(install, log)
    for name, ok in results.items():
        print(f"{'PASS' if ok else 'FAIL'} {mode}: {name}")
    return all(results.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--work", type=Path, default=ROOT / "build_win/update-e2e")
    parser.add_argument("--mode", choices=("automatic", "notify", "both"), default="both")
    parser.add_argument("--slow", type=float, default=0.0)
    parser.add_argument("--shots", type=int, default=0)
    parser.add_argument("--keep", action="store_true")
    args = parser.parse_args()
    if args.work.exists():
        shutil.rmtree(args.work)
    modes = ("automatic", "notify") if args.mode == "both" else (args.mode,)
    ok = all([run_case(args.work / mode, args.exe.resolve(), args.rom.resolve(), mode, args.slow, args.shots)
              for mode in modes])
    if not ok or args.keep:
        print(f"kept {args.work}")
    else:
        shutil.rmtree(args.work)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
