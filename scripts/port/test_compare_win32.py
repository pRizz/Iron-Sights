"""Tests for the Windows executable comparison: synthetic PE files and temporary Git repositories."""
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('compare_win32', Path(__file__).with_name('compare_win32.py'))
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)


def pe(text=b'\x90' * 16, stamp=0, extra=None):
    """Build a minimal PE32 file: a .text section plus the `extra` name-to-bytes sections."""
    sections = {'.text': text, **(extra or {})}
    optional = b'\0' * 224
    header_end = 0x40 + 4 + 20 + len(optional) + 40 * len(sections)
    offset = (header_end + 0x1FF) & ~0x1FF
    table = b''
    body = b''
    for name, data in sections.items():
        table += struct.pack('<8sIIIIIIHHI', name.encode(), len(data), 0x1000, len(data), offset + len(body),
                             0, 0, 0, 0, 0x60000020)
        body += data
    coff = struct.pack('<HHIIIHH', 0x14C, len(sections), stamp, 0, 0, len(optional), 0x102)
    dos = b'MZ' + b'\0' * 0x3A + struct.pack('<I', 0x40)
    return dos + b'PE\0\0' + coff + optional + table + b'\0' * (offset - header_end) + body


def git(root, *args):
    env = {**os.environ, 'GIT_CONFIG_GLOBAL': str(root.parent/'empty.gitconfig'), 'GIT_CONFIG_NOSYSTEM': '1'}
    subprocess.run(['git', '-C', str(root), *args], check=True, capture_output=True, env=env)


class Comparison(unittest.TestCase):
    def test_identical_files_have_no_differences(self):
        self.assertEqual(compare.differences(pe(text=b'\x90' * 64), pe(text=b'\x90' * 64)), [])

    def test_timestamp_alone_is_not_a_difference(self):
        self.assertEqual(compare.differences(pe(stamp=1), pe(stamp=2)), [])

    def test_debug_sections_alone_are_not_a_difference(self):
        # Arrange
        a = pe(extra={'.debug_info': b'a' * 32, '/4': b'x' * 8})
        b = pe(extra={'.debug_info': b'b' * 48, '/4': b'y' * 8})
        # Act / Assert
        self.assertEqual(compare.differences(a, b), [])

    def test_one_byte_of_code_is_a_difference(self):
        # Act
        result = compare.differences(pe(text=b'\x90' * 64), pe(text=b'\x90' * 63 + b'\xcc'))
        # Assert
        self.assertEqual(len(result), 1)
        self.assertIn('.text', result[0])

    def test_a_missing_data_section_is_a_difference(self):
        # Act
        result = compare.differences(pe(extra={'.rdata': b'r' * 16}), pe())
        # Assert
        self.assertIn('.rdata', result[0])

    def test_an_extra_data_section_is_a_difference(self):
        # Act
        result = compare.differences(pe(), pe(extra={'.data': b'd' * 16}))
        # Assert
        self.assertIn('.data', result[0])

    def test_non_pe_input_is_rejected(self):
        with self.assertRaises(ValueError):
            compare.pe_sections(b'\x7fELF' + b'\0' * 60)

    def test_truncated_pe_input_is_rejected(self):
        with self.assertRaises(ValueError):
            compare.pe_sections(pe()[:0x50])

    def test_loaded_drops_only_debug_sections(self):
        # Arrange
        sections = {'.text': b'a', '.debug_line': b'b', '/19': b'c', '.rdata': b'd'}
        # Act / Assert
        self.assertEqual(sorted(compare.loaded(sections)), ['.rdata', '.text'])


class Export(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        base = Path(self.tmp.name)
        (base/'empty.gitconfig').write_text('')
        self.root = base/'repo'
        self.root.mkdir()
        self.destination = base/'out'
        git(self.root, 'init', '-q')
        (self.root/'a.c').write_text('one')
        git(self.root, 'add', 'a.c')
        git(self.root, '-c', 'user.name=t', '-c', 'user.email=t@t', 'commit', '-q', '-m', 'one')
        (self.root/'a.c').write_text('two')
        (self.root/'b.c').write_text('untracked')

    def test_worktree_export_includes_uncommitted_edits_and_skips_untracked(self):
        # Act
        compare.export_source('WORKTREE', self.destination, self.root)
        # Assert
        self.assertEqual((self.destination/'a.c').read_text(), 'two')
        self.assertFalse((self.destination/'b.c').exists())

    def test_commit_export_ignores_uncommitted_edits(self):
        # Act
        compare.export_source('HEAD', self.destination, self.root)
        # Assert
        self.assertEqual((self.destination/'a.c').read_text(), 'one')


if __name__ == '__main__':
    unittest.main()
