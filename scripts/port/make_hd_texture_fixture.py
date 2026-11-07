"""Build the ROM-free pack fixture for test_hd_textures.cpp (FORK-053).

    python3 scripts/port/make_hd_texture_fixture.py OUT_DIR

Writes a pack folder the way a player might leave one: a ZIP holding a
deflated PNG (A), a stored PNG (B), an out-of-scope PNG (D), a corrupt PNG
(E) and a text file, plus an extracted PNG (C) in a subfolder. The images are
tiny synthetic patterns, not artwork.
"""
import struct
import sys
import zipfile
import zlib
from pathlib import Path


def png(width, height, colour_type, pixel):
    """A PNG of width x height filled per pixel(x, y) -> bytes."""
    rows = b''.join(b'\x00' + b''.join(pixel(x, y) for x in range(width)) for y in range(height))

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))

    header = struct.pack('>IIBBBBB', width, height, 8, colour_type, 0, 0, 0)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')


def main():
    out = Path(sys.argv[1])
    (out / 'extracted' / 'Areas').mkdir(parents=True, exist_ok=True)
    a = png(8, 4, 6, lambda x, y: bytes((10 + x, 20 + y, 30, 255)))
    b = png(16, 16, 2, lambda x, y: bytes((x * 16, y * 16, 0)))
    d = png(4, 4, 6, lambda x, y: bytes((1, 2, 3, 4)))
    e = png(4, 4, 6, lambda x, y: bytes((5, 6, 7, 8)))[:40]  # cut short: unreadable
    with zipfile.ZipFile(out / 'pack.zip', 'w') as z:
        z.writestr('GOLDENEYE/Areas/A/GOLDENEYE#AAAA0001#3#1_all.png', a, compress_type=zipfile.ZIP_DEFLATED)
        z.writestr('GOLDENEYE/Areas/B/GOLDENEYE#BBBB0002#2#1#$_ciByRGBA.png', b, compress_type=zipfile.ZIP_STORED)
        z.writestr('GOLDENEYE/Areas/D/GOLDENEYE#DDDD0004#3#1_all.png', d, compress_type=zipfile.ZIP_DEFLATED)
        z.writestr('GOLDENEYE/Areas/E/GOLDENEYE#EEEE0005#3#1_all.png', e, compress_type=zipfile.ZIP_STORED)
        z.writestr('README.txt', 'not a texture')
    c = png(4, 4, 6, lambda x, y: bytes((200, 100, 50, 128)))
    (out / 'extracted' / 'Areas' / 'GOLDENEYE#CCCC0003#4#1_all.png').write_bytes(c)


if __name__ == '__main__':
    main()
