"""Generator tests; run with the same Pillow/NumPy environment as generation."""
import importlib.util
from pathlib import Path
import random
import struct
import tempfile
import unittest
from PIL import Image

spec = importlib.util.spec_from_file_location("icons", Path(__file__).with_name("generate_weapon_icons.py"))
icons = importlib.util.module_from_spec(spec)
spec.loader.exec_module(icons)


def unpack(data):
    output = bytearray()
    pos = 0
    while pos < len(data):
        packet, = struct.unpack_from("<H", data, pos)
        pos += 2
        count = packet & 0x7fff
        if packet & 0x8000:
            output += data[pos:pos+4]*count
            pos += 4
        else:
            output += data[pos:pos+count*4]
            pos += count*4
    return bytes(output)


class WeaponIcons(unittest.TestCase):
    def test_packet_boundaries_and_repeatability(self):
        rng = random.Random(317)
        for raw in (b"", b"\0"*4, b"\0"*(32770*4), bytes(range(256))*600,
                    bytes(rng.randrange(256) for _ in range(40000)),
                    b"\0"*80 + b"\1\2\3\4"*2 + b"\0"*4):
            packed = icons.pack_pixels(raw)
            self.assertEqual(unpack(packed), raw)
            self.assertEqual(icons.pack_pixels(raw), packed)

    def test_mattes_and_invalid_capture_detection(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            black = Image.new("RGB", (64, 64))
            white = Image.new("RGB", (64, 64), "white")
            # Opaque and 50% opaque coloured regions, away from the border.
            black.paste((60,100,140), (12,12,36,52))
            white.paste((60,100,140), (12,12,36,52))
            black.paste((30,50,70), (36,12,52,52))
            white.paste((157,177,197), (36,12,52,52))
            for n, image in enumerate((black,white,black,white), 1):
                image.save(path/f"weapon_04_{n}.bmp")
            first, bbox, hashes = icons.recover(path, 4)
            second, _, _ = icons.recover(path, 4)
            self.assertEqual(first.tobytes(), second.tobytes())
            self.assertEqual(bbox, (12,12,52,52))
            self.assertEqual(len(hashes), 2)
            self.assertIn(128, first.getchannel("A").tobytes())
            black.putpixel((25,25), (1,2,3))
            black.save(path/"weapon_04_3.bmp")
            with self.assertRaisesRegex(ValueError, "repeated captures differ"):
                icons.recover(path, 4)
            for n in (1,3):
                Image.new("RGB", (64,64)).save(path/f"weapon_04_{n}.bmp")
            for n in (2,4):
                Image.new("RGB", (64,64), "white").save(path/f"weapon_04_{n}.bmp")
            with self.assertRaisesRegex(ValueError, "empty model"):
                icons.recover(path, 4)


if __name__ == "__main__":
    unittest.main()
