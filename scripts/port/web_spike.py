#!/usr/bin/env python3
"""Run the pinned, ROM-free Wasm probe and game-source survey in Docker."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

from web_spike_audit import run_audit

ROOT = Path(__file__).resolve().parents[2]
TOOLCHAIN = ROOT / 'scripts/port/web_toolchain.json'


def load_toolchain(path=TOOLCHAIN):
    config = json.loads(path.read_text(encoding='utf-8'))
    version = config['sdk_version']
    image = config['image']
    if config['schema'] != 1 or not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Invalid web SDK configuration')
    expected = r'emscripten/emsdk:' + re.escape(version) + r'@sha256:[0-9a-f]{64}'
    if not re.fullmatch(expected, image):
        raise ValueError('Web SDK image must match its version and immutable digest')
    return config


def run_logged(command, log, expected_failure=False, timeout=300):
    print('RUN ' + shlex.join(map(str, command)), flush=True)
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=timeout, check=False)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(shlex.join(map(str, command)) + '\n' + result.stdout,
                   encoding='utf-8')
    print(result.stdout[-5000:], end='', flush=True)
    if result.returncode and not expected_failure:
        raise RuntimeError(f'Command failed ({result.returncode}); see {log}')
    return result


def probe(output, jobs):
    build = output / 'probe'
    run_logged(['emcmake', 'cmake', '-S', str(ROOT / 'scripts/port/web_probe'),
                '-B', str(build), '-DCMAKE_BUILD_TYPE=Release'], output / 'probe-configure.log')
    run_logged(['cmake', '--build', str(build), '--parallel', str(jobs)],
               output / 'probe-build.log')
    results = []
    for layout in ('compact', 'high'):
        result = run_logged(['node', str(build / f'probe_{layout}.js')],
                            output / f'probe-{layout}.log')
        results.append(json.loads(result.stdout.strip().splitlines()[-1]))
    return results


def audit(output, jobs):
    build = output / 'core'
    run_logged(['emcmake', 'cmake', '-S', str(ROOT), '-B', str(build),
                '-DGE_HEADLESS=ON', '-DGE_VIDEO=OFF', '-DGE_ROMDATA_FALLBACK=OFF',
                '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DGE_WEB_LINK_OPT=0'],
               output / 'core-configure.log')
    # Generate the header without attempting the native-only executable overlay.
    run_logged(['cmake', '--build', str(build), '--target', 'ge_buildinfo'],
               output / 'core-buildinfo.log')
    baseline = run_logged(['cmake', '--build', str(build), '--parallel', str(jobs)],
                          output / 'core-baseline.log', expected_failure=True)
    result = run_audit(build / 'compile_commands.json', output / 'audit', ROOT, jobs)
    result['root_cmake_build_returncode'] = baseline.returncode
    return result


def inside(options, config):
    output = Path('/out')
    version = run_logged(['emcc', '--version'], output / 'sdk-version.log').stdout
    if not re.search(r'\b' + re.escape(config['sdk_version']) + r'\b', version):
        raise RuntimeError('Running Emscripten version does not match the pin')
    summary = dict(schema=1, sdk_version=config['sdk_version'], image=config['image'])
    result = 0
    try:
        if options.mode in ('probe', 'all'):
            summary['probe'] = probe(output, options.jobs)
        if options.mode in ('audit', 'all'):
            result_audit = audit(output, options.jobs)
            summary['audit'] = {key: value for key, value in result_audit.items()
                                if key != 'sources'}
            result = 1 if result_audit['failed'] else 0
    except (RuntimeError, subprocess.SubprocessError, OSError, ValueError) as error:
        summary['error'] = str(error)
        print('ERROR ' + str(error), flush=True)
        result = 2
    summary['exit_code'] = result
    (output / f'summary-{options.mode}.json').write_text(
        json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    return result


def outside(options, config):
    output = ROOT / 'build/web-spike'
    output.mkdir(parents=True, exist_ok=True)
    image = config['image']
    maybe_image = subprocess.run(['docker', 'image', 'inspect', image],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if maybe_image.returncode:
        run_logged(['docker', 'pull', image], output / 'sdk-pull.log')
    command = ['docker', 'run', '--rm', '--network=none',
               '--mount', f'type=bind,source={ROOT},target=/src,readonly',
               '--mount', f'type=bind,source={output},target=/out',
               '-w', '/src', image, 'python3', 'scripts/port/web_spike.py',
               options.mode, '--inside', '--jobs', str(options.jobs)]
    log = output / f'run-{options.mode}.log'
    print('ROM-free web spike; outputs: ' + str(output), flush=True)
    # Stream progress while retaining the complete combined log on the host.
    with log.open('w', encoding='utf-8') as capture:
        process = subprocess.Popen(command, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, encoding='utf-8')
        for line in process.stdout:
            print(line, end='', flush=True)
            capture.write(line)
            capture.flush()
        return process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('probe', 'audit', 'all'), nargs='?', default='all')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--inside', action='store_true', help=argparse.SUPPRESS)
    options = parser.parse_args()
    if options.jobs < 1 or options.jobs > 32:
        parser.error('--jobs must be between 1 and 32')
    try:
        config = load_toolchain()
        return inside(options, config) if options.inside else outside(options, config)
    except (RuntimeError, subprocess.SubprocessError, OSError, ValueError) as error:
        print('ERROR ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
