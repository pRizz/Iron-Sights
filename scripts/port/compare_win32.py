#!/usr/bin/env python3
"""Prove that a change leaves the 32-bit Windows executable unchanged.

`commits REF_A REF_B` exports both source states, builds each with the README's
Windows configuration in the `goldeneye` container (mounted at the same path both
times, because the compiler embeds source paths), and compares the bytes the
loader maps. `compare EXE_A EXE_B` compares two existing executables. Debug
sections and the header's timestamp are ignored. Either ref may be `WORKTREE`:
the tracked files as they are on disk now, uncommitted edits included.

Exit status: 0 identical, 1 different. Build logs are kept in build/lp64/compare.
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
GAME_EXECUTABLES = ('IronSights.exe', 'ProjectMidas.exe')
DEFAULT_DEADLINE_SECONDS = 180  # a build took 55 s here, under emulation
LOG_TAIL_LINES = 30


def pe_sections(data: bytes) -> dict[str, bytes]:
    """Return each section's name mapped to its raw bytes; ValueError if `data` is not a PE file."""
    if len(data) < 0x40 or data[:2] != b'MZ':
        raise ValueError('not a PE file: no DOS header')
    (pe_offset,) = struct.unpack_from('<I', data, 0x3C)
    if data[pe_offset:pe_offset + 4] != b'PE\0\0':
        raise ValueError('not a PE file: no PE signature')
    coff = pe_offset + 4
    if len(data) < coff + 20:
        raise ValueError('truncated PE file: no COFF header')
    count, = struct.unpack_from('<H', data, coff + 2)
    optional_size, = struct.unpack_from('<H', data, coff + 16)
    table = coff + 20 + optional_size
    if len(data) < table + 40 * count:
        raise ValueError('truncated PE file: section table')
    sections = {}
    for index in range(count):
        entry = table + 40 * index
        name = data[entry:entry + 8].rstrip(b'\0').decode('latin-1')
        size, start = struct.unpack_from('<II', data, entry + 16)
        if start + size > len(data):
            raise ValueError(f'truncated PE file: section {name}')
        sections[name] = data[start:start + size]
    return sections


def loaded(sections: dict[str, bytes]) -> dict[str, bytes]:
    """Drop debug sections (`.debug*` and the `/N` long names that hold them); keep the rest."""
    return {name: data for name, data in sections.items() if not name.startswith(('.debug', '/'))}


def differences(a: bytes, b: bytes) -> list[str]:
    """Return one line per loaded section that differs, or exists in only one file; empty if identical."""
    left, right = loaded(pe_sections(a)), loaded(pe_sections(b))
    lines = []
    for name in sorted(left.keys() | right.keys()):
        if name not in right:
            lines.append(f'{name}: only in the first file ({len(left[name])} bytes)')
        elif name not in left:
            lines.append(f'{name}: only in the second file ({len(right[name])} bytes)')
        elif left[name] != right[name]:
            first = next((i for i, (x, y) in enumerate(zip(left[name], right[name])) if x != y),
                         min(len(left[name]), len(right[name])))
            count = sum(x != y for x, y in zip(left[name], right[name])) + abs(len(left[name]) - len(right[name]))
            lines.append(f'{name}: differs ({len(left[name])} vs {len(right[name])} bytes, '
                         f'{count} bytes differ, first at offset {first:#x})')
    return lines


def export_source(ref: str, destination: Path, root: Path = ROOT) -> None:
    """Write the tracked files of `ref` into `destination`; WORKTREE means the files on disk now."""
    destination.mkdir(parents=True)
    if ref == WORKTREE:
        listing = subprocess.run(['git', '-C', str(root), 'ls-files', '-z'],
                                 check=True, capture_output=True).stdout
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
    """Print the comparison of two executables; return 0 if identical, else 1."""
    lines = differences(exe_a.read_bytes(), exe_b.read_bytes())
    if not lines:
        print('identical: every loaded section matches (debug sections and the timestamp ignored)')
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
    if options.action == 'compare':
        return report(options.exe_a, options.exe_b)
    COMPARE_DIR.mkdir(parents=True, exist_ok=True)
    exe_a, _ = build(options.ref_a, 'a', options.deadline, options.jobs)
    exe_b, _ = build(options.ref_b, 'b', options.deadline, options.jobs)
    return report(exe_a, exe_b)


if __name__ == '__main__':
    sys.exit(main())
