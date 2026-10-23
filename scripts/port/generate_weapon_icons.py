#!/usr/bin/env python3
"""Render real ROM weapon models into transparent, supersampled HUD PNGs.

Requires a GE_VIDEO build, the user's baserom.u.z64, Pillow and NumPy.
    python scripts/port/generate_weapon_icons.py --exe build_win/ProjectMidas.exe
    python scripts/port/generate_weapon_icons.py --captures build_win/weapon-icons-...

The executable's offline export harness fixes camera/pose/material state and
captures each model twice on black and twice on white. Unequal repeat pixels,
missing models and clipped models are errors. GPU output is reproducible on a
fixed driver/backend; cross-GPU byte identity is not assumed. PNG processing
and packing are deterministic for fixed captures and dependency versions.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image, ImageChops, ImageDraw, __version__ as PIL_VERSION

ROOT = Path(__file__).resolve().parents[2]
NAMES = [None, "unarmed", "knife", "throwing_knife", "pp7", "pp7_silenced", "dd44",
         "klobb", "kf7", "zmg", "d5k", "d5k_silenced", "phantom", "ar33", "rcp90",
         "shotgun", "automatic_shotgun", "sniper_rifle", "cougar_magnum", "golden_gun",
         "silver_pp7", "gold_pp7", "laser", None, "grenade_launcher", "rocket_launcher",
         "grenade", "timed_mine", "proximity_mine", "remote_mine", None, "taser"]
WIDTH, HEIGHT = 512, 160


def sha(data):
    return hashlib.sha256(data).hexdigest()


def capture(exe, rom, resolution, item=None):
    (ROOT/"build_win").mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="weapon-icons-", dir=ROOT / "build_win"))
    target = work / exe.name
    shutil.copy2(exe, target)
    for dll in exe.parent.glob("*.dll"):
        shutil.copy2(dll, work / dll.name)
    (work / "midas.ini").write_text(
        "[display]\nfullscreen=window\nvsync=0\nmaxfps=60\ntickhz=60\nmsaa=1\n"
        "widescreen=0\nlow_res=0\nfiltering=bilinear\nfilter_2d=1\nrender_thread=0\n"
        "hit_markers=0\ndamage_indicators=0\nobjective_notifications=0\nweapon_hud=0\n"
        "mission_timer=0\n[input]\nmouse=1\n", encoding="utf-8")
    (work / "input.txt").write_text(
        "P 0 0 0 0\nP 80 8000 0 0\nP 82 0 0 0\nP 120 8000 0 0\n"
        "P 122 0 0 0\nP 160 8000 0 0\nP 162 0 0 0\nP 2000 0 0 0\n")
    env = {k: v for k, v in os.environ.items() if not k.startswith("GE_")}
    env.update(GE_ROM=str(rom), GE_STAGE="dam", GE_STAGE_AFTER="1", GE_WINDOW=resolution,
               GE_NOINPUT="1", GE_SEED="1", GE_CHEATS="invuln,invis,guns", GE_FRAMES="1500",
               GE_INPUTPLAY=str(work / "input.txt"), GE_WEAPON_ICON_EXPORT="1", GE_DUMP_BOND="1")
    if item is not None:
        env["GE_WEAPON_ICON_ITEM"] = str(item)
        env["GE_FRAMES"] = "400"
    print(f"Capturing actual weapon models: {work}", flush=True)
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    with (work / "capture.log").open("wb") as log:
        subprocess.run([str(target)], cwd=work, env=env, stdout=log, stderr=log,
                       creationflags=flags, check=True, timeout=240)
    return work


def recover(captures, item):
    images = [Image.open(captures / f"weapon_{item:02d}_{n}.bmp").convert("RGB") for n in range(1, 5)]
    for a, b in ((0, 2), (1, 3)):
        if images[a].size != images[b].size or ImageChops.difference(images[a], images[b]).getbbox():
            raise ValueError(f"{NAMES[item]}: repeated captures differ; pose/material state is not fixed")
    black, white = (np.asarray(image, dtype=np.int16) for image in images[:2])
    if black.shape != white.shape:
        raise ValueError("Matte image dimensions differ")
    alpha = np.clip(255 - np.median(white-black, axis=2), 0, 255).astype(np.uint8)
    alpha[alpha < 3] = 0
    rgb = np.clip(np.rint(black.astype(np.float32)*255 / np.maximum(alpha[..., None], 1)), 0, 255).astype(np.uint8)
    rgb[alpha == 0] = 0
    rgba = Image.fromarray(np.dstack((rgb, alpha)))
    bbox = Image.fromarray((alpha > 8).astype(np.uint8)*255).getbbox()
    if not bbox or (bbox[2]-bbox[0])*(bbox[3]-bbox[1]) < 100:
        raise ValueError(f"{NAMES[item]}: missing or empty model")
    w, h = rgba.size
    if bbox[0] < 2 or bbox[1] < 2 or bbox[2] > w-2 or bbox[3] > h-2:
        raise ValueError(f"{NAMES[item]}: model touches capture border; increase camera distance")
    box = (max(0, bbox[0]-4), max(0, bbox[1]-4), min(w, bbox[2]+4), min(h, bbox[3]+4))
    model = rgba.crop(box)
    model.thumbnail((WIDTH-24, HEIGHT-16), Image.Resampling.LANCZOS)
    icon = Image.new("RGBA", (WIDTH, HEIGHT))
    icon.paste(model, ((WIDTH-model.width)//2, (HEIGHT-model.height)//2))
    return icon, bbox, [sha(image.tobytes()) for image in images[:2]]


def pack_pixels(raw):
    """Bounded RGBA packet format: u16 LE count, high bit = repeated pixel."""
    pixels = [raw[i:i+4] for i in range(0, len(raw), 4)]
    out = bytearray()
    i = 0
    while i < len(pixels):
        run = 1
        while i+run < len(pixels) and pixels[i+run] == pixels[i] and run < 32767:
            run += 1
        if run >= 3:
            out += struct.pack("<H", run | 0x8000) + pixels[i]
            i += run
        else:
            start = i
            i += run
            while i < len(pixels) and i-start < 32767:
                if i+2 < len(pixels) and pixels[i] == pixels[i+1] == pixels[i+2]:
                    break
                i += 1
            out += struct.pack("<H", i-start) + b"".join(pixels[start:i])
    return bytes(out)


def process_staged(captures, output, rom, exe):
    output.mkdir(parents=True, exist_ok=True)
    packed = bytearray()
    rows = [(0, 0)] * len(NAMES)
    entries = []
    sheet = Image.new("RGB", (WIDTH*4, (HEIGHT+34)*8), "#10171c")
    draw = ImageDraw.Draw(sheet)
    for item, name in enumerate(NAMES):
        if name is None:
            continue
        icon, bbox, source_hashes = recover(captures, item)
        icon.save(output / f"{name}.png", compress_level=9)
        encoded = pack_pixels(icon.tobytes())
        rows[item] = (len(packed), len(encoded))
        packed += encoded
        index = len(entries)
        x, y = (index % 4)*WIDTH, (index // 4)*(HEIGHT+34)
        sheet.paste(icon, (x, y), icon)
        draw.text((x+12, y+HEIGHT+5), f"{item:02d}  {name}", fill="#e3ebde")
        entries.append(dict(item=item, name=name, crop=bbox, source_sha256=source_hashes,
                            rgba_sha256=sha(icon.tobytes()), png_sha256=sha((output/f"{name}.png").read_bytes())))
    rows[0] = rows[1]  # no independent unarmed model; use Bond's actual fist
    sheet.crop((0, 0, WIDTH*4, ((len(entries)+3)//4)*(HEIGHT+34))).save(output/"preview.png")
    (output/"weapon_icons.rle").write_bytes(packed)
    header = ["/* Generated by scripts/port/generate_weapon_icons.py; do not edit. */",
              "#ifndef PORT_WEAPON_IMAGES_H", "#define PORT_WEAPON_IMAGES_H",
              f"#define PORT_WEAPON_IMAGE_WIDTH {WIDTH}", f"#define PORT_WEAPON_IMAGE_HEIGHT {HEIGHT}",
              f"#define PORT_WEAPON_IMAGE_BYTES {len(packed)}",
              "typedef struct { unsigned int offset, size; } PortWeaponImage;",
              "static const PortWeaponImage portWeaponImages[32] = {"]
    header += [f"    {{{off}, {size}}}, /* {item}: {NAMES[item] or 'alias/unused'} */" for item, (off, size) in enumerate(rows)]
    header += ["};", "#endif", ""]
    (output/"weapon_icons.h").write_text("\n".join(header), encoding="utf-8", newline="\n")
    manifest = dict(format=1, dimensions=[WIDTH, HEIGHT], capture_size=list(Image.open(captures/"weapon_04_1.bmp").size),
                    camera="watch model origin; 35 degree FOV; depth x1.6; side/top offsets 0.18/0.16",
                    materials="original textures; neutral fog; fixed export-camera metallic reflection basis",
                    transparency="black/white matte recovery; repeated captures must match pixel-for-pixel",
                    pillow=PIL_VERSION, numpy=np.__version__, rom_sha256=sha(rom.read_bytes()),
                    exporter_sha256=sha(exe.read_bytes()), packed_sha256=sha(packed), icons=entries)
    (output/"manifest.json").write_text(json.dumps(manifest, indent=2)+"\n", encoding="utf-8", newline="\n")
    print(f"Generated {len(entries)} real-model icons; packed RGBA: {len(packed):,} bytes.")


def process(captures, output, rom, exe):
    output.parent.mkdir(parents=True, exist_ok=True)
    # Validate every model before replacing any existing production asset.
    # Retain mtimes for equal outputs so verification does not force a rebuild.
    with tempfile.TemporaryDirectory(prefix=".weapon-icons-", dir=output.parent) as folder:
        stage = Path(folder)
        process_staged(captures, stage, rom, exe)
        output.mkdir(parents=True, exist_ok=True)
        for path in stage.iterdir():
            destination = output/path.name
            if not destination.exists() or destination.read_bytes() != path.read_bytes():
                os.replace(path, destination)
    print(f"Installed verified icons: {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", type=Path, default=ROOT/"build_win/ProjectMidas.exe")
    parser.add_argument("--rom", type=Path, default=ROOT/"baserom.u.z64")
    parser.add_argument("--captures", type=Path, help="Reprocess existing black/white captures without launching the game")
    parser.add_argument("--output", type=Path, default=ROOT/"assets/port/weapon_icons")
    parser.add_argument("--resolution", default="2048x1536", help="Use a 4:3 capture size, up to 4096 pixels wide")
    parser.add_argument("--item", type=int, help="Capture one model for camera inspection; does not regenerate the asset pack")
    args = parser.parse_args()
    try:
        width, height = map(int, args.resolution.lower().split("x"))
        if width*3 != height*4 or width < 1024 or width > 4096:
            raise ValueError()
    except ValueError:
        parser.error("--resolution must be 4:3, between 1024 and 4096 pixels wide")
    if args.item is not None and (args.item < 1 or args.item >= len(NAMES) or NAMES[args.item] is None):
        parser.error("--item must name a model-backed inventory weapon (1..31, except 23 and 30)")
    exe, rom = args.exe.resolve(), args.rom.resolve()
    if not exe.is_file() or not rom.is_file():
        parser.error("--exe and --rom must point to existing files")
    work = args.captures.resolve() if args.captures else capture(exe, rom, args.resolution, args.item)
    if args.item is None:
        process(work, args.output.resolve(), rom, exe)
    else:
        icon, _, _ = recover(work, args.item)
        icon.save(work/"preview.png")
        print(work/"preview.png")


if __name__ == "__main__":
    main()
