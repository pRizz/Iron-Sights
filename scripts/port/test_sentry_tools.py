"""ROM-free checks of Sentry artifact integrity and public configuration."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("sentry_tools", Path(__file__).with_name("sentry_tools.py"))
tools = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tools)


class SentryTools(unittest.TestCase):
    def test_header_escapes_public_dsn(self):
        # Arrange
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "generated/config.h"
            # Act
            tools.header(output, "http://public@127.0.0.1:1234/1")
            # Assert
            self.assertIn('"http://public@127.0.0.1:1234/1"', output.read_text())

    def test_header_rejects_arbitrary_plain_http_destination(self):
        # Arrange
        with tempfile.TemporaryDirectory() as temporary:
            # Act / Assert
            with self.assertRaises(ValueError):
                tools.header(Path(temporary) / "config.h", "http://untrusted.example/1")

    def test_stage_rejects_changed_runtime_bytes(self):
        # Arrange
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "sdk"
            source.mkdir()
            info = dict(sdk_commit=tools.CONFIG["sdk_commit"], architecture="x86", sha256={})
            for name in tools.RUNTIME_FILES:
                (source / name).write_bytes(b"runtime")
                info["sha256"][name] = tools.hashlib.sha256(b"runtime").hexdigest()
            (source / "sentry-build.json").write_text(json.dumps(info))
            (source / "sentry.dll").write_bytes(b"tampered")
            # Act / Assert
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                tools.stage(source, Path(temporary) / "player")

    def test_stage_rejects_wrong_sdk_revision(self):
        # Arrange
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            (source / "sentry-build.json").write_text(json.dumps(dict(sdk_commit="wrong", architecture="x86")))
            # Act / Assert
            with self.assertRaisesRegex(ValueError, "pinned"):
                tools.stage(source, source / "player")


if __name__ == "__main__":
    unittest.main()
