"""Inventory tracked buffer-sensitive code; candidates are not confirmed defects.

Run from any directory. Reports stay local under build_win/buffer-audit. Supply
Cppcheck XML to summarize the separately executed compiler-configuration scan.
--leaks turns on LeakSanitizer with the checked-in lsan_suppressions.txt.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import os
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
LSAN_SUPPRESSIONS = ROOT/'scripts/port/lsan_suppressions.txt'
SANITIZER_ERRORS = ('runtime error:', 'ERROR: AddressSanitizer', 'ERROR: LeakSanitizer')
# A pool too small at 64 bits, a converter's refusal (LP64-004) or any other `ge: lp64:` line outside
# the boot check's informational list fails a mission whatever its exit status. The markers, the list
# and the rule are the boot check's own (lp64_boot.py), loaded by path as lp64_oracle.py loads this
# file, so the two tools cannot drift apart. A 32-bit game prints none of these lines; the oracle
# build's `ge: lp64: oracle:` lines are informational here and judged by lp64_oracle.py's totals.
_lp64_boot_spec = importlib.util.spec_from_file_location('lp64_boot', Path(__file__).with_name('lp64_boot.py'))
lp64_boot = importlib.util.module_from_spec(_lp64_boot_spec)
_lp64_boot_spec.loader.exec_module(lp64_boot)
# The mission smoke's one executable copy (run_copies.py), loaded by path for the same reason.
_run_copies_spec = importlib.util.spec_from_file_location('run_copies', Path(__file__).with_name('run_copies.py'))
_run_copies = importlib.util.module_from_spec(_run_copies_spec)
_run_copies_spec.loader.exec_module(_run_copies)
SharedExecutable = _run_copies.SharedExecutable
REFUSAL_MARKERS = lp64_boot.REFUSAL_MARKERS


def sanitizer_env(leaks):
    """detect_leaks=0 keeps the original buffer-only samples; leaks adds LSan."""
    if not leaks:
        return dict(ASAN_OPTIONS='detect_leaks=0')
    return dict(ASAN_OPTIONS='detect_leaks=1',
                LSAN_OPTIONS=f'suppressions={LSAN_SUPPRESSIONS}:print_suppressions=1')


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


def mission_smoke(executable, rom, output, frames, jobs, leaks=False, fixed_tick=False):
    """Headless-only samples; every child owns its executable/runtime directory. FIXED_TICK runs one
    game field a frame with the 64-bit census lines, for lp64_census.py."""
    executable, rom = executable.resolve(strict=True), rom.resolve(strict=True)
    output.parent.mkdir(parents=True, exist_ok=True)

    def check(stage):
        with tempfile.TemporaryDirectory(prefix='midas-buffer-') as temporary:
            folder = Path(temporary)
            child = shared.place(folder)
            env = {key: value for key, value in os.environ.items() if not key.startswith('GE_')}
            env.update(GE_ROM=str(rom), GE_STAGE=stage, GE_STAGE_AFTER='1',
                       GE_FRAMES=str(frames), GE_SEED='1', GE_CHEATS='invuln,invis',
                       GE_NOINISAVE='1', **sanitizer_env(leaks))
            if fixed_tick:
                # The same game fields at any speed: an emulated 32-bit run and a native 64-bit one,
                # compared by lp64_census.py, otherwise reach different points in their frames. The
                # census lines are a 64-bit build's (a 32-bit build prints none). Without the flag a
                # slow or long frame advances several fields, so a run covers more game time.
                env.update(GE_FIXEDTICK='1', GE_LP64_CENSUS='1')
            log_path = output.parent / (stage + '.log')
            with log_path.open('w', encoding='utf-8') as log:
                result = subprocess.run([str(child)], cwd=folder, env=env, stdout=log,
                                        stderr=subprocess.STDOUT, timeout=300 if leaks else 120)
            text = log_path.read_text(errors='replace')
            refusal = next(iter(lp64_boot.failing_lp64_lines(text.splitlines())), None)
            passed = (result.returncode == 0 and 'GE_STAGE direct launch:' in text
                      and f'frame limit reached: frames={frames}' in text
                      and not any(error in text for error in SANITIZER_ERRORS)
                      and refusal is None)
            verdict = ': PASS' if passed else ': FAIL' + (f' ({refusal})' if refusal else '')
            print(stage + verdict, flush=True)
            return dict(stage=stage, exit=result.returncode, passed=passed, refusal=refusal, log=str(log_path))

    # One copy made before any mission starts, linked into each mission's folder (run_copies.py): a copy
    # made per mission inside the pool raced with the other missions' starts (Text file busy).
    with SharedExecutable(executable) as shared, ThreadPoolExecutor(max_workers=jobs) as pool:
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
    parser.add_argument('--leaks', action='store_true', help='Mission smoke with LeakSanitizer enabled')
    parser.add_argument('--fixed-tick', action='store_true',
                        help='Mission smoke at one game field a frame with the 64-bit census lines '
                             '(GE_FIXEDTICK=1, GE_LP64_CENSUS=1), for lp64_census.py')
    options = parser.parse_args()
    if options.mission_smoke:
        if not options.rom or not 600 <= options.frames <= 36000:
            parser.error('--mission-smoke needs --rom and 600..36000 frames')
        return mission_smoke(options.mission_smoke, options.rom, options.output,
                             options.frames, options.jobs, options.leaks, options.fixed_tick)
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
