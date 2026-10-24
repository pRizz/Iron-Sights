#!/usr/bin/env python3
"""Build the real CMake web target with the pinned SDK; optionally smoke a local ROM."""
import argparse
from pathlib import Path
import subprocess
import shutil
import sys

from web_spike import ROOT, load_toolchain, run_logged


def inside(options):
    output = Path('/out')
    video = options.mode == 'video'
    run_logged(['emcmake', 'cmake', '-S', str(ROOT), '-B', str(output),
                '-DCMAKE_BUILD_TYPE=Release', '-DGE_ROMDATA_FALLBACK=OFF',
                '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                '-DGE_WEB_LINK_OPT=' + str(options.link_opt),
                '-DGE_VIDEO=' + ('ON' if video else 'OFF'),
                '-DGE_HEADLESS=' + ('OFF' if video else 'ON')], output / 'configure.log')
    build_command = ['cmake', '--build', str(output), '--parallel', str(options.jobs)]
    if options.keep_going:
        build_command.extend(['--', '-k'])
    run_logged(build_command, output / 'build.log', timeout=900)
    if video:
        shutil.copyfile(ROOT / 'scripts/port/web_probe/play.html', output / 'index.html')
    if options.smoke:
        run_logged(['node', str(ROOT / 'scripts/port/web_core_smoke.mjs'),
                    str(output / 'ProjectMidas.mjs'), '/rom.z64', str(options.frames)],
                   output / 'smoke.log')
    return 0


def outside(options):
    config = load_toolchain()
    image = config['image']
    cache = f"midas-emsdk-{config['sdk_version']}-{image.rsplit('sha256:', 1)[1][:12]}"
    output = ROOT / 'build/web' / options.mode
    output.mkdir(parents=True, exist_ok=True)
    inspected = subprocess.run(['docker', 'image', 'inspect', image],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if inspected.returncode:
        run_logged(['docker', 'pull', image], output / 'sdk-pull.log')
    command = ['docker', 'run', '--rm', '-e', 'BINARYEN_CORES=4',
               '--network=' + ('bridge' if options.mode == 'video' else 'none'),
               '--mount', f'type=bind,source={ROOT},target=/src,readonly',
               '--mount', f'type=bind,source={output},target=/out',
               '--mount', f'type=volume,source={cache},target=/emsdk/upstream/emscripten/cache',
               '-w', '/src']
    if options.smoke:
        rom = Path(options.rom).resolve()
        if not rom.is_file():
            raise ValueError('The local smoke ROM does not exist')
        command.extend(['--mount', f'type=bind,source={rom},target=/rom.z64,readonly'])
    command.extend([image, 'python3', 'scripts/port/web_build.py', options.mode,
                    '--inside', '--jobs', str(options.jobs), '--frames', str(options.frames),
                    '--link-opt', str(options.link_opt)])
    if options.smoke:
        command.extend(['--smoke', '--rom', '/rom.z64'])
    if options.keep_going:
        command.append('--keep-going')
    print('Web build outputs: ' + str(output), flush=True)
    with (output / 'run.log').open('w', encoding='utf-8') as capture:
        process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, encoding='utf-8')
        for line in process.stdout:
            print(line, end='', flush=True)
            capture.write(line)
            capture.flush()
        return process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('core', 'video'))
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--link-opt', type=int, choices=range(4), default=2,
                        help='Use 0 for fast runtime debugging; release defaults to 2')
    parser.add_argument('--smoke', action='store_true')
    parser.add_argument('--keep-going', action='store_true', help='Collect independent Make build failures')
    parser.add_argument('--rom')
    parser.add_argument('--frames', type=int, default=360)
    parser.add_argument('--inside', action='store_true', help=argparse.SUPPRESS)
    options = parser.parse_args()
    if not 1 <= options.jobs <= 32 or not 1 <= options.frames <= 36000:
        parser.error('Use 1..32 jobs and 1..36000 smoke frames')
    if options.smoke and (not options.rom or options.mode != 'core'):
        parser.error('--smoke requires core mode and --rom pointing to your local ROM')
    try:
        return inside(options) if options.inside else outside(options)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print('ERROR ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
