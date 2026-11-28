"""The in-game updater's real release cycle (FORK-095, UPD-004 of scripts/port/self_update_plan.md).

    python scripts/port/update_cycle.py prepare --from TAG --rom ROM [--work DIR]
        [--flavors F,...] [--modes automatic,notify]
    python scripts/port/update_cycle.py run [--work DIR] [--flavors F,...] [--modes M,...]
        [--image IMAGE] [--timeout SECONDS]

prepare downloads release TAG's packages from GitHub (each checked against the release's
SHA256SUMS file) into WORK/download/TAG and installs each flavor once a mode in
WORK/cases/FLAVOR-MODE, with a player's data beside it (update_cycle_checks.py): the ROM, a
save, packs, a midas.ini with a changed setting and the mode, and an edited controller file.

run launches each case as a player would, with no test switches: the Windows zips natively (a
640x480 window opens for a few seconds, sound off), the Linux tarballs, AppImages (with
APPIMAGE_EXTRACT_AND_RUN=1, data under the case's XDG folder) and Steam Deck ZIPs (through
run.sh) offscreen in IMAGE (midas-lp64-i686-video, which has Mesa's GL and EGL at both widths).
Each updates from the real latest release over the network. The run stops each case once
Data/Updates/log.txt shows its end: Automatic installed, restarted and confirmed at the legal
screen; Notify found the newer build. It then checks the program files, the player data and
the log, prints PASS or FAIL a check, writes WORK/cycle.json, and exits 1 on any failure.
TAG must be older than the latest release and new enough to have the updater (build-457.1 or
later; build-463.1 or later for Automatic by default, which this does not rely on).
WORK defaults to build_win/update-cycle; it holds a ROM copy a case, so delete it afterwards.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parent))
import package_identity  # noqa: E402
import update_cycle_checks as checks  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
REPO = "pRizz/Iron-Sights"
IMAGE = "midas-lp64-i686-video"


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "IronSights-update-cycle"})
    with urllib.request.urlopen(request, timeout=120) as response:
        return response.read()


def release_file(work, tag, name):
    """Release TAG's asset NAME, downloaded once into WORK/download/TAG."""
    path = work / "download" / tag / name
    if not path.is_file():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(fetch(f"https://github.com/{REPO}/releases/download/{tag}/{name}"))
    return path.read_bytes()


def listed_sha(sums, name):
    """NAME's digest in a sha256sum listing (text "  " or binary " *" form)."""
    for line in sums.decode("utf-8").splitlines():
        parts = line.split(None, 1)
        if len(parts) == 2 and parts[1].lstrip("*") == name:
            return parts[0]
    return None


def package_of(work, tag, flavor):
    """FLAVOR's package name and bytes from release TAG, checked against its SHA256SUMS file."""
    package, sums, _ = package_identity.FLAVORS[flavor]
    data = release_file(work, tag, package)
    if checks.sha256(data) != listed_sha(release_file(work, tag, sums), package):
        raise SystemExit(f"{tag}/{package} does not match {sums}")
    return package, data


def selected(text, choices, what):
    names = list(choices) if text is None else text.split(",")
    unknown = [name for name in names if name not in choices]
    if unknown:
        raise SystemExit(f"unknown {what}: {', '.join(unknown)}")
    return names


def prepare(args):
    rom = args.rom.read_bytes()
    for flavor in selected(args.flavors, package_identity.FLAVORS, "flavor"):
        package, data = package_of(args.work, args.tag, flavor)
        info = json.loads(release_file(args.work, args.tag, package_identity.FLAVORS[flavor][2]))
        for mode in selected(args.modes, checks.MODES, "mode"):
            case = args.work / "cases" / f"{flavor}-{mode}"
            shutil.rmtree(case, ignore_errors=True)
            checks.lay_out(case, flavor, mode, package, data, rom, info["build_number"])
            print(f"{case.name}: build {info['build_number']} from {args.tag}")


def linux_command(flavor, package):
    start = {"tar": "chmod +x IronSights sentry-crash && exec ./IronSights",
             "deck": "exec sh run.sh",
             "appimage": f"chmod +x {package} && exec ./{package}"}[checks.kind(flavor)]
    return f"cd /case/game && {start} > /case/run.log 2>&1"


def start_case(case, before, image):
    """Starts CASE; returns a function that stops it and one that says whether it may still run."""
    flavor = before["flavor"]
    if flavor.startswith("windows-"):
        game = case / "game"
        env = dict(os.environ, SDL_AUDIODRIVER="dummy", GE_WINDOW="640x480")
        for name in ("GE_NOUPDATE", "GE_FRAMES"):
            env.pop(name, None)
        with open(case / "run.log", "wb") as out:
            subprocess.Popen([str(game / "IronSights.exe")], cwd=game, env=env, stdout=out, stderr=out)
        # The restarted process is not our child, so only the log tells when it is done.
        return (lambda: stop_windows(game)), (lambda: True)
    name = f"midas-update-cycle-{case.name}"
    subprocess.run(["docker", "rm", "-f", name], capture_output=True)
    # Root: files Windows wrote into the mount belong to root there, and run.sh and the tarball's
    # start need chmod. A Linux restart keeps the process (execv), so the container's end is the game's.
    command = ["docker", "run", "-d", "--user", "0", "--name", name, "-v", f"{case.resolve()}:/case",
               "-e", "SDL_VIDEODRIVER=offscreen", "-e", "SDL_AUDIODRIVER=dummy", "-e", "GE_WINDOW=640x480"]
    if checks.kind(flavor) == "appimage":
        command += ["-e", "XDG_DATA_HOME=/case/xdg", "-e", "APPIMAGE_EXTRACT_AND_RUN=1"]
    subprocess.run(command + [image, "sh", "-c", linux_command(flavor, before["package"])],
                   check=True, capture_output=True)
    state = ["docker", "inspect", "-f", "{{.State.Running}}", name]
    return ((lambda: subprocess.run(["docker", "rm", "-f", name], capture_output=True)),
            (lambda: subprocess.run(state, capture_output=True, text=True).stdout.strip() == "true"))


def stop_windows(game):
    """Ends the processes that run an executable in GAME (the restarted one is not our child)."""
    script = ("Get-Process IronSights -ErrorAction SilentlyContinue | "
              f"Where-Object {{ $_.Path -like '{game.resolve()}\\*' }} | Stop-Process -Force")
    subprocess.run(["powershell", "-NoProfile", "-Command", script], capture_output=True)


def update_log(case, before):
    path = checks.data_root(case, before["flavor"]) / "Data/Updates/log.txt"
    return path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""


def run_case(work, case, image, timeout):
    before = json.loads((case / "before.json").read_text(encoding="utf-8"))
    began = time.time()
    stop, alive = start_case(case, before, image)
    try:
        while time.time() - began < timeout and not checks.finished(update_log(case, before), before["mode"]):
            if not alive():
                print(f"{case.name}: the game ended early; run.log ends:\n"
                      + (case / "run.log").read_text(encoding="utf-8", errors="replace")[-2000:])
                break
            time.sleep(0.5)
        time.sleep(2)  # the confirmation's deletions follow its log line
    finally:
        stop()
    log = update_log(case, before)
    seconds = round(time.time() - began)
    new_sha, tag = None, checks.installed_tag(log)
    if tag and checks.kind(before["flavor"]) == "appimage":
        package, sums, _ = package_identity.FLAVORS[before["flavor"]]
        new_sha = listed_sha(release_file(work, tag, sums), package)
    results = checks.verdict(case, before, log, new_sha)
    print(f"== {case.name} ({seconds} s)\n{log}", end="")
    for name, ok in results.items():
        print(f"{'PASS' if ok else 'FAIL'} {case.name}: {name}", flush=True)
    return {"case": case.name, "seconds": seconds, "installed": tag, "checks": results}


def run(args):
    flavors = selected(args.flavors, package_identity.FLAVORS, "flavor")
    modes = selected(args.modes, checks.MODES, "mode")
    cases = [args.work / "cases" / f"{flavor}-{mode}" for flavor in flavors for mode in modes]
    missing = [case.name for case in cases if not (case / "before.json").is_file()]
    if missing:
        raise SystemExit(f"not prepared: {', '.join(missing)}")
    records = [run_case(args.work, case, args.image, args.timeout) for case in cases]
    (args.work / "cycle.json").write_text(json.dumps(records, indent=1) + "\n", encoding="utf-8")
    failed = [record["case"] for record in records if not all(record["checks"].values())]
    print(f"{len(records) - len(failed)} of {len(records)} cases passed" + (f"; failed: {', '.join(failed)}"
                                                                         if failed else ""))
    return 1 if failed else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("prepare", "run"):
        command = sub.add_parser(name)
        command.add_argument("--work", type=Path, default=ROOT / "build_win/update-cycle")
        command.add_argument("--flavors")
        command.add_argument("--modes")
    sub.choices["prepare"].add_argument("--from", dest="tag", required=True)
    sub.choices["prepare"].add_argument("--rom", type=Path, required=True)
    sub.choices["run"].add_argument("--image", default=IMAGE)
    sub.choices["run"].add_argument("--timeout", type=float, default=240)
    args = parser.parse_args(argv)
    return prepare(args) or 0 if args.command == "prepare" else run(args)


if __name__ == "__main__":
    sys.exit(main())
