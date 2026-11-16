#!/usr/bin/env python3
"""Build and run the game's toolchain containers for i686, x86_64 and aarch64.

`build-images` builds both images; `run --arch ARCH -- COMMAND...` runs a
command in the right one with the checkout and the ROM mounted. Docker is
always called with an argument list, never through a shell.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
from typing import Mapping, Optional

ROOT = Path(__file__).resolve().parents[2]
ARCHES = ('i686', 'x86_64', 'aarch64')
PROJECT_DIR = '/home/dev/project'
ROM_MOUNT = '/rom/baserom.u.z64'
AMD64_IMAGE = 'goldeneye'
ARM64_IMAGE = 'midas-lp64-arm64'
ARM64_DOCKERFILE = Path('scripts/port/lp64-arm64.Dockerfile')


def image_and_platform(arch: str) -> tuple[str, str]:
    """Return the Docker image name and platform that run `arch`."""
    if arch in ('i686', 'x86_64'):
        return AMD64_IMAGE, 'linux/amd64'
    if arch == 'aarch64':
        return ARM64_IMAGE, 'linux/arm64'
    raise ValueError(f'unknown architecture {arch!r}; expected one of {ARCHES}')


def find_rom(root: Path, environ: Mapping[str, str]) -> Optional[Path]:
    """Return GE_ROM as an absolute path if set, else the only *.z64 in `root`, else None.

    Raises ValueError when GE_ROM is not an existing file (Docker would otherwise mount a relative
    path as a named volume and create a mistyped absolute one as a directory), or when `root` holds
    more than one *.z64 and GE_ROM is unset.
    """
    explicit = environ.get('GE_ROM')
    if explicit:
        rom = Path(explicit).resolve()
        if not rom.is_file():
            raise ValueError(f'GE_ROM is not an existing file: {rom}')
        return rom
    candidates = sorted(root.glob('*.z64'))
    if len(candidates) > 1:
        names = ', '.join(path.name for path in candidates)
        raise ValueError(f'more than one *.z64 in {root} ({names}); set GE_ROM')
    return candidates[0] if candidates else None


def docker_command(arch: str, root: Path, maybe_rom: Optional[Path], command: list[str]) -> list[str]:
    """Build the `docker run` argument list for `command` on `arch`."""
    image, platform = image_and_platform(arch)
    argv = ['docker', 'run', '--rm', '--platform', platform,
            '-v', f'{root}:{PROJECT_DIR}', '-w', PROJECT_DIR]
    if maybe_rom is not None:
        argv += ['-v', f'{maybe_rom}:{ROM_MOUNT}:ro', '-e', f'GE_ROM={ROM_MOUNT}']
    return argv + [image] + command


def build_commands(root: Path, uid: int, gid: int) -> list[list[str]]:
    """Return the two `docker build` argument lists for the toolchain images."""
    ids = ['--build-arg', f'HOST_UID={uid}', '--build-arg', f'HOST_GID={gid}']
    return [
        ['docker', 'build', '--platform', 'linux/amd64', *ids, '-t', AMD64_IMAGE, str(root)],
        ['docker', 'build', '--platform', 'linux/arm64', *ids, '-t', ARM64_IMAGE,
         '-f', str(root/ARM64_DOCKERFILE), str(root)],
    ]


def main(argv: Optional[list[str]] = None) -> int:
    """Run the CLI and return the exit status of the last Docker call."""
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    sub.add_parser('build-images', help='Build the goldeneye (amd64) and midas-lp64-arm64 images')
    run = sub.add_parser('run', help='Run a command in the container for an architecture')
    run.add_argument('--arch', choices=ARCHES, required=True)
    run.add_argument('command', nargs=argparse.REMAINDER, help='after --')
    options = parser.parse_args(argv)

    if options.action == 'build-images':
        for command in build_commands(ROOT, os.getuid(), os.getgid()):
            status = subprocess.run(command).returncode
            if status:
                return status
        return 0

    command = options.command[1:] if options.command[:1] == ['--'] else options.command
    if not command:
        parser.error('run needs a command after --')
    try:
        maybe_rom = find_rom(ROOT, os.environ)
    except ValueError as error:
        print(f'error: {error}', file=sys.stderr)
        return 2
    return subprocess.run(docker_command(options.arch, ROOT, maybe_rom, command)).returncode


if __name__ == '__main__':
    sys.exit(main())
