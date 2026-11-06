"""Install or check the Enhanced fonts' pinned files (font styles plan, FONT-2).

    python scripts/port/enhanced_fonts_pack.py install --dest DIR
    python scripts/port/enhanced_fonts_pack.py check --dest DIR

The files are Carnivorous's CC0 GEPD font images, downloaded from
Graslu/GEPDTexturePacks at the pinned commit and checked against
port/fast3d/enhanced_fonts_list.inc. They are never committed or bundled; DIR is
a game's Data/FontPacks/gepd folder (or a scratch folder for tests). This is the
manual install the game's own download (FONT-3) will automate.
"""
import argparse
import hashlib
import re
import sys
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIST = ROOT / 'port' / 'fast3d' / 'enhanced_fonts_list.inc'
REPOSITORY = 'Graslu/GEPDTexturePacks'
COMMIT = 'ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63'
PACK_DIR = 'GoldenEye Texture Pack/'
ROW = re.compile(r'^\{(\d), \'(\\?.)\', "([^"]+)", (\d+), "([0-9a-f]{64})"\},$')
SOURCE_NOTE = f"""Enhanced fonts for Iron Sights

Carnivorous's GEPD font images (CC0-1.0), from
https://github.com/{REPOSITORY} at commit {COMMIT}.
Installed by the player; Iron Sights does not bundle them.
"""


def pinned_files():
    """(font, character, pack path, size, sha256) rows of the pinned list."""
    rows = []
    for line in LIST.read_text(encoding='utf-8').splitlines():
        match = ROW.match(line)
        if match:
            font, char, path, size, digest = match.groups()
            rows.append((int(font), char[-1], path, int(size), digest))
    if len(rows) != 186:
        raise SystemExit(f'expected 186 pinned files in {LIST}, found {len(rows)}')
    return rows


def matches(data, size, digest):
    return len(data) == size and hashlib.sha256(data).hexdigest() == digest


def download(path):
    url = (f'https://raw.githubusercontent.com/{REPOSITORY}/{COMMIT}/'
           + urllib.parse.quote(PACK_DIR + path))
    with urllib.request.urlopen(url, timeout=30) as response:
        return response.read()


def install(dest):
    dest.mkdir(parents=True, exist_ok=True)
    fetched = 0
    for _, _, path, size, digest in pinned_files():
        target = dest / Path(path).name
        if target.is_file() and matches(target.read_bytes(), size, digest):
            continue
        data = download(path)
        if not matches(data, size, digest):
            raise SystemExit(f'{path}: size or SHA-256 does not match the pinned list')
        partial = target.with_name(target.name + '.part')
        partial.write_bytes(data)
        partial.replace(target)
        fetched += 1
    (dest / 'SOURCE.txt').write_text(SOURCE_NOTE, encoding='utf-8')
    print(f'installed {fetched} new files; all 186 match the pinned list in {dest}')


def check(dest):
    bad = [path for _, _, path, size, digest in pinned_files()
           if not (dest / Path(path).name).is_file()
           or not matches((dest / Path(path).name).read_bytes(), size, digest)]
    for path in bad:
        print(f'missing or different: {Path(path).name}')
    print(f'{186 - len(bad)} of 186 files match the pinned list in {dest}')
    return 1 if bad else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('command', choices=('install', 'check'))
    parser.add_argument('--dest', type=Path, required=True, help='the Data/FontPacks/gepd folder')
    args = parser.parse_args()
    if args.command == 'install':
        install(args.dest)
        return 0
    return check(args.dest)


if __name__ == '__main__':
    sys.exit(main())
