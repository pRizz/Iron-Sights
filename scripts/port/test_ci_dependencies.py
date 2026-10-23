"""ROM-free checks of the dependency cache's signed-metadata trust boundary."""
import hashlib
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from ci_dependencies import archive_record, verify_archives, compiler_identity


class DependencyCache(unittest.TestCase):
    def test_object_identity_tracks_tools_across_hosted_image_rollouts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for tool in ("i686-w64-mingw32-gcc", "i686-w64-mingw32-g++", "cc1", "cc1plus"):
                (root/tool).write_bytes(tool.encode())
            plan = dict(requested={"compiler": "1"}, ubuntu="noble", architecture="amd64")
            def command(args, capture=False):
                return "ccache fixture" if args[0] == "ccache" else str(root/args[1].split("=")[1])
            with patch("ci_dependencies.run", side_effect=command), \
                    patch("ci_dependencies.shutil.which", side_effect=lambda name: str(root/name)):
                with patch.dict(os.environ, ImageVersion="old-image"):
                    original = compiler_identity(plan)
                with patch.dict(os.environ, ImageVersion="new-image"):
                    self.assertEqual(original, compiler_identity(plan))
                (root/"cc1").write_bytes(b"updated frontend")
                self.assertNotEqual(original, compiler_identity(plan))
                changed = dict(plan, requested={"compiler": "2"})
                self.assertNotEqual(compiler_identity(plan), compiler_identity(changed))

    def test_signed_metadata_must_identify_exact_archive_with_sha256(self):
        metadata = ("Package: fixture\nVersion: 1\nArchitecture: amd64\n"
                    "Filename: pool/fixture_1_amd64.deb\nSize: 4\nSHA256: "+"a"*64+"\n")
        line = "'https://archive.ubuntu.com/ubuntu/pool/fixture_1_amd64.deb' fixture_1_amd64.deb 4 MD5Sum:ignored"
        self.assertEqual(archive_record(line, metadata)["sha256"], "a"*64)
        for bad in (metadata.replace("SHA256:", "MD5Sum:"),
                    metadata.replace("Size: 4", "Size: 5"), metadata+"\n"+metadata,
                    metadata.replace("fixture_1_amd64.deb", "another_1_amd64.deb")):
            with self.assertRaises(ValueError):
                archive_record(line, bad)
        with self.assertRaises(ValueError):
            archive_record(line.replace(" fixture_1_amd64.deb ", " ../fixture_1_amd64.deb "), metadata)

    def test_cache_contents_cannot_override_fresh_trusted_plan(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            valid = b"authenticated archive fixture"
            name = "fixture_1_amd64.deb"
            records = [dict(name=name, size=len(valid), sha256=hashlib.sha256(valid).hexdigest())]
            (root/name).write_bytes(valid)
            self.assertEqual(verify_archives(root, records, require=True), [])
            # Same size and name: hashing must catch tampering, not just stat().
            (root/name).write_bytes(b"X"*len(valid))
            (root/"obsolete_1_amd64.deb").write_bytes(valid)
            self.assertEqual(sorted(verify_archives(root, records)), [name, "obsolete_1_amd64.deb"])
            with self.assertRaises(ValueError):
                verify_archives(root, records, require=True)
            (root/name).write_bytes(valid)
            self.assertEqual(verify_archives(root, records, require=True), [])


if __name__ == "__main__":
    unittest.main()
