"""End-to-end check of the in-game updater on a real executable (FORK-095, UPD-002).

    python scripts/port/update_e2e.py --exe build_win/upd-test/IronSights.exe --rom PATH [--work DIR]

EXE must be a GE_UPDATE_TEST build (`-DGE_UPDATE_TEST=ON`), which takes its
release address and build number from the environment. The check lays EXE out
as an installed windows-x86-zip package of build 100 in WORK/install, writes a
release of build 200 (the same executable plus a marker file) to
WORK/release, serves it on 127.0.0.1, and launches the install once with
`[updates] mode=automatic`. The game must find the release, download and
unpack it, swap its files while it runs, restart itself, and the new process
must reach the legal screen and confirm the install. It then checks the
files, the player data and the log. A real window opens for a few seconds;
sound is off. Windows only for now (the zip flavor). WORK defaults to
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
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import package_identity  # noqa: E402
import update_fixture  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
TAG = "build-200.1-0123456789ab"
FLAVOR = "windows-x86-zip"


def package_files(exe, extra):
    """The package's files: EXE and the SDL DLL beside it, package.txt, extra, SHA256SUMS.txt."""
    files = {"IronSights.exe": exe.read_bytes(), "SDL2.dll": (exe.parent / "SDL2.dll").read_bytes(),
             "Data/shaders_base.txt": (ROOT / "tools/shaders_base.txt").read_bytes(),
             package_identity.NAME: package_identity.text(FLAVOR)}
    files.update(extra)
    files[package_identity.SUMS] = package_identity.sums(files).encode()
    return files


def lay_out(work, exe, rom):
    install, release = work / "install", work / "release"
    for name, data in package_files(exe, {}).items():
        update_fixture.write(install / name, data)
    update_fixture.write(install / "midas.ini", (f"[display]\nfullscreen=windowed\n[data]\nrom={rom}\n"
                                                 "[updates]\nmode=automatic\n").encode())
    # The game rewrites eeprom.bin itself; a file it never touches stands for player data.
    update_fixture.write(install / "Data/Saves/updater-check.txt", b"player data")
    package = update_fixture.zip_bytes(package_files(exe, {"new-only.txt": b"from build 200"}))
    update_fixture.write(release / "Iron-Sights-windows-x86.zip", package)
    update_fixture.write(release / "SHA256SUMS.txt",
                         f"{hashlib.sha256(package).hexdigest()}  Iron-Sights-windows-x86.zip\n".encode())
    return install, release


def serve(release):
    """Serves RELEASE so that DOWNLOADS/latest/download/X and DOWNLOADS/download/TAG/X are its files."""
    import functools
    import http.server

    class Handler(http.server.SimpleHTTPRequestHandler):
        def translate_path(self, path):
            return str(release / Path(path.split("?")[0]).name)

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


def check(install, log):
    updates = install / "Data/Updates"
    state = (updates / "state.txt").read_text(encoding="utf-8") if (updates / "state.txt").is_file() else ""
    results = {
        "the new build's marker file is in place": (install / "new-only.txt").is_file(),
        "the old executable is gone after confirmation": not (install / "IronSights.exe.old").exists(),
        "the journal was confirmed and removed": not (updates / "journal.txt").exists(),
        "the staging folder is gone": not (updates / TAG).exists(),
        "the state records the install": f"installed: {TAG}" in state,
        "player data is untouched": (install / "Data/Saves/updater-check.txt").read_bytes() == b"player data",
        "midas.ini still asks for automatic updates": "mode=automatic" in (install / "midas.ini").read_text(),
        "the update log shows the install": f"installed (installed: {TAG})" in log,
        "the update log shows the restart": "restarted into the installed build" in log,
        "the new process counted its first start": "start 1 of build 200 after its install" in log,
        "the new process confirmed at the legal screen": "build 200 reached the legal screen" in log,
    }
    for name, ok in results.items():
        print(("PASS " if ok else "FAIL ") + name)
    return all(results.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--work", type=Path, default=ROOT / "build_win/update-e2e")
    parser.add_argument("--keep", action="store_true")
    args = parser.parse_args()
    if args.work.exists():
        shutil.rmtree(args.work)
    install, release = lay_out(args.work, args.exe.resolve(), args.rom.resolve())
    server, downloads = serve(release)
    env = dict(os.environ, GE_UPDATE_TEST_DOWNLOADS=downloads, GE_UPDATE_TEST_BUILD="100", GE_UPDATE_TEST_CI="1",
               SDL_AUDIODRIVER="dummy", GE_FRAMES="900", GE_WINDOW="640x480")
    exe = install / "IronSights.exe"
    first = subprocess.run([str(exe)], cwd=install, env=env, timeout=180)
    print(f"first process exited {first.returncode}")
    wait_for(lambda: running(exe), 10)
    wait_for(lambda: not running(exe), 180)
    server.shutdown()
    log_file = install / "Data/Updates/log.txt"
    log = log_file.read_text(encoding="utf-8", errors="replace") if log_file.is_file() else ""
    print(log, end="")
    ok = check(install, log)
    if not ok or args.keep:
        print(f"kept {args.work}")
    else:
        shutil.rmtree(args.work)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
