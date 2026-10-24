#!/usr/bin/env python3
"""Recover missing Wasm C declarations from Clang definitions, never inferred casts."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
from pathlib import Path
import re
import shlex
import subprocess

# Toolchain-library definitions are declared explicitly by web/compat.h.
HOST_FUNCTIONS = {'atof', 'fabsf', 'tanf'}

def mismatch_owners(log):
    owners = {}
    maybe_name = None
    for line in log.splitlines():
        match = re.search(r'function signature mismatch: (\w+)', line)
        if match:
            maybe_name = match[1]
            owners.setdefault(maybe_name, [])
            continue
        match = re.search(r'in CMakeFiles/ge\.dir/(.+\.c)\.o', line)
        if maybe_name and match and match[1] not in owners[maybe_name]:
            owners[maybe_name].append(match[1])
    return owners


def json_records(text):
    decoder = json.JSONDecoder()
    position = 0
    while position < len(text):
        if text[position].isspace():
            position += 1
            continue
        record, position = decoder.raw_decode(text, position)
        yield record


def definition_type(entry, name):
    tokens = shlex.split(entry['command'])
    command = []
    position = 0
    while position < len(tokens):
        if tokens[position] == '-o':
            position += 2
            continue
        if tokens[position] != '-c':
            command.append(tokens[position])
        position += 1
    command.extend(['-DGE_WEB_PROTOTYPE_SCAN=1', '-fsyntax-only', '-Xclang', '-ast-dump=json',
                    '-Xclang', '-ast-dump-filter=' + name])
    result = subprocess.run(command, cwd=entry['directory'], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=120, check=True)
    for record in json_records(result.stdout):
        if record.get('kind') != 'FunctionDecl' or record.get('name') != name:
            continue
        if any(child.get('kind') == 'CompoundStmt' for child in record.get('inner', [])):
            return record['type']['qualType']
    return None


def recover(name, sources, entries):
    for source in sources:
        maybe_entry = entries.get('/src/' + source)
        if maybe_entry is None:
            continue
        maybe_type = definition_type(maybe_entry, name)
        if maybe_type is not None:
            return name, maybe_type, source
    raise ValueError('No authoritative definition for ' + name)


def declaration(name, signature):
    match = re.fullmatch(r'(.+?) \((.*)\)', signature)
    if not match:
        raise ValueError('Unsupported function declaration: ' + name + ' ' + signature)
    arguments = match[2] or 'void'
    return f'{match[1]} {name}({arguments});'


def generate(database, log, output, jobs, types_file=None):
    entries = {entry['file']: entry for entry in json.loads(database.read_text())}
    if types_file:
        owners = {name: [source] for name, signature, source in json.loads(types_file.read_text())}
    else:
        owners = mismatch_owners(log.read_text())
    owners = {name: sources for name, sources in owners.items() if name not in HOST_FUNCTIONS}
    if not owners:
        raise ValueError('No signature failures found in the supplied linker log')
    recovered = []
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        futures = [executor.submit(recover, name, sources, entries)
                   for name, sources in owners.items()]
        for future in as_completed(futures):
            recovered.append(future.result())
            if len(recovered) % 25 == 0:
                print(f'Recovered {len(recovered)}/{len(owners)} definitions', flush=True)
    write_header(recovered, output)


def write_header(recovered, output):
    recovered.sort()
    lines = ['/* Generated from real Clang definitions by scripts/port/web_prototypes.py. */',
             '#ifndef MIDAS_WEB_PROTOTYPES_H', '#define MIDAS_WEB_PROTOTYPES_H',
             '#include <bondtypes.h>', '#include <bondconstants.h>',
             'typedef struct textoverride textoverride;',
             'typedef struct save_data save_data;', '']
    struct_names = set(re.findall(r'\bstruct (\w+)', ' '.join(item[1] for item in recovered)))
    lines.extend('struct ' + name + ';' for name in sorted(struct_names))
    for name, signature, source in recovered:
        lines.extend(['', '/* ' + source + ' */', declaration(name, signature)])
    lines.extend(['', '#endif', ''])
    output.write_text('\n'.join(lines), encoding='utf-8')
    output.with_suffix('.json').write_text(json.dumps(recovered, indent=2) + '\n')
    print(f'Wrote {len(recovered)} authoritative declarations to {output}', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database', type=Path)
    parser.add_argument('--log', type=Path)
    parser.add_argument('--types-file', type=Path, help='Re-render a previously recovered AST type report')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=4)
    options = parser.parse_args()
    if options.types_file and options.database:
        generate(options.database, None, options.output, options.jobs, options.types_file)
    elif options.types_file:
        write_header(json.loads(options.types_file.read_text()), options.output)
    elif options.database and options.log:
        generate(options.database, options.log, options.output, options.jobs)
    else:
        parser.error('Use --types-file or both --database and --log')
