"""Prepare a replaceable native texture pack, preserving every RGBA pixel.

Uses Pillow only for preparation. Runtime needs no PNG library or new DLL.
Default scope is the pinned campaign environment inventory, not weapons or UI.
"""
import argparse
import hashlib
import io
import itertools
import json
from pathlib import Path
import re
import struct
import urllib.request
import zipfile
import zlib
from texture_pack_supplement import artwork, MANIFEST as SUPPLEMENT_MANIFEST

ROOT = Path(__file__).resolve().parents[2]
URL = "https://evilgames.eu/files/texture-packs/ge007-hd-v2025.12.30-gliden64-png-hd.zip"
SHA256 = "a72d082e23b516f28f4868b0b32cbb676a5b0ce7f00e7eb79badf89f71ab90ba"
NAME = re.compile(r"GOLDENEYE#([0-9a-fA-F]{8})#([0-4])#([0-3])(?:#([0-9a-fA-F]{8}|\$))?_(all|ciByRGBA)\.png$")
NOTICE = """GoldenEye 007 HD - native campaign environments
Textures by intermissionfb. Font textures and technical assistance: GhostlyDark.
https://evilgames.eu/texture-packs/ge007-hd.htm
https://github.com/GhostlyDark/GoldenEye-007-HD
Source HD release: v2025.12.30. Artist artwork remains credited to its authors;
the port's MIT license does not relicense this artwork. The source release has
no explicit redistribution license. This notice records attribution, not a
new grant of rights. No ROM or original game texture data is included.

Preparation preserves decoded RGBA pixels, strips PNG container metadata and
deduplicates identical images. Data remains DEFLATE compressed on disk.
Runtime DEFLATE decoding: puff 2.3 by Mark Adler (zlib 1.3.1 distribution).
See puff-LICENSE.txt for its license.
"""


def key(name):
    match = NAME.fullmatch(name.replace("\\", "/").rsplit("/", 1)[-1])
    if not match:
        return None
    texture, fmt, size, palette, _ = match.groups()
    return int(texture, 16), int("0" if palette == "$" else palette or "0", 16), int(fmt), int(size)


def deflate_pixels(image):
    """Re-use PNG's proven adaptive lossless filters; keep only raw DEFLATE."""
    data = io.BytesIO()
    image.save(data, format="PNG", optimize=True, compress_level=9)
    png = data.getvalue()
    position, idat = 8, bytearray()
    while position < len(png):
        size = struct.unpack_from(">I", png, position)[0]
        if png[position+4:position+8] == b"IDAT":
            idat.extend(png[position+8:position+8+size])
        position += size+12
    # PNG IDAT has the two-byte zlib header and four-byte Adler trailer.
    return bytes(idat[2:-4])


def archive_artwork(archive, scope, wanted):
    for entry in sorted(archive.infolist(), key=lambda item: item.filename):
        identity = key(entry.filename)
        if identity is None or (scope != "all" and identity not in wanted and (identity[0],0,identity[2],identity[3]) not in wanted):
            continue
        if entry.file_size > 80*1024*1024:
            raise ValueError("Oversized texture: "+entry.filename)
        yield entry.filename, archive.read(entry)


def prepare(archive_path, output, scope="levels", inventory=None, supplement_dir=None):
    from PIL import Image, __version__ as pillow_version
    source = archive_path.read_bytes()
    pinned = hashlib.sha256(source).hexdigest() == SHA256
    if scope not in ("dam", "levels", "all"):
        raise ValueError("Unknown texture scope: "+scope)
    inventory = inventory or ROOT/f"scripts/port/hd_{'dam' if scope == 'dam' else 'level'}_sources.json"
    manifest = json.loads(inventory.read_text())
    wanted = {k for name in manifest["textures"] if (k := key(name)) is not None}
    records, blobs, images, sources = {}, [], {}, []
    png_bytes = rgba_bytes = constant_alpha = 0
    with zipfile.ZipFile(io.BytesIO(source)) as archive:
        entries = itertools.chain(archive_artwork(archive, scope, wanted),
                                  artwork(supplement_dir) if supplement_dir is not None else ())
        for name, png in entries:
            identity = key(name)
            if identity is None or (scope != "all" and identity not in wanted and (identity[0],0,identity[2],identity[3]) not in wanted):
                continue
            if len(png) > 80*1024*1024:
                raise ValueError("Oversized texture: "+name)
            with Image.open(io.BytesIO(png)) as original:
                if original.width > 4096 or original.height > 4096:
                    raise ValueError("Texture exceeds 4096: "+name)
                image = original.convert("RGBA")
            pixels = image.tobytes()
            image_id = (image.size, hashlib.sha256(pixels).hexdigest())
            if identity in records:
                if records[identity][0] != image_id:
                    raise ValueError("Conflicting artwork for key: "+name)
                continue
            png_bytes += len(png)
            rgba_bytes += len(pixels)
            constant_alpha += image.getchannel("A").getextrema()[0] == image.getchannel("A").getextrema()[1]
            if image_id not in images:
                images[image_id] = len(blobs)
                blobs.append(deflate_pixels(image))
            records[identity] = image_id, images[image_id], zlib.adler32(pixels)
            sources.append(dict(path=name, key=list(identity), width=image.width,
                                height=image.height, png_sha256=hashlib.sha256(png).hexdigest(),
                                rgba_sha256=image_id[1]))
    if not records:
        raise ValueError("No matching PNG textures in source archive")
    source_keys = len(records)
    imported_keys = set(records)
    # Native TLUT storage differs from GLideN64's palette fingerprint boundary.
    # Only reviewed stock-USA identities get aliases; never guess by index CRC.
    aliases = []
    for name in ("hd_dam_native_aliases.json", "hd_level_native_aliases.json"):
        aliases.extend(json.loads((ROOT/"scripts/port"/name).read_text())["aliases"])
    for alias in aliases:
        original_key = tuple(alias["pack_key"])
        if original_key not in records:
            continue
        native_key = (original_key[0], alias["native_palette"], *original_key[2:])
        if native_key in records and records[native_key] != records[original_key]:
            raise ValueError("Conflicting native palette alias")
        records[native_key] = records[original_key]
    offset = 16+len(records)*36
    offsets = []
    for blob in blobs:
        offsets.append(offset)
        offset += len(blob)
    table = bytearray(struct.pack("<8sII", b"ISTEX001", len(records), offset))
    for identity, (image_id, index, checksum) in sorted(records.items()):
        table.extend(struct.pack("<9I", *identity, *image_id[0], offsets[index], len(blobs[index]), checksum))
    table.extend(b"".join(blobs))
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".tmp")
    temporary.write_bytes(table)
    temporary.replace(output)
    coverage = {}
    if scope != "all":
        for name in manifest["textures"]:
            identity = key(name)
            if identity is None:
                continue
            level = name.split("/")[2]
            group = coverage.setdefault(level, dict(requested=0, imported=0))
            group["requested"] += 1
            group["imported"] += identity in imported_keys or (identity[0],0,identity[2],identity[3]) in imported_keys
    report = dict(schema=2, scope=scope, source=URL if pinned else str(archive_path.name),
                  source_sha256=hashlib.sha256(source).hexdigest(), pinned_release=pinned,
                  pillow=pillow_version, keys=len(records), source_keys=source_keys,
                  native_palette_aliases=len(records)-source_keys, unique_images=len(blobs),
                  duplicate_images=source_keys-len(blobs), source_png_bytes=png_bytes,
                  decoded_rgba_bytes=rgba_bytes, pack_bytes=len(table),
                  constant_alpha_images=constant_alpha, rgba_pixels_preserved=True, row_order="source-order",
                  supplement_revision=json.loads(SUPPLEMENT_MANIFEST.read_text())["revision"] if supplement_dir is not None else None,
                  inventory_revision=manifest["revision"], level_coverage=coverage,
                  missing_scope_keys=[list(k) for k in sorted(wanted-imported_keys)
                                      if (k[0],0,k[2],k[3]) not in imported_keys], textures=sources)
    output.with_suffix(".json").write_text(json.dumps(report, indent=2)+"\n")
    (output.parent/"HD-Textures-NOTICE.txt").write_text(NOTICE)
    (output.parent/"puff-LICENSE.txt").write_bytes((ROOT/"port/fast3d/third_party/puff/puff.h").read_bytes())
    print(json.dumps({k:v for k,v in report.items() if k not in ("textures", "missing_scope_keys")}, indent=2))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--download", action="store_true", help="Download and verify the pinned HD release")
    parser.add_argument("--scope", choices=("dam", "levels", "all"), default="levels")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--supplement-dir", type=Path, help="Verified pinned source artwork omitted from the release ZIP")
    args = parser.parse_args()
    if args.download:
        args.archive.parent.mkdir(parents=True, exist_ok=True)
        if not args.archive.is_file():
            urllib.request.urlretrieve(URL, args.archive)
        if hashlib.sha256(args.archive.read_bytes()).hexdigest() != SHA256:
            raise ValueError("Downloaded archive does not match the pinned SHA256")
        if args.scope == "levels":
            args.supplement_dir = args.supplement_dir or args.archive.parent/"hd-level-supplement"
            list(artwork(args.supplement_dir, download=True))
    prepare(args.archive, args.output, args.scope, supplement_dir=args.supplement_dir)


if __name__ == "__main__":
    main()
