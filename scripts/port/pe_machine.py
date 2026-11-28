"""The machine of a Windows PE executable, for tools that must tell the 32-bit exe from the 64-bit
one (WIN64-003)."""
from pathlib import Path
import struct

I386 = 0x14C
AMD64 = 0x8664


def pe_machine(path):
    """PATH's PE machine field, or None when it is not a PE executable (a Linux build, say)."""
    with open(path, 'rb') as handle:
        header = handle.read(4096)
    if header[:2] != b'MZ' or len(header) < 0x40:
        return None
    pe = struct.unpack_from('<I', header, 0x3C)[0]
    if pe + 6 > len(header) or header[pe:pe+4] != b'PE\0\0':
        return None
    return struct.unpack_from('<H', header, pe+4)[0]


def gallery_capture_build(path):
    """PATH resolved, when it can be the hand gallery's capture build. The references are the 32-bit
    Windows exe's pictures, so a 64-bit Windows exe raises ValueError naming it."""
    exe = Path(path).resolve(strict=True)
    if pe_machine(exe) == AMD64:
        raise ValueError(f'{exe} is a 64-bit Windows build; the gallery is captured with the 32-bit exe '
                         '(build_win/IronSights.exe or an isolated 32-bit copy)')
    return exe
