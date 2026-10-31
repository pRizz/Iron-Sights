"""Inventory tracked buffer-sensitive code; candidates are not confirmed defects.

Run from any directory. Reports stay local under build_win/buffer-audit. Supply
Cppcheck XML to summarize the separately executed compiler-configuration scan.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import os
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
EXTENSIONS = {'.c', '.cpp', '.cc', '.h', '.hpp', '.inl', '.s', '.ts', '.tsx',
              '.js', '.mjs', '.py', '.sh', '.ps1', '.cs', '.glsl', '.html'}
PATTERNS = {
    'unbounded_text': re.compile(r'\b(?:strcpy|strcat|sprintf|vsprintf|gets)\s*\('),
    'memory_copy': re.compile(r'\b(?:memcpy|memmove|memset|bcopy)\s*\('),
    'allocation': re.compile(r'\b(?:malloc|calloc|realloc|alloca|portAllocHigh|dynAllocate\w*|mempAlloc\w*)\s*\('),
    'binary_io': re.compile(r'\b(?:fread|fwrite|read|readFile|writeFile|DataView|Uint\d+Array)\s*\('),
    'fixed_array': re.compile(r'\b\w+\s*\[\s*(?:\d+|[A-Z][A-Z_0-9]*)\s*\]'),
}
MISSIONS = ('dam', 'facility', 'runway', 'surface', 'bunker1', 'silo', 'frigate',
            'surface2', 'bunker2', 'statue', 'archives', 'streets', 'depot',
            'train', 'jungle', 'control', 'caverns', 'cradle', 'aztec', 'egypt')


def domain(name):
    if name.startswith(('port/fast3d/glad/', 'port/fast3d/third_party/')):
        return 'runtime-vendor'
    if name.startswith(('assets/', 'port/generated/')):
        return 'generated-assets'
    if name.startswith('port/fast3d/'):
        return 'renderer-desktop'
    if name.startswith('port/web/'):
        return 'web-native-bridge'
    if name.startswith('port/'):
        return 'platform-fork'
    if name.startswith(('src/libultra/', 'src/libultrare/')):
        return 'inherited-audio-library'
    if name.startswith(('src/', 'include/')):
        return 'inherited-engine'
    if name.startswith('web/src/'):
        return 'web-application'
    return 'tooling-and-other'


def source_record(name, raw):
    text = raw.decode('utf-8', errors='replace')
    sites = []
    for line, value in enumerate(text.splitlines(), 1):
        for kind, pattern in PATTERNS.items():
            if pattern.search(value):
                sites.append({'line': line, 'kind': kind})
    return {'path': name, 'domain': domain(name), 'lines': len(text.splitlines()),
            'sha256': hashlib.sha256(raw).hexdigest(), 'sites': sites}


def checker_report(path):
    errors = ET.parse(path).getroot().findall('./errors/error')
    records = []
    for error in errors:
        record = dict(error.attrib)
        record['locations'] = [dict(location.attrib) for location in error.findall('location')]
        records.append(record)
    return {'diagnostics': records, 'ids': dict(Counter(e['id'] for e in records)),
            'severities': dict(Counter(e['severity'] for e in records))}


def mission_smoke(executable, rom, output, frames, jobs):
    """Headless-only samples; every child owns its executable/runtime directory."""
    executable, rom = executable.resolve(strict=True), rom.resolve(strict=True)
    output.parent.mkdir(parents=True, exist_ok=True)

    def check(stage):
        with tempfile.TemporaryDirectory(prefix='midas-buffer-') as temporary:
            folder = Path(temporary)
            child = folder / executable.name
            shutil.copy2(executable, child)
            env = {key: value for key, value in os.environ.items() if not key.startswith('GE_')}
            env.update(GE_ROM=str(rom), GE_STAGE=stage, GE_STAGE_AFTER='1',
                       GE_FRAMES=str(frames), GE_SEED='1', GE_CHEATS='invuln,invis',
                       GE_NOINISAVE='1', ASAN_OPTIONS='detect_leaks=0')
            log_path = output.parent / (stage + '.log')
            with log_path.open('w', encoding='utf-8') as log:
                result = subprocess.run([str(child)], cwd=folder, env=env, stdout=log,
                                        stderr=subprocess.STDOUT, timeout=120)
            text = log_path.read_text(errors='replace')
            passed = (result.returncode == 0 and 'GE_STAGE direct launch:' in text
                      and f'frame limit reached: frames={frames}' in text
                      and 'runtime error:' not in text and 'ERROR: AddressSanitizer' not in text)
            print(stage + (': PASS' if passed else ': FAIL'), flush=True)
            return dict(stage=stage, exit=result.returncode, passed=passed, log=str(log_path))

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        results = list(pool.map(check, MISSIONS))
    output.write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    return 0 if all(record['passed'] for record in results) else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build_win/buffer-audit/inventory.json')
    parser.add_argument('--cppcheck', type=Path)
    parser.add_argument('--mission-smoke', type=Path, help='An already built headless executable')
    parser.add_argument('--rom', type=Path)
    parser.add_argument('--frames', type=int, default=900)
    parser.add_argument('--jobs', type=int, choices=range(1, 9), default=4)
    options = parser.parse_args()
    if options.mission_smoke:
        if not options.rom or not 600 <= options.frames <= 36000:
            parser.error('--mission-smoke needs --rom and 600..36000 frames')
        return mission_smoke(options.mission_smoke, options.rom, options.output,
                             options.frames, options.jobs)
    names = subprocess.check_output(['git', '-C', str(ROOT), 'ls-files', '-z']).decode().split('\0')
    sources = [source_record(name, (ROOT/name).read_bytes()) for name in names
               if Path(name).suffix.lower() in EXTENSIONS and (ROOT/name).is_file()]
    summary = {}
    for record in sources:
        group = summary.setdefault(record['domain'], {'files': 0, 'lines': 0, 'sites': Counter()})
        group['files'] += 1
        group['lines'] += record['lines']
        group['sites'].update(site['kind'] for site in record['sites'])
    report = {'revision': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD']).decode().strip(),
              'policy': 'Lexical candidates, including comments and tests; no safety verdict is inferred.',
              'domains': summary, 'sources': sources}
    if options.cppcheck:
        report['cppcheck'] = checker_report(options.cppcheck)
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'domains': summary, 'report': str(options.output)}, indent=2))


if __name__ == '__main__':
    raise SystemExit(main())
