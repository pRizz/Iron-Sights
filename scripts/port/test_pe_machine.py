"""ROM-free tests of pe_machine.py: a PE executable's machine, and the gallery's capture-build check."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pe_machine  # noqa: E402


def pe_bytes(machine):
    data = bytearray(512)
    data[:2] = b'MZ'
    data[0x3C:0x40] = (0x80).to_bytes(4, 'little')
    data[0x80:0x84] = b'PE\0\0'
    data[0x84:0x86] = machine.to_bytes(2, 'little')
    return bytes(data)


class Machine(unittest.TestCase):
    def test_each_windows_width_and_a_linux_build_are_told_apart(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder:
            x86, x64, elf = Path(folder)/'x86.exe', Path(folder)/'x64.exe', Path(folder)/'IronSights'
            x86.write_bytes(pe_bytes(pe_machine.I386))
            x64.write_bytes(pe_bytes(pe_machine.AMD64))
            elf.write_bytes(b'\x7fELF' + bytes(60))

            # Act / Assert
            self.assertEqual(pe_machine.pe_machine(x86), pe_machine.I386)
            self.assertEqual(pe_machine.pe_machine(x64), pe_machine.AMD64)
            self.assertIsNone(pe_machine.pe_machine(elf))


class GalleryCaptureBuild(unittest.TestCase):
    def test_a_64_bit_windows_exe_is_refused_by_name(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder)/'IronSights.exe'
            exe.write_bytes(pe_bytes(pe_machine.AMD64))
            with self.assertRaisesRegex(ValueError, '64-bit Windows build'):
                pe_machine.gallery_capture_build(exe)

    def test_a_32_bit_windows_exe_and_a_linux_build_are_accepted(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder)/'IronSights.exe'
            exe.write_bytes(pe_bytes(pe_machine.I386))
            elf = Path(folder)/'IronSights'
            elf.write_bytes(b'\x7fELF' + bytes(60))
            self.assertEqual(pe_machine.gallery_capture_build(exe), exe.resolve())
            self.assertEqual(pe_machine.gallery_capture_build(elf), elf.resolve())


if __name__ == '__main__':
    unittest.main()
