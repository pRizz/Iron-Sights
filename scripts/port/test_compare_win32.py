"""Tests for the Windows executable comparison: synthetic PE files and temporary Git repositories."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('compare_win32', Path(__file__).with_name('compare_win32.py'))
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)


def pe(text=b'\x90' * 16, stamp=0, extra=None, *, entry=0x1000, image_base=0x400000, size_of_image=0x10000,
       directories=None, vsizes=None, flags=None, sections=None):
    """Build a minimal PE32 file: a .text section plus the `extra` name-to-bytes sections.

    `directories` maps a data-directory index to (address, size); `vsizes` and `flags` map a section
    name to its VirtualSize and Characteristics; `sections` replaces the section list (name, bytes).
    """
    listed = sections if sections is not None else [('.text', text)] + list((extra or {}).items())
    optional = bytearray(224)
    struct.pack_into('<H', optional, 0, 0x10B)
    struct.pack_into('<IIII', optional, 16, entry, 0, 0, 0)
    struct.pack_into('<I', optional, 28, image_base)
    struct.pack_into('<II', optional, 32, 0x1000, 0x200)
    struct.pack_into('<I', optional, 56, size_of_image)
    struct.pack_into('<I', optional, 92, 16)
    for index, (address, size) in (directories or {}).items():
        struct.pack_into('<II', optional, 96 + 8 * index, address, size)
    header_end = 0x40 + 4 + 20 + len(optional) + 40 * len(listed)
    offset = (header_end + 0x1FF) & ~0x1FF
    table = b''
    body = b''
    for name, data in listed:
        table += struct.pack('<8sIIIIIIHHI', name.encode(), (vsizes or {}).get(name, len(data)), 0x1000,
                             len(data), offset + len(body) if data else 0, 0, 0, 0, 0,
                             (flags or {}).get(name, 0x60000020))
        body += data
    coff = struct.pack('<HHIIIHH', 0x14C, len(listed), stamp, 0, 0, len(optional), 0x102)
    dos = b'MZ' + b'\0' * 0x3A + struct.pack('<I', 0x40)
    return dos + b'PE\0\0' + coff + bytes(optional) + table + b'\0' * (offset - header_end) + body


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

    def test_duplicate_section_names_are_rejected(self):
        # Arrange
        data = pe(sections=[('.text', b'a' * 16), ('.text', b'b' * 16)])
        # Act / Assert
        with self.assertRaises(ValueError):
            compare.pe_sections(data)

    def test_loaded_drops_the_build_id(self):
        # Arrange
        sections = {'.text': b'a', '.buildid': b'b'}
        # Act / Assert
        self.assertEqual(list(compare.loaded(sections)), ['.text'])

    def test_growing_a_section_with_no_raw_data_is_a_difference(self):
        # Arrange
        a = pe(extra={'.bss': b''}, vsizes={'.bss': 0x100})
        b = pe(extra={'.bss': b''}, vsizes={'.bss': 0x200})
        # Act
        result = compare.differences(a, b)
        # Assert
        self.assertEqual(len(result), 1)
        self.assertIn('.bss: VirtualSize', result[0])

    def test_characteristics_alone_are_a_difference(self):
        # Act
        result = compare.differences(pe(flags={'.text': 0x60000020}), pe(flags={'.text': 0xE0000020}))
        # Assert
        self.assertEqual(len(result), 1)
        self.assertIn('Characteristics', result[0])

    def test_entry_point_alone_is_a_difference(self):
        # Act
        result = compare.differences(pe(entry=0x1000), pe(entry=0x1010))
        # Assert
        self.assertEqual(len(result), 1)
        self.assertIn('AddressOfEntryPoint', result[0])

    def test_image_base_alone_is_a_difference(self):
        # Act
        result = compare.differences(pe(image_base=0x400000), pe(image_base=0x10000000))
        # Assert
        self.assertEqual(len(result), 1)
        self.assertIn('ImageBase', result[0])

    def test_import_directory_size_is_a_difference(self):
        # Act
        result = compare.differences(pe(directories={1: (0x2000, 0x40)}), pe(directories={1: (0x2000, 0x50)}))
        # Assert
        self.assertEqual(len(result), 1)
        self.assertIn('Import', result[0])

    def test_debug_directory_alone_is_not_a_difference(self):
        # Act
        result = compare.differences(pe(directories={6: (0x3000, 0x1C)}), pe(directories={6: (0x3400, 0x38)}))
        # Assert
        self.assertEqual(result, [])

    def test_size_of_image_alone_is_not_a_difference(self):
        self.assertEqual(compare.differences(pe(size_of_image=0x10000), pe(size_of_image=0x12000)), [])

    def test_a_differing_build_id_is_reported_as_identity_not_difference(self):
        # Arrange
        a = pe(extra={'.buildid': b'a' * 32})
        b = pe(extra={'.buildid': b'b' * 32})
        # Act / Assert
        self.assertEqual(compare.differences(a, b), [])
        self.assertEqual(compare.identity_differences(a, b), ['.buildid'])

    def test_report_says_when_only_the_build_id_differs(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder:
            a, b = Path(folder)/'a.exe', Path(folder)/'b.exe'
            a.write_bytes(pe(extra={'.buildid': b'a' * 32}))
            b.write_bytes(pe(extra={'.buildid': b'b' * 32}))
            # Act
            with contextlib.redirect_stdout(io.StringIO()) as printed:
                status = compare.report(a, b)
        # Assert
        self.assertEqual(status, 0)
        self.assertEqual(printed.getvalue().strip(), 'identical in loaded code and data; .buildid differs')


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
