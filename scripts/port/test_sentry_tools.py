"""ROM-free checks of Sentry artifact integrity and public configuration."""
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("sentry_tools", Path(__file__).with_name("sentry_tools.py"))
tools = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tools)


class SentryTools(unittest.TestCase):
    def cli_fixture(self, work, data):
        target = work / ("sentry-cli.exe" if tools.os.name == "nt" else "sentry-cli")
        filename = "sentry-cli-Windows-x86_64.exe" if tools.os.name == "nt" else "sentry-cli-Linux-x86_64"
        config = dict(tools.CONFIG, cli_sha256={filename: tools.hashlib.sha256(data).hexdigest()})
        return target, filename, config

    def test_valid_cli_cache_needs_no_github_api_or_download(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            target, _, config = self.cli_fixture(work, b"verified CLI")
            target.write_bytes(b"verified CLI")
            with patch.dict(tools.CONFIG, config), patch.object(tools.urllib.request, "urlopen") as download:
                self.assertEqual(tools.cli(work), target)
                download.assert_not_called()

    def test_corrupt_cli_cache_uses_pinned_direct_download(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            target, filename, config = self.cli_fixture(work, b"verified CLI")
            target.write_bytes(b"tampered")
            with patch.dict(tools.CONFIG, config), patch.object(tools.urllib.request, "urlopen", return_value=io.BytesIO(b"verified CLI")) as download:
                self.assertEqual(tools.cli(work).read_bytes(), b"verified CLI")
                download.assert_called_once_with(
                    f"https://github.com/getsentry/sentry-cli/releases/download/{config['cli_version']}/{filename}", timeout=60)

    def test_cli_download_checksum_mismatch_is_rejected_without_execution(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            target, _, config = self.cli_fixture(work, b"verified CLI")
            with patch.dict(tools.CONFIG, config), patch.object(tools.urllib.request, "urlopen", return_value=io.BytesIO(b"tampered")):
                with self.assertRaisesRegex(ValueError, "checksum mismatch"):
                    tools.cli(work)
                self.assertFalse(target.exists())

    def test_cli_rejects_malformed_pin_without_network(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            _, filename, config = self.cli_fixture(work, b"verified CLI")
            config["cli_sha256"][filename] = "invalid"
            with patch.dict(tools.CONFIG, config), patch.object(tools.urllib.request, "urlopen") as download:
                with self.assertRaisesRegex(ValueError, "pinned SHA256"):
                    tools.cli(work)
                download.assert_not_called()

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
