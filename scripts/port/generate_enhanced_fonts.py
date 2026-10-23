"""Prepare Carnivorous's CC0 GEPD fonts; Pillow is needed only to regenerate.

The output contains community-authored masks and fingerprints, never ROM pixels.
Normal builds use the checked-in output and need neither Pillow nor downloads.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[2]
COMMIT = "ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63"
PACK_SHA256 = "f1ae56eb62dac4ffb98842e029d0f44d522b4f8056a7d3268ac7db4c7e835621"
SOURCE = "https://github.com/Graslu/GEPDTexturePacks"


def fingerprint(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return value


def rice_fingerprint(data, width, height):
    """Rice pack-name fingerprint of an I8 tile, in N64 byte order."""
    value = 0
    for row in range(height):
        for column in range(width - 4, -1, -4):
            word = int.from_bytes(data[row*width+column:row*width+column+4], "big") ^ column
            value = (((value << 4) | (value >> 28)) + word) & 0xffffffff
        value = (value + (word ^ (height - 1 - row))) & 0xffffffff
    return value


def native_glyphs(font):
    text = (ROOT/"assets/font"/("font"+font+".c")).read_text(encoding="utf-8")
    def words(kind):
        body = re.search(r"font"+font+"_"+kind+r"\[\]\s*=\s*\{(.*?)\}", text, re.S)[1]
        return [int(word, 16) for word in re.findall(r"0x[0-9A-Fa-f]+", body)]
    table, pixels = words("fontchartable"), words("fontbytes")
    image = bytes(169*4) + struct.pack(">"+str(len(table))+"I", *table) + struct.pack(">"+str(len(pixels))+"I", *pixels)
    for index in range(94):
        _, baseline, height, width, _, offset = table[index*6:index*6+6]
        stride = (width+7) & ~7
        raw = image[offset:offset+stride*height]
        cleaned = bytearray(raw)
        for row in range(height):
            cleaned[row*stride+width:(row+1)*stride] = bytes(stride-width)
        yield dict(font=font, character=chr(index+33), width=width, height=height, stride=stride,
                   baseline=baseline, rice=rice_fingerprint(raw, stride, height),
                   key=fingerprint(cleaned))


def encode(data):
    result = bytearray()
    for byte in data:
        if result and result[-1] == byte and result[-2] < 255:
            result[-2] += 1
        else:
            result.extend((1, byte))
    return result


def generate(pack, output, manifest):
    from PIL import Image
    if hashlib.sha256(pack.read_bytes()).hexdigest() != PACK_SHA256:
        raise ValueError("Expected the pinned GEPD CC0 archive")
    records, payload, sources = [], bytearray(), []
    with zipfile.ZipFile(pack) as archive:
        names = {int(name.split("#")[1],16): name for name in archive.namelist()
                 if "/GoldenEye Texture Pack/" in name and name.endswith("#4#1_rgb.png")}
        for font in ("BankGothic", "ZurichBold"):
            for glyph in native_glyphs(font):
                name = names.get(glyph["rice"])
                if not name:
                    # The source pack intentionally has no tilde for either font.
                    if glyph["character"] != "~":
                        raise ValueError("Missing font image: "+str(glyph))
                    continue
                png = archive.read(name)
                mask = Image.open(io.BytesIO(png)).convert("RGB").getchannel("R")
                width, height = glyph["stride"]*8, glyph["height"]*8
                # Packs include power-of-two padding. Preserve the authored 8x
                # coordinates, fitting rare overhanging ink into its native cell.
                box = mask.getbbox()
                ink_width = glyph["width"]*8
                if box and (box[2] > ink_width or box[3] > height):
                    # Exclude unused power-of-two padding before fitting ink;
                    # shrinking the whole padded tile would make letters narrow.
                    mask = mask.crop((0, 0, min(mask.width, max(ink_width, box[2])),
                                      min(mask.height, max(height, box[3]))))
                    mask = mask.resize((min(mask.width, ink_width), min(mask.height, height)), Image.Resampling.LANCZOS)
                canvas = Image.new("L", (width, height))
                canvas.paste(mask.crop((0, 0, min(mask.width, ink_width), min(mask.height, height))), (0, 0))
                encoded = encode(canvas.tobytes())
                records.append((glyph["key"], glyph["stride"], glyph["height"], len(payload), len(encoded)))
                sources.append(dict(glyph, source=name.split("/",1)[1], png_sha256=hashlib.sha256(png).hexdigest(),
                                    mask_sha256=hashlib.sha256(canvas.tobytes()).hexdigest()))
                payload.extend(encoded)
    if len({record[0] for record in records}) != len(records):
        raise ValueError("Ambiguous native glyph fingerprints")
    lines = ["// Generated by scripts/port/generate_enhanced_fonts.py; community masks, CC0.",
             "// Source: "+SOURCE+" @ "+COMMIT, "static const FontRecord font_records[] = {"]
    lines += [f"    {{0x{key:016x}ULL, {w}, {h}, {offset}, {size}}}," for key,w,h,offset,size in sorted(records)]
    lines += ["};", "static const uint8_t font_masks[] = {"]
    lines += ["    "+",".join(str(b) for b in payload[i:i+32])+"," for i in range(0,len(payload),32)]
    lines += ["};", ""]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    manifest.write_text(json.dumps(dict(source=SOURCE, commit=COMMIT, license="CC0-1.0",
                                       archive_sha256=PACK_SHA256, glyphs=sources), indent=2)+"\n", encoding="utf-8")
    print(f"Prepared {len(records)} community glyphs, {len(payload):,} RLE bytes")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pack", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=ROOT/"port/fast3d/fonts/gepd_font_data.inc")
    parser.add_argument("--manifest", type=Path, default=ROOT/"port/fast3d/fonts/sources.json")
    args = parser.parse_args()
    generate(args.pack, args.output, args.manifest)
