"""Checks for the vendored open assets (no network, no ROM)."""
import json
import struct
import unittest
from pathlib import Path

import update_open_assets as vendor


class VendoredOpenAssets(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(vendor.MANIFEST.read_text(encoding="utf-8"))

    def test_files_match_the_manifest(self):
        self.assertEqual(vendor.verify(), [])

    def test_license_is_mit(self):
        text = (vendor.VENDOR / "LICENSE").read_text(encoding="utf-8")
        self.assertTrue(text.startswith("MIT License"))
        self.assertEqual(self.manifest["license"], "MIT")

    def test_every_font_is_vendored(self):
        self.assertEqual(vendor.FONTS, ("title", "body"))
        for font in vendor.FONTS:
            for name in vendor.FONT_FILES:
                self.assertIn(f"{font}/{name}", self.manifest["files"])

    def test_metrics_cover_printable_ascii(self):
        for font in vendor.FONTS:
            with self.subTest(font=font):
                metrics = json.loads((vendor.VENDOR / f"{font}/metrics.json").read_text(encoding="utf-8"))
                self.assertEqual(sorted(metrics["glyphs"]), [chr(c) for c in range(0x21, 0x7F)])
                self.assertEqual(metrics["license"], "MIT")
                self.assertEqual("v" + metrics["version"], self.manifest["tag"])
                self.assertEqual(metrics["atlas"]["type"], "mtsdf")

    def test_atlas_matches_its_metrics(self):
        for font in vendor.FONTS:
            with self.subTest(font=font):
                metrics = json.loads((vendor.VENDOR / f"{font}/metrics.json").read_text(encoding="utf-8"))
                png = (vendor.VENDOR / f"{font}/atlas.png").read_bytes()
                self.assertEqual(png[:8], b"\x89PNG\r\n\x1a\n")
                width, height, depth, colour = struct.unpack(">IIBB", png[16:26])
                self.assertEqual((width, height), (metrics["atlas"]["width"], metrics["atlas"]["height"]))
                self.assertEqual((depth, colour), (8, 6))  # 8-bit RGBA
                for ch, glyph in metrics["glyphs"].items():
                    x, y, w, h = glyph["atlas"]
                    self.assertLessEqual(x + w, width, ch)
                    self.assertLessEqual(y + h, height, ch)


if __name__ == "__main__":
    unittest.main()
