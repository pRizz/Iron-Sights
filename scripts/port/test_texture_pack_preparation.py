"""Synthetic-art pack preparation tests; no downloaded pack or ROM required."""
import contextlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile
import zlib
from PIL import Image
import prepare_texture_pack as pack


class Preparation(unittest.TestCase):
    def image(self, color=(10, 20, 30, 127)):
        output = io.BytesIO()
        Image.new("RGBA", (16, 8), color).save(output, format="PNG")
        return output.getvalue()

    def prepare(self, entries, scope="all"):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        root = Path(folder.name)
        source, output = root/"input.zip", root/"hd.istex"
        with zipfile.ZipFile(source, "w") as archive:
            for name, data in entries:
                archive.writestr(name, data)
        with contextlib.redirect_stdout(io.StringIO()):
            report = pack.prepare(source, output, scope)
        return output, report

    def test_exact_image_aliases_share_one_compressed_payload(self):
        # Arrange / Act
        output, report = self.prepare([(f"GOLDENEYE#{i:08X}#4#1_all.png", self.image()) for i in (1, 2)])
        # Assert: separate match keys retain one pixel-identical blob.
        self.assertEqual((report["keys"], report["unique_images"], report["duplicate_images"]), (2, 1, 1))
        data = output.read_bytes()
        a, b = struct.unpack_from("<9I", data, 16), struct.unpack_from("<9I", data, 52)
        self.assertEqual(a[6:], b[6:])
        filtered = zlib.decompress(data[a[6]:a[6]+a[7]], -15)
        self.assertEqual(len(filtered), (16*4+1)*8)
        self.assertEqual(a[8], zlib.adler32(bytes((10,20,30,127))*16*8))

    def test_ambiguous_match_keys_are_rejected(self):
        # Arrange / Act / Assert: silently choosing by folder order is unsafe.
        name = "GOLDENEYE#12345678#2#1#87654321_ciByRGBA.png"
        with self.assertRaisesRegex(ValueError, "Conflicting artwork"):
            self.prepare([("a/"+name, self.image()), ("b/"+name, self.image((255,0,0,0)))])

    def test_dam_scope_excludes_ui_and_unrelated_keys(self):
        # Arrange / Act
        known = json.loads((pack.ROOT/"scripts/port/hd_dam_sources.json").read_text())["textures"][0]
        _, report = self.prepare([(known, self.image()), ("UI/GOLDENEYE#00000001#4#1_all.png", self.image())], "dam")
        # Assert
        self.assertEqual(report["keys"], 1)
        self.assertEqual(report["textures"][0]["path"], known)

    def test_only_supported_pack_paths_become_keys(self):
        # Arrange / Act / Assert
        self.assertEqual(pack.key("folder/GOLDENEYE#12345678#2#1#87654321_ciByRGBA.png"), (0x12345678,0x87654321,2,1))
        for name in ("README.md", "GOLDENEYE#xyz#4#1_all.png", "GOLDENEYE#12345678#9#1_all.png"):
            self.assertIsNone(pack.key(name))

    def test_reviewed_native_palette_aliases_share_the_same_artwork(self):
        # Arrange / Act
        alias = json.loads((pack.ROOT/"scripts/port/hd_dam_native_aliases.json").read_text())["aliases"][0]
        texture, palette, fmt, size = alias["pack_key"]
        name = f"GOLDENEYE#{texture:08X}#{fmt}#{size}#{palette:08X}_ciByRGBA.png"
        output, report = self.prepare([(name, self.image())])
        # Assert: only the exact reviewed native fingerprint gets another key.
        self.assertEqual((report["source_keys"], report["native_palette_aliases"], report["unique_images"]), (1,1,1))
        data = output.read_bytes()
        keys = [struct.unpack_from("<9I",data,16+36*i) for i in range(2)]
        self.assertEqual({item[1] for item in keys}, {palette,alias["native_palette"]})
        self.assertEqual(keys[0][6:],keys[1][6:])


if __name__ == "__main__":
    unittest.main()
