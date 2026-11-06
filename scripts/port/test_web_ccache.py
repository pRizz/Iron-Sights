#!/usr/bin/env python3
"""ROM- and network-free checks of the browser build's pinned compiler cache."""
import hashlib
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from web_ccache import docker_arguments, ensure, pin  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def archive_bytes(member, body):
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode='w:gz') as bundle:
        info = tarfile.TarInfo(member)
        info.size = len(body)
        bundle.addfile(info, io.BytesIO(body))
    return buffer.getvalue()


class WebCcacheTests(unittest.TestCase):
    def entry(self, data):
        return pin(dict(ccache=dict(
            version='4.14.2', sha256=hashlib.sha256(data).hexdigest(),
            url='https://github.com/ccache/ccache/releases/download/v4.14.2/ccache-4.14.2-linux-x86_64-musl-static.tar.gz')))

    def test_repository_pin_is_the_official_static_release(self):
        # Arrange / Act
        entry = pin(json.loads((ROOT / 'scripts/port/web_toolchain.json').read_text(encoding='utf-8')))
        # Assert
        self.assertTrue(entry['url'].startswith('https://github.com/ccache/ccache/releases/download/v'))
        self.assertTrue(entry['member'].endswith('-musl-static/ccache'))

    def test_pin_rejects_another_url(self):
        # Arrange
        config = dict(ccache=dict(version='4.14.2', sha256='a' * 64, url='https://example.com/ccache.tar.gz'))
        # Act / Assert
        with self.assertRaisesRegex(ValueError, 'official static release'):
            pin(config)

    def test_downloads_once_and_extracts_the_binary(self):
        # Arrange
        data = archive_bytes('ccache-4.14.2-linux-x86_64-musl-static/ccache', b'binary')
        entry = self.entry(data)
        calls = []

        def download(url, path):
            calls.append(url)
            Path(path).write_bytes(data)

        with tempfile.TemporaryDirectory() as directory:
            # Act
            first = ensure(Path(directory), entry, download)
            second = ensure(Path(directory), entry, download)
            # Assert
            self.assertEqual(first.read_bytes(), b'binary')
            self.assertEqual(first, second)
            self.assertEqual(len(calls), 1)

    def test_rejects_a_mismatched_download(self):
        # Arrange
        entry = self.entry(b'expected')

        def download(url, path):
            Path(path).write_bytes(b'tampered')

        with tempfile.TemporaryDirectory() as directory:
            # Act / Assert
            with self.assertRaisesRegex(ValueError, 'pinned SHA-256'):
                ensure(Path(directory), entry, download)
            self.assertEqual(list(Path(directory).glob('*.tar.gz*')), [])

    def test_docker_arguments_wrap_emcc_with_the_cache(self):
        # Arrange
        with tempfile.TemporaryDirectory() as directory:
            # Act
            arguments = docker_arguments(Path(directory) / 'ccache', Path(directory) / 'objects')
            # Assert
            self.assertIn('EM_COMPILER_WRAPPER=ccache', arguments)
            self.assertTrue((Path(directory) / 'objects').is_dir())


if __name__ == '__main__':
    unittest.main()
