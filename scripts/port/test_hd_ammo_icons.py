"""ROM-free checks for the embedded HD ammo icons and their provenance."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/"scripts/port"))
import generate_hd_ammo_icons as generator  # noqa: E402

DATA = ROOT/"port/fast3d/hud_icons"


class HdAmmoIconTests(unittest.TestCase):
    def test_table_matches_sources_and_native_sizes(self):
        sources = json.loads((DATA/"sources.json").read_text(encoding="utf-8"))
        text = (DATA/"hd_ammo_icons.inc").read_text(encoding="utf-8")
        rows = re.findall(r"\{ (\d+), (\d+), (\d+), (\d+), (\d+), (\d+) \}, /\* (\d+): (\w+) \*/", text)
        pixels = int(re.search(r"hdAmmoPixels\[(\d+)\]", text)[1])
        native = generator.native_sizes()
        self.assertEqual(len(rows), len(generator.SLOTS))
        self.assertEqual(sources["source_sha256"], generator.SHA256)
        end = 0
        for (nw, nh, w, h, offset, size, slot, name), row, (image, path) in zip(rows, sources["slots"], generator.SLOTS):
            self.assertEqual((name, row["image"], row["source"]), (image, image, path))
            self.assertEqual((int(nw), int(nh)), native[image])
            if path is None:
                self.assertEqual((int(w), int(h), int(size)), (0, 0, 0))
                continue
            self.assertEqual((int(w), int(h)), (int(nw)*generator.SCALE, int(nh)*generator.SCALE))
            self.assertEqual(int(offset), end)
            end += int(size)
        self.assertEqual(end, pixels)

    def test_run_length_packets_round_trip(self):
        pixels = bytes([1, 2, 3, 4]*5 + [9, 9, 9, 0, 7, 7, 7, 7] + [5, 6, 7, 8]*3)
        packed = generator.rle(pixels)
        out, at = bytearray(), 0
        while at < len(packed):
            count = int.from_bytes(packed[at:at+2], "little")
            at += 2
            if count & 0x8000:
                out += packed[at:at+4]*(count & 0x7fff)
                at += 4
            else:
                out += packed[at:at+count*4]
                at += count*4
        self.assertEqual(bytes(out), pixels)

    def test_notice_keeps_attribution(self):
        notice = (DATA/"NOTICE.txt").read_text(encoding="utf-8")
        for credit in ("intermissionfb", "GhostlyDark", "github.com/GhostlyDark/GoldenEye-007-HD",
                       "no explicit redistribution license"):
            self.assertIn(credit, notice)

    @unittest.skipUnless(shutil.which("g++"), "Native ammo icon decoder test requires a C++ compiler")
    def test_native_decoder_and_tile_layout(self):
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder)/"ammo-test"
            subprocess.run(["g++", "-std=c++20", "-O2", "-I"+str(ROOT/"port/fast3d"), "-I"+str(ROOT/"port/include"),
                            str(ROOT/"scripts/port/test_hd_ammo_icons.cpp"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
