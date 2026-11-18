#!/usr/bin/env python3
"""Count what the stock setups' prop records hold where the code reads them untested (LP64-005 Task 3).

Some overlay verdicts rest on what the setups hold: which record an index word names, which models
the records of a type name, what those models hold at a switch slot or under the root. This walks
every setup's prop records (by the file-word sizes in port/generated/setup_kinds.h, the converter's
own table) and prints counts only, never bytes of the ROM.

    python3 scripts/port/lp64_setup_scan.py --rom ROM types [--skeleton door] [--match RE]
    python3 scripts/port/lp64_setup_scan.py --rom ROM models --type DOOR
    python3 scripts/port/lp64_setup_scan.py --rom ROM refs --type MONITOR --word 61 [--attached]
    python3 scripts/port/lp64_setup_scan.py --rom ROM slots --type CCTV --slots 0
    python3 scripts/port/lp64_setup_scan.py --rom ROM path --type DOOR --path child.child [--mask 38:0x40000]

A record's model is its ObjectRecord's `obj`, an index into PitemZ_entries (the include order of
assets/obseg/prop/propItemModelFileRecord.inc.c); only object records (the types
setupCommandGetObject returns) name one. `refs` resolves the command at the record's own index plus
the s32 at --word, as setupGetPtrToCommandByIndex does (NULL off either end); --attached keeps the
records setupSingleMonitor fixes to an owner (pad below zero, not PROPFLAG_INSIDEANOTHEROBJ). `slots` and `path` count
each record once, through its model. Exits 1 when no setup or no record matches, and when a setup or
a model is unreadable (its row names it).
"""
import argparse
import collections
import os
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_model_scan as models_scan  # noqa: E402

ROOT = models_scan.ROOT
END = 48
OBJECT_TYPES = {1, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 17, 20, 21, 36, 39, 40, 41, 42, 43, 45, 47}
KIND_RE = re.compile(r'/\*\s*(\d+)\s+(\w+)\s*\*/\s*\{\s*(\d+),')
INCLUDE_RE = re.compile(r'#include\s+<assets/obseg/prop/(\w+)/propFileRecord\.inc\.c>')
RECORD_RE = re.compile(r'PROPFILERECORD\(\s*(\w+)\s*,')
SKELETON_RE = re.compile(r'MODELFILEHEADER\(\s*(\w+),[^,]*,\s*&SKELETON\((\w+)\)')
UNREADABLE, NULL = models_scan.UNREADABLE, 'NULL'


class Record:
    def __init__(self, setup, index, kind, model):
        self.setup, self.index, self.kind, self.model = setup, index, kind, model


def read_kinds(path):
    """{type number: (name, file words)} from the converter's prop record size table."""
    table = Path(path).read_text().split('kSetupPropdefKinds', 1)[-1].split('};', 1)[0]
    return {int(n): (name, int(words)) for n, name, words in KIND_RE.findall(table)}


def read_prop_names(assets):
    """PitemZ_entries' model names in index order."""
    names = []
    for directory in INCLUDE_RE.findall((Path(assets)/'prop/propItemModelFileRecord.inc.c').read_text()):
        match = RECORD_RE.search((Path(assets)/'prop'/directory/'propFileRecord.inc.c').read_text())
        names.append(match.group(1).lower() if match else directory.lower())
    return names


def read_skeletons(assets):
    skeletons = {}
    for path in sorted(Path(assets).rglob('ModelFileHeader.inc.c')):
        for name, skeleton in SKELETON_RE.findall(path.read_text(encoding='utf-8', errors='replace')):
            skeletons[name.lower()] = skeleton
    return skeletons


def read_setup(data, name, kinds):
    """The setup's records up to END; a last None marks a record whose type has no size."""
    records, at = [], struct.unpack_from('>I', data, 12)[0]
    while at + 4 <= len(data):
        kind = data[at + 3]
        if kind == END:
            return records
        if kind not in kinds:
            break
        model = struct.unpack_from('>H', data, at + 4)[0] if kind in OBJECT_TYPES and kinds[kind][1] > 1 else None
        records.append(Record(name, len(records), kind, model))
        records[-1].at = at
        at += 4 * kinds[kind][1]
    records.append(None)
    return records


def load_setups(rom, filelist, match, kinds):
    setups = []
    for offset, size, path in models_scan.read_filelist(filelist):
        if '/setup/' in path and re.search(match, path):
            data = models_scan.inflate(rom[offset:offset + size])
            setups.append((os.path.basename(path), data, read_setup(data, os.path.basename(path), kinds)))
    return setups


def attached(data, rec):
    """setupSingleMonitor's test: pad below zero and not inside another object (PROPFLAG_INSIDEANOTHEROBJ)."""
    pad, flags = struct.unpack_from('>hI', data, rec.at + 6)
    return pad < 0 and not flags & 0x8000


def masked(data, rec, mask, kinds):
    """True when the record's big-endian word WORD has a bit of MASK set (`WORD:MASK`)."""
    word, _, bits = mask.partition(':')
    word = int(word, 0)
    return word < kinds[rec.kind][1] and struct.unpack_from('>I', data, rec.at + 4 * word)[0] & int(bits, 0) != 0


def type_name(kinds, kind):
    return kinds[kind][0] if kind in kinds else str(kind)


def count(args, rom, kinds, setups):
    """The counts the command asks for, keyed by tuples."""
    counts = collections.Counter()
    names, skeletons = read_prop_names(args.assets), read_skeletons(args.assets)
    prop_models = {m.name: m for m in models_scan.load_models(rom, args.filelist, args.assets) if m.cls == 'prop'}
    wanted = None if getattr(args, 'type', None) is None else {k for k, v in kinds.items() if v[0] == args.type}
    for setup, data, records in setups:
        if records and records[-1] is None:
            counts[('%s (%s)' % (UNREADABLE, setup),)] += 1
            records = records[:-1]
        for rec in records:
            if wanted is not None and rec.kind not in wanted:
                continue
            if args.mask and not masked(data, rec, args.mask, kinds):
                continue
            name = names[rec.model] if rec.model is not None and rec.model < len(names) else None
            if args.command == 'types':
                if args.skeleton is None or (name and skeletons.get(name) == args.skeleton):
                    counts[(type_name(kinds, rec.kind),)] += 1
                continue
            if args.command == 'refs':
                if args.attached and not attached(data, rec):
                    continue
                target = rec.index + struct.unpack_from('>i', data, rec.at + 4 * args.word)[0]
                counts[(type_name(kinds, records[target].kind) if 0 <= target < len(records) else NULL,)] += 1
                continue
            if rec.model is None:
                continue
            if name is None or name not in prop_models:
                counts[('%s (model %d)' % (UNREADABLE, rec.model),)] += 1
            elif args.command == 'models':
                counts[(name, skeletons.get(name, '?'))] += 1
            else:
                counts.update(model_counts(args, prop_models[name]))
    return counts


def model_counts(args, model):
    if args.command == 'slots':
        return {key[1:]: n for key, n in models_scan.count_slots([model], models_scan.parse_slots(args.slots)).items()}
    try:
        node = model.root
        for step in args.path.split('.'):
            node = model.ref(node + {'child': 0x14, 'next': 0x0c}[step]) if node is not None else None
        return {(models_scan.EMPTY if node is None else models_scan.opcode_name(model.opcode(node)),): 1}
    except models_scan.ScanError:
        return {(models_scan.unreadable(model),): 1}


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--rom', default=os.environ.get('GE_ROM'), help='the US ROM (default: GE_ROM)')
    parser.add_argument('--filelist', default=str(ROOT/'scripts/filelist.u.csv'))
    parser.add_argument('--assets', default=str(ROOT/'assets/obseg'))
    parser.add_argument('--kinds', default=str(ROOT/'port/generated/setup_kinds.h'))
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('types', 'models', 'refs', 'slots', 'path'):
        command = sub.add_parser(name)
        command.add_argument('--match', default='/setup/', help='regular expression a setup path must match')
        command.add_argument('--mask', metavar='WORD:MASK',
                             help='only records whose file word WORD (as the ROM holds it) has a bit of MASK set')
        if name == 'types':
            command.add_argument('--skeleton', help='only records whose model has this skeleton')
        else:
            command.add_argument('--type', required=True, help='record type name, e.g. DOOR')
        if name == 'refs':
            command.add_argument('--word', type=int, required=True, help='the s32 word holding an index offset')
            command.add_argument('--attached', action='store_true',
                                 help='only records with a negative pad outside another object')
        if name == 'slots':
            command.add_argument('--slots', required=True, help='slot numbers, e.g. 0,3-7')
        if name == 'path':
            command.add_argument('--path', required=True, help='child/next steps from the root, e.g. child.child')
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    if not args.rom or not Path(args.rom).is_file():
        print('lp64_setup_scan: no ROM (pass --rom or set GE_ROM)', file=sys.stderr)
        return 1
    rom, kinds = Path(args.rom).read_bytes(), read_kinds(args.kinds)
    setups = load_setups(rom, args.filelist, args.match, kinds)
    if not setups:
        print('lp64_setup_scan: no setup matches %r' % args.match, file=sys.stderr)
        return 1
    counts = count(args, rom, kinds, setups)
    found = {k: v for k, v in counts.items() if not str(k[-1]).startswith(UNREADABLE)}
    print('setups: %d' % len(setups))
    models_scan.print_counts(counts)
    if not found:
        print('lp64_setup_scan: no record matches (check --type and the filters)', file=sys.stderr)
        return 1
    return 0 if models_scan.report_unreadable(counts) else 1


if __name__ == '__main__':
    sys.exit(main())
