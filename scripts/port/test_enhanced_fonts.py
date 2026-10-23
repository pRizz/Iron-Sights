"""ROM-free font mapping and native decoder checks; no Pillow needed."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
from generate_enhanced_fonts import ROOT, native_glyphs


class EnhancedFonts(unittest.TestCase):
    def test_native_mapping_and_unsupported_character_fallback(self):
        text = (ROOT/"port/fast3d/fonts/gepd_font_data.inc").read_text()
        records = {int(key,16):(int(w),int(h)) for key,w,h in re.findall(r"\{0x([0-9a-f]+)ULL, (\d+), (\d+),", text)}
        self.assertEqual(len(records), 186)
        for font in ("BankGothic", "ZurichBold"):
            for glyph in native_glyphs(font):
                key = glyph["key"]
                if glyph["character"] == "~":
                    self.assertNotIn(key, records)
                else:
                    self.assertEqual(records[key], (glyph["stride"],glyph["height"]))

    @unittest.skipUnless(shutil.which("g++"), "Native font decoder test requires a C++ compiler")
    def test_native_masks_and_malformed_encoding(self):
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder)/"fonts-test"
            subprocess.run(["g++", "-std=c++20", "-O2", "-I"+str(ROOT/"port/fast3d"),
                            str(ROOT/"scripts/port/test_enhanced_fonts.cpp"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
