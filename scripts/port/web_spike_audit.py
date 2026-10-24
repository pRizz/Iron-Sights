"""Compile-only Wasm survey using the source set selected by the real CMake build."""
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess


# Survey normalization only. Native build commands and sources remain unchanged.
NATIVE_FLAGS = {'-m32', '-msse2', '-mfpmath=sse', '-fno-toplevel-reorder',
                '-malign-data=abi'}
DIAGNOSTIC_COMPATIBILITY = ['-Wno-error=implicit-function-declaration',
                            '-Wno-error=int-conversion',
                            '-Wno-error=incompatible-pointer-types']


def survey_command(entry, destination):
    """Preserve CMake's source/defines/includes; remove documented native flags."""
    tokens = shlex.split(entry['command'])
    command, removed = [tokens[0]], []
    position = 1
    while position < len(tokens):
        token = tokens[position]
        if token == '-o':
            position += 2
            continue
        if token in NATIVE_FLAGS or token.startswith('-Wa,-I'):
            removed.append(token)
        else:
            command.append(token)
        position += 1
    command.extend(DIAGNOSTIC_COMPATIBILITY)
    command.extend(['-o', destination.as_posix()])
    return command, removed


def diagnostics(output):
    """Group exact compiler errors while preserving their full per-file logs."""
    return sorted(set(re.findall(r'(?:fatal )?error: (.+)', output)))


def compile_entry(entry, output, root):
    source = Path(entry['file']).relative_to(root).as_posix()
    token = hashlib.sha256(source.encode()).hexdigest()[:16]
    command, removed = survey_command(entry, output / (token + '.o'))
    result = subprocess.run(command, cwd=entry['directory'], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=180, check=False)
    log = output / (token + '.log')
    log.write_text(shlex.join(command) + '\n' + result.stdout, encoding='utf-8')
    return dict(source=source, returncode=result.returncode,
                removed_flags=removed, errors=diagnostics(result.stdout),
                log='audit/' + log.name)


def run_audit(database, output, root, jobs):
    output.mkdir(parents=True, exist_ok=True)
    entries = json.loads(database.read_text(encoding='utf-8'))
    if not entries:
        raise ValueError('CMake selected no sources for the compile survey')
    results = []
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        futures = {executor.submit(compile_entry, entry, output, root): entry
                   for entry in entries}
        for future in as_completed(futures):
            entry = futures[future]
            try:
                result = future.result()
            except (subprocess.TimeoutExpired, OSError) as error:
                result = dict(source=Path(entry['file']).relative_to(root).as_posix(),
                              returncode=-1, errors=[str(error)], log=None)
            results.append(result)
            if result['returncode']:
                print('AUDIT failed: ' + result['source'], flush=True)
            elif len(results) % 25 == 0:
                print(f'AUDIT {len(results)}/{len(entries)} compiled', flush=True)
    results.sort(key=lambda item: item['source'])
    failures = sum(item['returncode'] != 0 for item in results)
    summary = dict(total=len(results), compiled=len(results)-failures,
                   failed=failures, compatibility_flags=DIAGNOSTIC_COMPATIBILITY,
                   scope='Compile only; no link, ROM, renderer or gameplay proof',
                   sources=results)
    (output.parent / 'audit.json').write_text(
        json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(f'AUDIT summary: {summary["compiled"]}/{summary["total"]} compiled; '
          f'{failures} failed', flush=True)
    return summary
