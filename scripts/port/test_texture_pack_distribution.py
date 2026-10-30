"""ROM-free artwork allowlist, validation and provenance tests."""
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest

from texture_pack_distribution import hd_provenance, texture_files


def fixture(directory):
    directory.mkdir(parents=True, exist_ok=True)
    data = b"ISTEX001" + struct.pack("<II", 1, 53) + struct.pack("<9I", 1, 0, 4, 1, 1, 1, 52, 1, 0) + b"x"
    (directory / "hd.istex").write_bytes(data)
    report = dict(pack_bytes=len(data), keys=1, source="fixture", source_sha256="0" * 64,
                  scope="levels", unique_images=1, rgba_pixels_preserved=True)
    (directory / "hd.json").write_text(json.dumps(report))
    for name in ("HD-Textures-NOTICE.txt", "puff-LICENSE.txt"):
        (directory / name).write_text("fixture notice")
    return data


class TextureDistribution(unittest.TestCase):
    def test_allowlist_and_exact_provenance(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            data = fixture(root)
            (root / "unrelated.zip").write_bytes(b"do not ship")
            self.assertEqual(set(texture_files(root)), {"hd.istex", "hd.json", "HD-Textures-NOTICE.txt", "puff-LICENSE.txt"})
            self.assertEqual(hd_provenance(root)["pack_sha256"], hashlib.sha256(data).hexdigest())

    def test_missing_invalid_mismatched_or_uncredited_art_fails_release(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            with self.assertRaises(ValueError):
                texture_files(root)
            data = fixture(root)
            for invalid in (b"bad", data[:12] + bytes(4) + data[16:]):
                (root / "hd.istex").write_bytes(invalid)
                with self.assertRaises(ValueError):
                    texture_files(root)
            fixture(root)
            (root / "hd.json").write_text('{"pack_bytes":53,"keys":2}')
            with self.assertRaises(ValueError):
                texture_files(root)
            fixture(root)
            (root / "puff-LICENSE.txt").unlink()
            with self.assertRaises(ValueError):
                texture_files(root)


if __name__ == "__main__":
    unittest.main()
