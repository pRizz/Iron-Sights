"""Prepare intermissionfb's HD ammo icons for the HUD; Pillow is needed only to regenerate.

The pinned GoldenEye 007 HD release names its ammo art by Rice texture CRC, but the
game's 32-bit ammo icons do not reproduce those CRCs, so each image is assigned to
the HUD's ammo slot (gunfire.c portAmmoIconPtr order) instead. Images are resampled
to eight times the native icon size and stored as RGBA run-length packets
(port_icon_decode.h). Normal builds use the checked-in output.

    build_win/hd-tools/Scripts/python.exe scripts/port/generate_hd_ammo_icons.py \
        --archive build_win/hd-research/ge007-hd.zip
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import urllib.request
import zipfile

from prepare_texture_pack import SHA256, URL

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT/"port/fast3d/hud_icons"
SCALE = 8
# HUD ammo slot -> (native image, source PNG in the pinned release). The plain
# mine icon has no community art and keeps its original image.
SLOTS = [
    ("s_ammo9mmimage", "UI/Ammo/GOLDENEYE#AF0A1953#0#3_all.png"),
    ("s_rifleammoimage", "UI/Ammo/GOLDENEYE#AC193F4F#0#3_all.png"),
    ("s_shotgunammoimage", "UI/Ammo/GOLDENEYE#2B351868#0#3_all.png"),
    ("s_knifeammoimage", "UI/Ammo/GOLDENEYE#98685E98#0#3_all.png"),
    ("s_glammoimage", "UI/Ammo/GOLDENEYE#EC11BEAB#0#3_all.png"),
    ("s_rocketammoimage", "UI/Ammo/GOLDENEYE#BA97A223#0#3_all.png"),
    ("s_genericmineammoimage", None),
    ("s_grenadeammoimage", "UI/Ammo/GOLDENEYE#AF8E57EF#0#3_all.png"),
    ("s_magnumammoimage", "UI/Ammo/GOLDENEYE#91651507#0#3_all.png"),
    ("s_goldengunammoimage", "UI/Ammo/GOLDENEYE#FA82F840#0#3_all.png"),
    ("s_remotemineammoimage", "UI/Ammo/GOLDENEYE#551F375D#0#3_all.png"),
    ("s_timedmineammoimage", "UI/Ammo/GOLDENEYE#6F1AC5BE#0#3_all.png"),
    ("s_proxmineammoimage", "UI/Ammo/GOLDENEYE#8640BBB3#0#3_all.png"),
    ("s_tankammoimage", "UI/Ammo/Tank/GOLDENEYE#0590F622#2#1#$_ciByRGBA.png"),
]
# The release repeats the tank shell under six palette keys; all must match.
TANK_ALIASES = ["0590F622", "3BDD4B1C", "5F54D152", "7EE28372", "90F09B58", "D580EAC6"]
NOTICE = """GoldenEye 007 HD - HUD ammo icons
Textures by intermissionfb. Font textures and technical assistance: GhostlyDark.
https://evilgames.eu/texture-packs/ge007-hd.htm
https://github.com/GhostlyDark/GoldenEye-007-HD
Source HD release: v2025.12.30 (UI/Ammo). Artist artwork remains credited to its
authors; the port's MIT license does not relicense this artwork. The source
release has no explicit redistribution license. The Iron Sights owner chose to
embed these icons in every build of the game (2026-11-05); this
notice records attribution, not a new grant of rights. No ROM or original game
texture data is included.

Each image is resampled to eight times the native icon size and stored as
lossless RGBA run-length packets.
"""


def native_sizes():
    """Native icon width/height from the inherited image table."""
    text = (ROOT/"assets/oddtextures.c").read_text(encoding="utf-8")
    sizes = {}
    for name, _ in SLOTS:
        entry = re.search(re.escape(name)+r"\[\]\s*=\s*\{\s*\{\s*\w+,\s*(0x[0-9A-Fa-f]+|\d+),\s*(0x[0-9A-Fa-f]+|\d+)", text)
        sizes[name] = (int(entry[1], 0), int(entry[2], 0))
    return sizes


def rle(pixels):
    """port_icon_decode.h packets: u16 count, high bit = one repeated pixel."""
    words = [pixels[i:i+4] for i in range(0, len(pixels), 4)]
    out, i = bytearray(), 0
    while i < len(words):
        run = 1
        while i+run < len(words) and run < 0x7fff and words[i+run] == words[i]:
            run += 1
        if run >= 3:
            out += (0x8000 | run).to_bytes(2, "little") + words[i]
            i += run
            continue
        start = i
        while i < len(words) and i-start < 0x7fff and not (i+2 < len(words) and words[i] == words[i+1] == words[i+2]):
            i += 1
        out += (i-start).to_bytes(2, "little") + b"".join(words[start:i])
    return bytes(out)


def resample(png, width, height):
    from PIL import Image
    image = Image.open(io.BytesIO(png)).convert("RGBA")
    # Premultiplied resampling keeps transparent edges from darkening.
    return image.convert("RGBa").resize((width, height), Image.LANCZOS).convert("RGBA").tobytes()


def generate(archive_path):
    source = archive_path.read_bytes()
    if hashlib.sha256(source).hexdigest() != SHA256:
        raise ValueError("Archive does not match the pinned GoldenEye 007 HD release")
    archive = zipfile.ZipFile(io.BytesIO(source))
    def read(path):
        return archive.read("GOLDENEYE/"+path)
    tank = {read(f"UI/Ammo/Tank/GOLDENEYE#{key}#2#1#$_ciByRGBA.png") for key in TANK_ALIASES}
    if len(tank) != 1:
        raise ValueError("Tank shell aliases differ; choose one deliberately")
    sizes, pixels, rows, provenance = native_sizes(), bytearray(), [], []
    for slot, (name, path) in enumerate(SLOTS):
        native = sizes[name]
        if path is None:
            rows.append((native, (0, 0), 0, 0))
            provenance.append(dict(slot=slot, image=name, native=native, source=None))
            continue
        png = read(path)
        width, height = native[0]*SCALE, native[1]*SCALE
        packed = rle(resample(png, width, height))
        rows.append((native, (width, height), len(pixels), len(packed)))
        provenance.append(dict(slot=slot, image=name, native=native, size=[width, height], source=path,
                               png_sha256=hashlib.sha256(png).hexdigest(), packed_bytes=len(packed)))
        pixels += packed
    OUTPUT.mkdir(parents=True, exist_ok=True)
    lines = ["/* Generated by scripts/port/generate_hd_ammo_icons.py; do not edit. */",
             f"/* Source: {URL} (SHA256 {SHA256}). See NOTICE.txt. */",
             f"#define HD_AMMO_ICON_SCALE {SCALE}",
             "static const HdAmmoIcon hdAmmoIcons[] = {"]
    for slot, ((nw, nh), (w, h), offset, size) in enumerate(rows):
        lines.append(f"    {{ {nw}, {nh}, {w}, {h}, {offset}, {size} }}, /* {slot}: {SLOTS[slot][0]} */")
    lines.append("};")
    lines.append(f"static const unsigned char hdAmmoPixels[{len(pixels)}] = {{")
    for i in range(0, len(pixels), 24):
        lines.append("    " + ",".join(str(b) for b in pixels[i:i+24]) + ",")
    lines.append("};")
    (OUTPUT/"hd_ammo_icons.inc").write_text("\n".join(lines)+"\n", encoding="utf-8", newline="\n")
    (OUTPUT/"NOTICE.txt").write_text(NOTICE, encoding="utf-8", newline="\n")
    (OUTPUT/"sources.json").write_text(json.dumps(dict(source=URL, source_sha256=SHA256, scale=SCALE,
        tank_aliases=TANK_ALIASES, slots=provenance), indent=2)+"\n", encoding="utf-8", newline="\n")
    return provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--download", action="store_true", help="Download the pinned HD release if missing")
    args = parser.parse_args()
    if args.download and not args.archive.is_file():
        args.archive.parent.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(URL, args.archive)
    for row in generate(args.archive):
        print(row["slot"], row["image"], row.get("size"), row.get("packed_bytes"))


if __name__ == "__main__":
    main()
