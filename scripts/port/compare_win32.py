#!/usr/bin/env python3
"""Prove that a change leaves the 32-bit Windows executable unchanged.

`commits REF_A REF_B` exports both source states, builds each with the README's
Windows configuration in the `goldeneye` container (mounted at the same path both
times, because the compiler embeds source paths), and compares what the loader
maps. `compare EXE_A EXE_B` compares two existing executables. Either ref may be
`WORKTREE`: the files on disk now, tracked files with uncommitted edits plus
untracked files Git does not ignore (the build globs source directories, so a
new file must be in the export before it is added). Ignored files stay out.

Compared: every loaded section's bytes, VirtualSize, VirtualAddress and
Characteristics (sections with no raw data, such as .bss, included; a section in
only one file is a difference, one named twice in a file is an error); the COFF
Machine and Characteristics; the optional-header fields in HEADER_FIELDS (entry
point, image base, alignments, versions, subsystem, DLL characteristics, stack
and heap sizes, loader flags, code and data bases and size totals); every data
directory's address and size except Debug and Security.

Deliberately not compared:
  * Debug sections: a section whose name marks it as debug (`.debug*`, or a
    `/N` long name) and whose flags mark it discardable. A long-named section
    the loader keeps is compared.
  * .buildid's bytes: an identifier derived from the whole file, debug
    information included, so it changes when only debug types change. If
    everything else is identical the tool says "identical in loaded code and
    data; .buildid differs" and exits 0. A .buildid in only one file is a
    difference.
  * COFF TimeDateStamp and optional-header CheckSum: build stamps.
  * SizeOfImage, SizeOfHeaders, NumberOfSections, the COFF symbol-table pointer
    and count, the linker version, and section file offsets: they describe the
    file, which debug information also fills, rather than what is loaded.
  * The Debug directory (it points into debug information) and the Security
    directory (a file offset to data that is not loaded).
Both builds run with SOURCE_DATE_EPOCH exported so the build's own clock stamps
(the About-page time, __DATE__/__TIME__, linker timestamps) match.

Exit status: 0 identical in loaded code and data, 1 different, 2 could not
compare (a build, Git or Docker failure, or an input that is not a PE file).
Build logs are kept in build/lp64/compare.
"""
import argparse
import importlib.util
import io
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tarfile
import time
from typing import Optional

ROOT = Path(__file__).resolve().parents[2]
COMPARE_DIR = ROOT/'build'/'lp64'/'compare'
WORKTREE = 'WORKTREE'
SDL2_ROOT = '$HOME/sdl2-mingw/SDL2-2.30.11/i686-w64-mingw32'
CONFIGURE = ('cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Debug '
             f'-DGE_VIDEO=ON -DSDL2_MINGW_ROOT={SDL2_ROOT}')
# Both builds see one clock: the build's About-page stamp (port/src/port_build_time.c via
# scripts/port/build_info.py), GCC's __DATE__/__TIME__ and the linker's header stamps all honour it.
SOURCE_DATE_EPOCH = '1700000000'
# Sections that identify a build rather than make up its program: `.buildid` holds a hash of the
# whole output file, debug information included, so it changes whenever only debug types change.
IDENTITY_SECTIONS = ('.buildid',)
DIRECTORY_NAMES = {0: 'Export', 1: 'Import', 2: 'Resource', 3: 'Exception', 5: 'BaseRelocation', 9: 'TLS',
                   12: 'IAT'}
IGNORED_DIRECTORIES = (4, 6)  # Security (a file offset, not loaded) and Debug (debug information)
IMAGE_SCN_MEM_DISCARDABLE = 0x02000000
COFF_FIELDS = (('Machine', '<H', 0), ('Characteristics', '<H', 18))
# Optional-header fields compared: (name, format in PE32, offset in PE32, format in PE32+, offset in
# PE32+); None where the field does not exist. An explicit list, not "every field but the stamps",
# because some header fields describe the file, which debug information also fills.
HEADER_FIELDS = (
    ('Magic', '<H', 0, '<H', 0),
    ('SizeOfCode', '<I', 4, '<I', 4),
    ('SizeOfInitializedData', '<I', 8, '<I', 8),
    ('SizeOfUninitializedData', '<I', 12, '<I', 12),
    ('AddressOfEntryPoint', '<I', 16, '<I', 16),
    ('BaseOfCode', '<I', 20, '<I', 20),
    ('BaseOfData', '<I', 24, None, None),
    ('ImageBase', '<I', 28, '<Q', 24),
    ('SectionAlignment', '<I', 32, '<I', 32),
    ('FileAlignment', '<I', 36, '<I', 36),
    ('MajorOperatingSystemVersion', '<H', 40, '<H', 40),
    ('MinorOperatingSystemVersion', '<H', 42, '<H', 42),
    ('MajorImageVersion', '<H', 44, '<H', 44),
    ('MinorImageVersion', '<H', 46, '<H', 46),
    ('MajorSubsystemVersion', '<H', 48, '<H', 48),
    ('MinorSubsystemVersion', '<H', 50, '<H', 50),
    ('Subsystem', '<H', 68, '<H', 68),
    ('DllCharacteristics', '<H', 70, '<H', 70),
    ('SizeOfStackReserve', '<I', 72, '<Q', 72),
    ('SizeOfStackCommit', '<I', 76, '<Q', 80),
    ('SizeOfHeapReserve', '<I', 80, '<Q', 88),
    ('SizeOfHeapCommit', '<I', 84, '<Q', 96),
    ('LoaderFlags', '<I', 88, '<I', 104),
)
GAME_EXECUTABLES = ('IronSights.exe', 'ProjectMidas.exe')
DEFAULT_DEADLINE_SECONDS = 180  # a build took 55 s here, under emulation
LOG_TAIL_LINES = 30
COULD_NOT_COMPARE = 2


def pe_layout(data: bytes) -> dict:
    """Parse the load-relevant parts of a PE file; ValueError if `data` is not one.

    Returns {'sections': {name: (raw bytes, virtual size, virtual address, characteristics)},
    'header': {field name: value}, 'directories': [(address, size), ...]}.
    """
    if len(data) < 0x40 or data[:2] != b'MZ':
        raise ValueError('not a PE file: no DOS header')
    (pe_offset,) = struct.unpack_from('<I', data, 0x3C)
    if data[pe_offset:pe_offset + 4] != b'PE\0\0':
        raise ValueError('not a PE file: no PE signature')
    coff = pe_offset + 4
    if len(data) < coff + 20:
        raise ValueError('truncated PE file: no COFF header')
    (count,) = struct.unpack_from('<H', data, coff + 2)
    (optional_size,) = struct.unpack_from('<H', data, coff + 16)
    optional = coff + 20
    if optional_size < 96 or len(data) < optional + optional_size:
        raise ValueError('truncated PE file: optional header')
    (magic,) = struct.unpack_from('<H', data, optional)
    if magic not in (0x10B, 0x20B):
        raise ValueError(f'not a PE file: optional header magic {magic:#x}')
    wide = magic == 0x20B
    directories_at = optional + (112 if wide else 96)
    if optional_size < directories_at - optional:
        raise ValueError('truncated PE file: optional header')
    (directory_count,) = struct.unpack_from('<I', data, directories_at - 4)
    header = {name: struct.unpack_from(fmt, data, coff + at)[0] for name, fmt, at in COFF_FIELDS}
    for name, narrow_fmt, narrow_at, wide_fmt, wide_at in HEADER_FIELDS:
        fmt, at = (wide_fmt, wide_at) if wide else (narrow_fmt, narrow_at)
        header[name] = None if fmt is None else struct.unpack_from(fmt, data, optional + at)[0]
    directory_count = min(directory_count, (optional + optional_size - directories_at) // 8)
    directories = [struct.unpack_from('<II', data, directories_at + 8 * i) for i in range(directory_count)]
    table = optional + optional_size
    if len(data) < table + 40 * count:
        raise ValueError('truncated PE file: section table')
    sections = {}
    for index in range(count):
        entry_at = table + 40 * index
        name = data[entry_at:entry_at + 8].rstrip(b'\0').decode('latin-1')
        virtual_size, virtual_address, size, start = struct.unpack_from('<IIII', data, entry_at + 8)
        (characteristics,) = struct.unpack_from('<I', data, entry_at + 36)
        if name in sections:
            raise ValueError(f'PE file has two sections named {name}')
        if size and start + size > len(data):
            raise ValueError(f'truncated PE file: section {name}')
        raw = data[start:start + size] if size else b''
        sections[name] = (raw, virtual_size, virtual_address, characteristics)
    return {'sections': sections, 'header': header, 'directories': directories}


def pe_sections(data: bytes) -> dict[str, bytes]:
    """Return each section's name mapped to its raw bytes; ValueError if `data` is not a PE file
    or names a section twice."""
    return {name: values[0] for name, values in pe_layout(data)['sections'].items()}


def is_debug(name: str, characteristics: int) -> bool:
    """True for a section named as debug information (`.debug*`, or a `/N` long name, which is where
    the linker puts long `.debug_*` names) that is also marked discardable."""
    return name.startswith(('.debug', '/')) and bool(characteristics & IMAGE_SCN_MEM_DISCARDABLE)


def loaded(sections: dict[str, tuple]) -> dict[str, tuple]:
    """Drop debug sections and the build-identity sections from a `pe_layout` section map."""
    return {name: values for name, values in sections.items()
            if not is_debug(name, values[3]) and name not in IDENTITY_SECTIONS}


def section_differences(name: str, left: tuple, right: tuple) -> list[str]:
    """One line per difference between two same-named sections' bytes, size, address and flags."""
    lines = []
    if left[0] != right[0]:
        first = next((i for i, (x, y) in enumerate(zip(left[0], right[0])) if x != y), min(len(left[0]), len(right[0])))
        count = sum(x != y for x, y in zip(left[0], right[0])) + abs(len(left[0]) - len(right[0]))
        lines.append(f'{name}: bytes differ ({len(left[0])} vs {len(right[0])} bytes, '
                     f'{count} bytes differ, first at offset {first:#x})')
    for label, index in (('VirtualSize', 1), ('VirtualAddress', 2), ('Characteristics', 3)):
        if left[index] != right[index]:
            lines.append(f'{name}: {label} differs ({left[index]:#x} vs {right[index]:#x})')
    return lines


def differences(a: bytes, b: bytes) -> list[str]:
    """Return one line per load-relevant difference between two executables; empty if identical.

    What is compared and what is not is listed in this module's docstring.
    """
    first, second = pe_layout(a), pe_layout(b)
    left, right = loaded(first['sections']), loaded(second['sections'])
    lines = []
    for name in sorted(left.keys() | right.keys()):
        if name not in right:
            lines.append(f'{name}: only in the first file ({len(left[name][0])} bytes)')
        elif name not in left:
            lines.append(f'{name}: only in the second file ({len(right[name][0])} bytes)')
        else:
            lines += section_differences(name, left[name], right[name])
    for name in IDENTITY_SECTIONS:
        if (name in first['sections']) != (name in second['sections']):
            where = 'first' if name in first['sections'] else 'second'
            lines.append(f'{name}: only in the {where} file')
    for field, value in first['header'].items():
        other = second['header'][field]
        if value != other:
            lines.append(f'{field} differs ({value:#x} vs {other:#x})' if None not in (value, other)
                         else f'{field} differs ({value} vs {other})')
    paired = zip(first['directories'], second['directories'])
    for index, (x, y) in enumerate(paired):
        if index not in IGNORED_DIRECTORIES and x != y:
            lines.append(f'data directory {DIRECTORY_NAMES.get(index, index)} differs '
                         f'(address {x[0]:#x}, size {x[1]:#x} vs address {y[0]:#x}, size {y[1]:#x})')
    if len(first['directories']) != len(second['directories']):
        lines.append('data directory count differs')
    return lines


def identity_differences(a: bytes, b: bytes) -> list[str]:
    """Return the names of build-identity sections (`.buildid`) present in both files that differ."""
    first, second = pe_sections(a), pe_sections(b)
    return [name for name in IDENTITY_SECTIONS if name in first and name in second and first[name] != second[name]]


def export_source(ref: str, destination: Path, root: Path = ROOT) -> None:
    """Write the tracked files of `ref` into `destination`. WORKTREE means the files on disk now:
    tracked files and untracked files Git does not ignore."""
    destination.mkdir(parents=True)
    if ref == WORKTREE:
        listing = subprocess.run(['git', '-C', str(root), 'ls-files', '-z', '--cached', '--others',
                                  '--exclude-standard'], check=True, capture_output=True).stdout
        for relative in filter(None, listing.decode('utf-8').split('\0')):
            source = root/relative
            if not source.is_symlink() and not source.exists():
                continue  # tracked, deleted on disk: the work tree has no such file
            (destination/relative).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination/relative, follow_symlinks=False)
        return
    archive = subprocess.run(['git', '-C', str(root), 'archive', '--format=tar', ref],
                             check=True, capture_output=True).stdout
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        tar.extractall(destination)


def load_runner():
    """Load lp64_env.py, which builds the Docker command line, from beside this file."""
    spec = importlib.util.spec_from_file_location('lp64_env', Path(__file__).with_name('lp64_env.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def build(ref: str, name: str, deadline: int, jobs: int) -> tuple[Path, float]:
    """Export `ref`, build it at the container's project path and return its executable and seconds taken."""
    runner = load_runner()
    export = COMPARE_DIR/name
    log = COMPARE_DIR/f'{name}.log'
    shutil.rmtree(export, ignore_errors=True)
    export_source(ref, export)
    command = ['timeout', str(deadline), 'bash', '-lc', f'export SOURCE_DATE_EPOCH={SOURCE_DATE_EPOCH} && {CONFIGURE} && cmake --build build -j{jobs}']
    argv = runner.docker_command('x86_64', export, None, command)
    print(f'building {ref} ({name}); log: {log}', flush=True)
    started = time.monotonic()
    with log.open('wb') as out:
        status = subprocess.run(argv, stdout=out, stderr=subprocess.STDOUT, timeout=deadline + 120).returncode
    seconds = time.monotonic() - started
    if status != 0:
        reason = f'exceeded its {deadline} s deadline' if status == 124 else f'exited {status}'
        tail = '\n'.join(log.read_text(errors='replace').splitlines()[-LOG_TAIL_LINES:])
        raise RuntimeError(f'building {ref} ({name}) {reason} after {seconds:.0f} s; log {log}; last lines:\n{tail}')
    found = [path for path in (export/'build').glob('*.exe') if path.name in GAME_EXECUTABLES]
    if len(found) != 1:
        raise RuntimeError(f'building {ref} produced no single game executable in {export/"build"}')
    print(f'built {found[0]} in {seconds:.0f} s', flush=True)
    return found[0], seconds


def report(exe_a: Path, exe_b: Path) -> int:
    """Print the comparison of two executables; return 0 if identical, else 1.

    Raises ValueError naming the file when either is not a PE file."""
    data_a, data_b = exe_a.read_bytes(), exe_b.read_bytes()
    for path, data in ((exe_a, data_a), (exe_b, data_b)):
        try:
            pe_layout(data)
        except ValueError as error:
            raise ValueError(f'{path}: {error}') from error
    lines = differences(data_a, data_b)
    if not lines:
        identity = identity_differences(data_a, data_b)
        suffix = f'; {", ".join(identity)} differs' if identity else ''
        print(f'identical in loaded code and data{suffix}')
        return 0
    print('different:')
    for line in lines:
        print('  ' + line)
    return 1


def main(argv: Optional[list[str]] = None) -> int:
    """Run the CLI."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='action', required=True)
    exes = sub.add_parser('compare', help='compare two built executables')
    exes.add_argument('exe_a', type=Path)
    exes.add_argument('exe_b', type=Path)
    commits = sub.add_parser('commits', help='build two refs (or WORKTREE) and compare them')
    commits.add_argument('ref_a')
    commits.add_argument('ref_b')
    commits.add_argument('--deadline', type=int, default=DEFAULT_DEADLINE_SECONDS,
                         help='seconds each build may take (default: %(default)s)')
    commits.add_argument('--jobs', type=int, default=8)
    options = parser.parse_args(argv)
    try:
        if options.action == 'compare':
            return report(options.exe_a, options.exe_b)
        COMPARE_DIR.mkdir(parents=True, exist_ok=True)
        exe_a, _ = build(options.ref_a, 'a', options.deadline, options.jobs)
        exe_b, _ = build(options.ref_b, 'b', options.deadline, options.jobs)
        return report(exe_a, exe_b)
    except (ValueError, RuntimeError, OSError, subprocess.SubprocessError) as error:
        print(f'could not compare: {error}', file=sys.stderr)
        return COULD_NOT_COMPARE


if __name__ == '__main__':
    sys.exit(main())
