#!/usr/bin/env python3
"""Give the Iron Sights Steam shortcut its name, icon and library artwork.

Run it in Desktop Mode from the extracted folder:  python3 set-steam-art.py

1. Installs an "Iron Sights" application-menu entry for this folder's run.sh with
   the bundled icon, so Steam's "Add a Non-Steam Game" list offers it by name.
2. Finds Steam shortcuts that launch this folder and copies the cover, banner,
   hero and logo from steam-art/ into each Steam user's grid folder.

Steam's shortcuts.vdf is only read, never written. Artwork that is already set
for a shortcut is kept unless --force is given. Restart Steam, or return to
Gaming Mode, to see new artwork.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import zlib

HERE = Path(__file__).resolve().parent
LAUNCHERS = ("run.sh", "IronSights")
DESKTOP_NAME = "iron-sights-steamdeck.desktop"
# Bundled image -> Steam grid file stem for a shortcut's 32-bit app ID.
ARTWORK = (("cover.png", "{}p"), ("banner.png", "{}"), ("hero.png", "{}_hero"), ("logo.png", "{}_logo"))
GRID_EXTENSIONS = (".png", ".jpg", ".jpeg")
EXEC_RESERVED = re.compile(r"[\s\"'\\><~|&;$*?#()`%]")


def parse_vdf(data):
    """Decode Steam's binary KeyValues (shortcuts.vdf) into nested dicts."""
    def string(pos):
        end = data.index(b"\0", pos)
        return data[pos:end].decode("utf-8", "replace"), end + 1

    def mapping(pos):
        result = {}
        while pos < len(data):
            kind = data[pos]
            pos += 1
            if kind == 0x08:
                return result, pos
            name, pos = string(pos)
            if kind == 0x00:
                result[name], pos = mapping(pos)
            elif kind == 0x01:
                result[name], pos = string(pos)
            elif kind in (0x02, 0x03):
                result[name] = struct.unpack_from("<i" if kind == 0x02 else "<f", data, pos)[0]
                pos += 4
            elif kind == 0x07:
                result[name] = struct.unpack_from("<Q", data, pos)[0]
                pos += 8
            else:
                raise ValueError(f"Unsupported binary VDF field type 0x{kind:02x} at byte {pos - 1}")
        return result, pos

    return mapping(0)[0]


def field(entry, name, default=None):
    """Steam has written shortcut keys in different cases (appname, AppName)."""
    for key, value in entry.items():
        if key.lower() == name.lower():
            return value
    return default


def unquote(value):
    return value.strip().strip('"')


def same_path(value, target):
    return bool(value) and os.path.normcase(os.path.realpath(value)) == os.path.normcase(os.path.realpath(target))


def launches_folder(entry, folder):
    """True when a shortcut runs this folder's launcher or starts in this folder."""
    exe = unquote(str(field(entry, "exe", "")))
    if any(same_path(exe, folder / name) for name in LAUNCHERS):
        return True
    return same_path(unquote(str(field(entry, "startdir", ""))), folder)


def app_id(entry):
    """The unsigned 32-bit ID that names a shortcut's grid images."""
    stored = field(entry, "appid")
    if isinstance(stored, int):
        return stored & 0xFFFFFFFF
    # Shortcuts saved before Steam stored appid use the legacy CRC of exe + name.
    legacy = str(field(entry, "exe", "")) + str(field(entry, "appname", ""))
    return zlib.crc32(legacy.encode("utf-8")) | 0x80000000


def matching_shortcuts(vdf, folder):
    shortcuts = field(vdf, "shortcuts", {})
    return [entry for entry in shortcuts.values() if isinstance(entry, dict) and launches_folder(entry, folder)]


def grid_plan(shortcut_id, existing, force):
    """Return (image, grid file, reason) rows; reason is None for copies to make."""
    plan = []
    for image, stem in ARTWORK:
        name = stem.format(shortcut_id)
        target = name + ".png"
        taken = [n for n in existing if n.lower() in {name.lower() + ext for ext in GRID_EXTENSIONS}]
        plan.append((image, target, None if force or not taken else "kept existing " + ", ".join(sorted(taken))))
    return plan


def desktop_exec(path):
    """Quote an Exec= program path per the Desktop Entry specification."""
    value = str(path)
    if EXEC_RESERVED.search(value):
        value = '"' + re.sub(r'(["`$\\])', r"\\\1", value) + '"'
    return value.replace("\\", "\\\\").replace("%", "%%")


def desktop_string(path):
    return str(path).replace("\\", "\\\\")


def desktop_entry(folder):
    return "\n".join((
        "[Desktop Entry]",
        "Type=Application",
        "Name=Iron Sights",
        "Comment=Iron Sights (Steam Deck ZIP)",
        "Exec=" + desktop_exec(folder / "run.sh"),
        "Path=" + desktop_string(folder),
        "Icon=" + desktop_string(folder / "iron-sights.png"),
        "Categories=Game;",
        "Terminal=false",
        ""))


def steam_roots(home):
    candidates = (home / ".local/share/Steam", home / ".steam/steam", home / ".steam/root",
                  home / ".var/app/com.valvesoftware.Steam/.local/share/Steam")
    roots = {}
    for root in candidates:
        if (root / "userdata").is_dir():
            roots.setdefault(os.path.realpath(root), root)
    return list(roots.values())


def install_menu_entry(folder, data_home):
    for name in LAUNCHERS:
        launcher = folder / name
        if launcher.exists():
            launcher.chmod(launcher.stat().st_mode | 0o111)
    target = data_home / "applications" / DESKTOP_NAME
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(desktop_entry(folder), encoding="utf-8")
    print(f"Menu entry: {target}")


def copy_artwork(config, folder, force):
    """Copy artwork for every matching shortcut of one Steam user; return the count."""
    shortcuts = matching_shortcuts(parse_vdf((config / "shortcuts.vdf").read_bytes()), folder)
    grid = config / "grid"
    for entry in shortcuts:
        shortcut_id = app_id(entry)
        print(f"Shortcut \"{field(entry, 'appname', '?')}\" (app {shortcut_id}) in {config.parent}")
        existing = os.listdir(grid) if grid.is_dir() else []
        for image, target, reason in grid_plan(shortcut_id, existing, force):
            if reason:
                print(f"  {target}: {reason} (use --force to replace)")
                continue
            grid.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(folder / "steam-art" / image, grid / target)
            print(f"  {target}: copied {image}")
    return len(shortcuts)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--force", action="store_true", help="replace artwork already set for the shortcut")
    parser.add_argument("--steam-root", type=Path, action="append", help="Steam data folder (repeatable)")
    args = parser.parse_args(argv)
    home = Path(os.environ.get("HOME") or Path.home())
    data_home = Path(os.environ.get("XDG_DATA_HOME") or home / ".local/share")

    install_menu_entry(HERE, data_home)
    roots = args.steam_root or steam_roots(home)
    configs = [c for root in roots for c in sorted((root / "userdata").glob("*/config")) if (c / "shortcuts.vdf").is_file()]
    found = sum(copy_artwork(config, HERE, args.force) for config in configs)
    if found:
        print("Done. Restart Steam, or return to Gaming Mode, to see the artwork.")
        return 0
    print("No Steam shortcut for this folder yet. In Steam choose Games > Add a Non-Steam Game to My Library,\n"
          "tick Iron Sights (or browse to run.sh) and run this script again. If the game is already in\n"
          "your library, exit Steam once so it saves its shortcut list, then run this script again.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
