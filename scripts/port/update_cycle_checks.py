"""Layouts and verdicts for update_cycle.py (FORK-095, UPD-004): no network, no processes.

A case is one released package installed in CASE/game with a player's data beside it (or, for
an AppImage, in CASE/xdg/iron-sights, its XDG data folder): a ROM, a save, a texture pack, a font
pack, a midas.ini with a changed setting and the case's mode, and, outside an AppImage, a
controller file the player edited. CASE/before.json records what the checks compare with.
"""
import hashlib
import io
import json
from pathlib import Path
import re
import tarfile
import zipfile

import package_identity

PLAYER_FILES = {
    "Data/Saves/cycle-check.txt": b"a file the game never writes stands for the saves\n",
    "Data/TexturePacks/cycle-pack/README.txt": b"a texture pack the player copied in\n",
    "Data/FontPacks/gepd/cycle-check.txt": b"the Enhanced fonts' folder\n",
}
ROM = "Data/baserom.u.z64"
EDITED = "gamecontrollerdefault.txt"
EDIT_MARK = b"\n# a line the player added (update_cycle.py)\n"
MODES = ("automatic", "notify")
RESULTS = "not eligible|not due|up to date|failed|installed|available|cancelled"
RESULT = re.compile(rf"^\d+ build \d+: ({RESULTS}) \(", re.M)


def kind(flavor):
    """zip, tar, appimage or deck: how the flavor is unpacked and started."""
    if flavor not in package_identity.FLAVORS:
        raise ValueError(f"unknown package flavor {flavor!r}")
    if flavor.endswith("-appimage"):
        return "appimage"
    if flavor.startswith("steamdeck-"):
        return "deck"
    return "tar" if flavor.endswith("-tar") else "zip"


def data_root(case, flavor):
    """Where the player's Data and midas.ini live for FLAVOR."""
    return case / "xdg/iron-sights" if kind(flavor) == "appimage" else case / "game"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    return sha256(path.read_bytes()) if path.is_file() else None


def ini_text(mode):
    return f"[display]\nfullscreen=windowed\n[updates]\nmode={mode}\n"


def _strip_top(names):
    """The package's top folder ("" when its entries do not share one)."""
    tops = {name.split("/", 1)[0] for name in names}
    if len(tops) == 1 and all("/" in name for name in names):
        return next(iter(tops)) + "/"
    return ""


def unpack(name, data, dest):
    """Writes package NAME's files (DATA) under DEST without its top folder; refuses links."""
    files = {}
    if name.endswith(".AppImage"):
        files[name] = data
    elif name.endswith(".zip"):
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            files = {info.filename: archive.read(info) for info in archive.infolist() if not info.is_dir()}
    elif name.endswith(".tar.gz"):
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
            for member in archive.getmembers():
                if member.issym() or member.islnk():
                    raise ValueError(f"{name} holds a link: {member.name}")
                if member.isfile():
                    files[member.name] = archive.extractfile(member).read()
    else:
        raise ValueError(f"unknown package {name}")
    top = _strip_top(list(files))
    for path, content in files.items():
        target = dest / path[len(top):]
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)


def player_hashes(root):
    return {path: file_sha(root / path) for path in (ROM, *PLAYER_FILES)}


def old_build(case):
    """The installed build number, from the package's build-info.json (None in an AppImage)."""
    info = case / "game/build-info.json"
    return json.loads(info.read_text(encoding="utf-8"))["build_number"] if info.is_file() else None


def lay_out(case, flavor, mode, package, data, rom, build):
    """Installs package (its name, DATA) in CASE as FLAVOR released as BUILD; returns before.json's record."""
    if mode not in MODES:
        raise ValueError(f"unknown mode {mode!r}")
    game, root = case / "game", data_root(case, flavor)
    unpack(package, data, game)
    (root / "Data").mkdir(parents=True, exist_ok=True)
    (root / ROM).write_bytes(rom)
    for path, content in PLAYER_FILES.items():
        (root / path).parent.mkdir(parents=True, exist_ok=True)
        (root / path).write_bytes(content)
    (root / "midas.ini").write_text(ini_text(mode), encoding="utf-8", newline="\n")
    before = {"flavor": flavor, "mode": mode, "package": package, "build": build,
              "player": player_hashes(root), "package_sha": sha256(data)}
    if kind(flavor) != "appimage":
        edited = game / EDITED
        edited.write_bytes(edited.read_bytes() + EDIT_MARK)
        before["edited"] = file_sha(edited)
    (case / "before.json").write_text(json.dumps(before, indent=1) + "\n", encoding="utf-8")
    return before


def finished(log, mode):
    """Whether the run in LOG (Data/Updates/log.txt) has nothing more to do."""
    if "reached the legal screen" in log or "cannot restart" in log or "cannot update itself" in log:
        return True
    results = RESULT.findall(log)
    return bool(results) if mode == "notify" else any(result != "installed" for result in results)


def installed_tag(log):
    match = re.search(r"installed \(installed: (build-[0-9.]+-[0-9a-f]{12})\)", log)
    return match.group(1) if match else None


def new_build(log):
    match = re.search(r"start 1 of build (\d+) after its install", log)
    return int(match.group(1)) if match else None


def listed_mismatches(game):
    """The files SHA256SUMS.txt lists whose bytes differ from it (or that are missing)."""
    sums = game / package_identity.SUMS
    if not sums.is_file():
        return ["SHA256SUMS.txt"]
    wrong = []
    for line in sums.read_text(encoding="utf-8").splitlines():
        digest, _, path = line.partition("  ")
        if file_sha(game / path) != digest:
            wrong.append(path)
    return wrong


def _common(case, before, log):
    root = data_root(case, before["flavor"])
    ini = (root / "midas.ini").read_text(encoding="utf-8") if (root / "midas.ini").is_file() else ""
    return root, {
        "player data is unchanged (ROM, save, packs)": player_hashes(root) == before["player"],
        "midas.ini keeps its settings": f"mode={before['mode']}" in ini and "fullscreen=windowed" in ini,
        "no swap journal is left": not (root / "Data/Updates/journal.txt").exists(),
    }


def verdict(case, before, log, new_package_sha=None):
    """Named checks for a finished case. NEW_PACKAGE_SHA: the installed release's AppImage hash."""
    root, checks = _common(case, before, log)
    game, appimage = case / "game", kind(before["flavor"]) == "appimage"
    current = file_sha(game / before["package"]) if appimage else None
    if before["mode"] == "notify":
        found = re.search(r"available \(a newer build is available: (\d+)\)", log)
        checks["the launch's check found a newer build"] = found is not None and (
            before["build"] is None or int(found.group(1)) > before["build"])
        checks["nothing was installed"] = (current == before["package_sha"] if appimage
                                           else old_build(case) == before["build"])
        checks["the game did not restart"] = "restart" not in log
        if not appimage:
            checks["the edited file is untouched"] = file_sha(game / EDITED) == before["edited"]
        return checks
    build = new_build(log)
    checks["the launch installed a newer release"] = installed_tag(log) is not None
    checks["the new build confirmed at the legal screen"] = (
        build is not None and f"build {build} reached the legal screen" in log)
    checks["no .old file is left"] = not any(game.rglob("*.old"))
    checks["the staging folder is gone"] = not any((root / "Data/Updates").glob("build-*"))
    checks["the new build is newer"] = build is not None and build > before["build"]
    if appimage:
        checks["the AppImage is the new release's"] = current is not None and current == new_package_sha
    else:
        checks["the program files are the new package's"] = (old_build(case) == build
                                                             and not listed_mismatches(game))
        checks["the edited file was kept as .edited"] = file_sha(game / (EDITED + ".edited")) == before["edited"]
    return checks
