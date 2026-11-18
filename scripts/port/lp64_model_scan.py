#!/usr/bin/env python3
"""Count what the stock ROM's models and setups hold where the code reads them untested (LP64-005 Task 3).

Some overlay verdicts rest on data, not on a guard in the code: a switch slot's node read as a
position, a root read as a header, a collision link read as a DLCOLLISION node. This reads the
ROM's chr, gun and prop model files and the setups and prints counts only (opcode numbers and
names, per model class and slot), never bytes of the ROM.

    python3 scripts/port/lp64_model_scan.py --rom ROM roots [--class chr] [--match RE] [--exclude RE]
    python3 scripts/port/lp64_model_scan.py --rom ROM slots --slots 0,3-7 [--class gun] [--match RE]
    python3 scripts/port/lp64_model_scan.py --rom ROM shadows | links
    python3 scripts/port/lp64_model_scan.py --rom ROM intros [--match mp_setup]

The ROM path is --rom or GE_ROM. A model's switch slot count and texture count are not in its
file; they come from the MODELFILEHEADER lines under assets/obseg. Exits 1 when the scan finds no
model (or no setup), so a wrong path or filter cannot print zeros that read as "none".
"""
import argparse
import collections
import os
from pathlib import Path
import re
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
OPCODES = ['NULL', 'HEADER', 'GROUP', 'OP03', 'DL', 'OP05', 'OP06', 'OP07', 'LOD', 'BSP', 'BBOX', 'OP11',
           'GUNFIRE', 'SHADOW', 'OP14', 'INTERLINK', 'OP16', 'OP17', 'SWITCH', 'OP19', 'OP20', 'GROUPSIMPLE',
           'DLPRIMARY', 'HEAD', 'DLCOLLISION']
SHADOW, DLCOLLISION = 13, 24
NODE_SIZE, TEXTURE_SIZE, VERTEX_SIZE = 24, 12, 16
INTRO_WORDS = {0: 3, 1: 4, 2: 4, 3: 8, 4: 2, 5: 2, 6: 10, 7: 3, 8: 2}
INTRO_NAMES = ['SPAWN', 'ITEM', 'AMMO', 'SWIRL', 'ANIM', 'CUFF', 'CAMERA', 'WATCH', 'CREDITS', 'END']
HEADER_RE = re.compile(r'MODELFILEHEADER\((\w+),[^,]*,[^,]*,[^,]*,\s*(\w+),\s*\w+,\s*[-\d.e]+,\s*\w+,\s*(\w+)\)')
EMPTY, UNREADABLE = 'empty', 'unreadable'


class ScanError(Exception):
    """The file is not shaped as the walk expects: a reference outside it."""


class Model:
    """One model file, inflated, with the header counts the game's code supplies."""

    def __init__(self, cls, name, data, num_switches, num_textures):
        self.cls = cls
        self.name = name
        self.data = data
        self.num_switches = num_switches
        self.root = num_switches * 4 + num_textures * TEXTURE_SIZE

    def u16(self, at):
        if at < 0 or at + 2 > len(self.data):
            raise ScanError('read past the file')
        return struct.unpack_from('>H', self.data, at)[0]

    def s16(self, at):
        if at < 0 or at + 2 > len(self.data):
            raise ScanError('read past the file')
        return struct.unpack_from('>h', self.data, at)[0]

    def ref(self, at):
        """The file offset a segment-5 word at `at` names, or None for 0."""
        if at < 0 or at + 4 > len(self.data):
            raise ScanError('read past the file')
        word = struct.unpack_from('>I', self.data, at)[0]
        if word == 0:
            return None
        if word >> 24 != 5 or (word & 0xffffff) >= len(self.data):
            raise ScanError('a reference outside the file')
        return word & 0xffffff

    def opcode(self, node):
        return self.u16(node) & 0xff

    def nodes(self):
        """Every node reachable from the root by Child and Next, as the game's walks reach them."""
        seen, stack = set(), [self.root]
        while stack:
            node = stack.pop()
            if node is None or node in seen:
                continue
            seen.add(node)
            yield node
            stack.append(self.ref(node + 0x0c))
            stack.append(self.ref(node + 0x14))

    def slot(self, index):
        """The node a switch slot names; None for an empty slot or one past numSwitches."""
        if index >= self.num_switches:
            return None
        return self.ref(index * 4)


def opcode_name(opcode):
    return '%d %s' % (opcode, OPCODES[opcode] if opcode < len(OPCODES) else '?')


def inflate(raw):
    return zlib.decompress(raw[2:], -15) if raw[:2] == b'\x11\x72' else raw


def read_filelist(path):
    rows = []
    for line in Path(path).read_text(encoding='utf-8', errors='replace').splitlines():
        fields = line.split(',')
        if len(fields) >= 3 and fields[0].isdigit() and fields[1].isdigit():
            rows.append((int(fields[0]), int(fields[1]), fields[2]))
    return rows


def read_headers(assets):
    headers = {}
    for path in sorted(Path(assets).rglob('*.inc.c')):
        for match in HEADER_RE.finditer(path.read_text(encoding='utf-8', errors='replace')):
            headers[match.group(1).lower()] = (int(match.group(2), 0), int(match.group(3), 0))
    return headers


def load_models(rom, filelist, assets):
    """The chr, gun and prop model files that have a MODELFILEHEADER, inflated."""
    headers = read_headers(assets)
    models = []
    for offset, size, path in read_filelist(filelist):
        parts = path.split('/')
        if len(parts) < 4 or parts[1] != 'obseg' or parts[2] not in ('chr', 'gun', 'prop'):
            continue
        name = os.path.basename(path)[1:-5].lower()  # C/G/P + name + Z.bin
        if name not in headers:
            continue
        num_switches, num_textures = headers[name]
        models.append(Model(parts[2], name, inflate(rom[offset:offset + size]), num_switches, num_textures))
    return models


def select(models, cls, match, exclude):
    return [m for m in models if (cls is None or m.cls == cls) and (match is None or re.search(match, m.name))
            and (exclude is None or not re.search(exclude, m.name))]


def count_roots(models):
    counts = collections.Counter()
    for model in models:
        try:
            counts[(model.cls, opcode_name(model.opcode(model.root)))] += 1
        except ScanError:
            counts[(model.cls, UNREADABLE)] += 1
    return counts


def count_slots(models, slots):
    counts = collections.Counter()
    for model in models:
        for index in slots:
            try:
                node = model.slot(index)
                counts[(model.cls, index, EMPTY if node is None else opcode_name(model.opcode(node)))] += 1
            except ScanError:
                counts[(model.cls, index, UNREADABLE)] += 1
    return counts


def count_targets(models, opcode, targets):
    """For each node of `opcode`, the opcodes of the nodes its record names (`targets` lists them)."""
    counts = collections.Counter()
    for model in models:
        try:
            for node in model.nodes():
                if model.opcode(node) != opcode:
                    continue
                for target in targets(model, model.ref(node + 4)):
                    counts[(model.cls, EMPTY if target is None else opcode_name(model.opcode(target)))] += 1
        except ScanError:
            counts[(model.cls, UNREADABLE)] += 1
    return counts


def shadow_header(model, record):
    return [model.ref(record + 0x14)]


def collision_links(model, record):
    count, vertices = model.s16(record + 0x0e), model.ref(record + 0x10)
    if vertices is None:
        return []
    links = (model.ref(vertices + v * VERTEX_SIZE + 8) for v in range(count))
    return [link for link in links if link is not None]


def count_intros(rom, filelist, match):
    """Intro record types in the setups whose path matches; None when no setup matches."""
    counts, setups = collections.Counter(), 0
    for offset, size, path in read_filelist(filelist):
        if '/setup/' not in path or not re.search(match, path):
            continue
        data = inflate(rom[offset:offset + size])
        setups += 1
        at = struct.unpack_from('>I', data, 8)[0]
        while True:
            kind = struct.unpack_from('>i', data, at)[0]
            if kind == 9:
                break
            if kind not in INTRO_WORDS:
                counts[UNREADABLE] += 1
                break
            counts[INTRO_NAMES[kind]] += 1
            at += 4 * INTRO_WORDS[kind]
    return (setups, counts) if setups else None


def parse_slots(text):
    slots = []
    for part in text.split(','):
        low, _, high = part.partition('-')
        slots.extend(range(int(low, 0), int(high or low, 0) + 1))
    return slots


def print_counts(counts):
    for key in sorted(counts, key=str):
        print('  '.join(str(k) for k in key) + ': %d' % counts[key])


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--rom', default=os.environ.get('GE_ROM'), help='the US ROM (default: GE_ROM)')
    parser.add_argument('--filelist', default=str(ROOT/'scripts/filelist.u.csv'))
    parser.add_argument('--assets', default=str(ROOT/'assets/obseg'))
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('roots', 'slots', 'shadows', 'links'):
        command = sub.add_parser(name)
        command.add_argument('--class', dest='cls', choices=('chr', 'gun', 'prop'))
        command.add_argument('--match', help='regular expression a model name must match')
        command.add_argument('--exclude', help='regular expression a model name must not match')
        if name == 'slots':
            command.add_argument('--slots', required=True, help='slot numbers, e.g. 0,3-7')
    intros = sub.add_parser('intros')
    intros.add_argument('--match', default='mp_setup', help='regular expression a setup path must match')
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    if not args.rom or not Path(args.rom).is_file():
        print('lp64_model_scan: no ROM (pass --rom or set GE_ROM)', file=sys.stderr)
        return 1
    rom = Path(args.rom).read_bytes()
    if args.command == 'intros':
        found = count_intros(rom, args.filelist, args.match)
        if found is None:
            print('lp64_model_scan: no setup matches %r' % args.match, file=sys.stderr)
            return 1
        print('setups: %d' % found[0])
        print_counts({(k,): v for k, v in found[1].items()})
        return 0
    models = select(load_models(rom, args.filelist, args.assets), args.cls, args.match, args.exclude)
    if not models:
        print('lp64_model_scan: no model found (check --rom, --filelist, --assets and the filters)', file=sys.stderr)
        return 1
    print('models: %d (%s)' % (len(models), ', '.join('%s %d' % kv for kv in
                                                     sorted(collections.Counter(m.cls for m in models).items()))))
    if args.command == 'roots':
        print_counts(count_roots(models))
    elif args.command == 'slots':
        print_counts(count_slots(models, parse_slots(args.slots)))
    elif args.command == 'shadows':
        print_counts(count_targets(models, SHADOW, shadow_header))
    else:
        print_counts(count_targets(models, DLCOLLISION, collision_links))
    return 0


if __name__ == '__main__':
    sys.exit(main())
