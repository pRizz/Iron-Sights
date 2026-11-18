#!/usr/bin/env python3
"""Count what the stock ROM's models and setups hold where the code reads them untested (LP64-005 Task 3).

Some overlay verdicts rest on data, not on a guard in the code: a switch slot's node read as a
position, a root read as a header, a collision link read as a DLCOLLISION node. This reads the
ROM's chr, gun and prop model files and the setups and prints counts only (opcode numbers and
names, per model class and slot), never bytes of the ROM.

    python3 scripts/port/lp64_model_scan.py --rom ROM roots [--class chr] [--match RE] [--exclude RE]
    python3 scripts/port/lp64_model_scan.py --rom ROM slots --slots 0,3-7 [--class gun] [--match RE]
    python3 scripts/port/lp64_model_scan.py --rom ROM shadows | links
    python3 scripts/port/lp64_model_scan.py --rom ROM textures [--class prop] [--match RE]
    python3 scripts/port/lp64_model_scan.py --rom ROM rwfirst [--class gun] [--match RE]
    python3 scripts/port/lp64_model_scan.py --rom ROM intros [--match mp_setup] [--items]

The ROM path is --rom or GE_ROM. A model's switch slot count and texture count are not in its
file; they come from the MODELFILEHEADER lines under assets/obseg. Exits 1 when the scan finds no
model (or no setup), so a wrong path or filter cannot print zeros that read as "none"; when any
model or setup is unreadable (its row names it, and the rest of that model is not counted); and
when a chr, gun or prop model in the file list has no MODELFILEHEADER line, unless
--allow-missing names it (and an allowance that names no skipped model fails too). A slot index at
or past a model's numSwitches counts as `out of range`, not `empty`: the game would read past the
slot table there, not find NULL. `textures` counts the models whose texture table holds an inline
entry (a TextureID that is a segment-5 word, texels in the file, not a texture number), naming each
with how many of its entries are inline. `rwfirst` counts, per class, the opcode of the first node the
game's read-write walk (modelCalculateRwDataLen) gives a record, the record at index 0 that
modelGetNodeRwData returns for a node without one. `intros --items` also counts each ITEM record by its right
and left item numbers and demo field, and the setups by how many ITEM records they hold.
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
LOD, SWITCH, HEAD = 8, 18, 23
RW_OPCODES = {1, 7, LOD, 9, 11, 12, SWITCH, HEAD, DLCOLLISION}  # the nodes modelCalculateRwDataLen gives a record
NODE_SIZE, TEXTURE_SIZE, VERTEX_SIZE = 24, 12, 16
INTRO_WORDS = {0: 3, 1: 4, 2: 4, 3: 8, 4: 2, 5: 2, 6: 10, 7: 3, 8: 2}
INTRO_NAMES = ['SPAWN', 'ITEM', 'AMMO', 'SWIRL', 'ANIM', 'CUFF', 'CAMERA', 'WATCH', 'CREDITS', 'END']
HEADER_RE = re.compile(r'MODELFILEHEADER\(\s*(\w+),[^,]*,[^,]*,[^,]*,\s*(\w+),\s*\w+,\s*[-\d.e]+,\s*\w+,\s*(\w+)\s*\)')
EMPTY, OUT_OF_RANGE, UNREADABLE = 'empty', 'out of range', 'unreadable'


class ScanError(Exception):
    """The input is not shaped as the walk expects: a reference outside a file, a header name defined twice."""


class Model:
    """One model file, inflated, with the header counts the game's code supplies."""

    def __init__(self, cls, name, data, num_switches, num_textures):
        self.cls = cls
        self.name = name
        self.data = data
        self.num_switches = num_switches
        self.num_textures = num_textures
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
        """The node a switch slot names; None for an empty slot. The caller tests numSwitches."""
        return self.ref(index * 4)


def opcode_name(opcode):
    return '%d %s' % (opcode, OPCODES[opcode] if opcode < len(OPCODES) else '?')


def unreadable(model):
    return '%s (%s)' % (UNREADABLE, model.name)


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
    """{name: (numSwitches, numTextures)}; a name two files define raises ScanError naming both."""
    headers, where = {}, {}
    for path in sorted(Path(assets).rglob('*.inc.c')):
        for match in HEADER_RE.finditer(path.read_text(encoding='utf-8', errors='replace')):
            name = match.group(1).lower()
            if name in where:
                raise ScanError('MODELFILEHEADER %s is defined in %s and %s' % (name, where[name], path))
            headers[name], where[name] = (int(match.group(2), 0), int(match.group(3), 0)), path
    return headers


def model_rows(filelist):
    """(offset, size, class, name) for each chr, gun and prop model file in the file list."""
    for offset, size, path in read_filelist(filelist):
        parts = path.split('/')
        if len(parts) < 4 or parts[1] != 'obseg' or parts[2] not in ('chr', 'gun', 'prop'):
            continue
        yield offset, size, parts[2], os.path.basename(path)[1:-5].lower()  # C/G/P + name + Z.bin


def missing_headers(filelist, assets):
    """The model files in the file list that have no MODELFILEHEADER line, so load_models skips them."""
    headers = read_headers(assets)
    return sorted(name for _, _, _, name in model_rows(filelist) if name not in headers)


def load_models(rom, filelist, assets):
    """The chr, gun and prop model files that have a MODELFILEHEADER, inflated."""
    headers = read_headers(assets)
    models = []
    for offset, size, cls, name in model_rows(filelist):
        if name not in headers:
            continue
        num_switches, num_textures = headers[name]
        models.append(Model(cls, name, inflate(rom[offset:offset + size]), num_switches, num_textures))
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
            counts[(model.cls, unreadable(model))] += 1
    return counts


def count_slots(models, slots):
    counts = collections.Counter()
    for model in models:
        for index in slots:
            if index >= model.num_switches:
                counts[(model.cls, index, OUT_OF_RANGE)] += 1
                continue
            try:
                node = model.slot(index)
                counts[(model.cls, index, EMPTY if node is None else opcode_name(model.opcode(node)))] += 1
            except ScanError:
                counts[(model.cls, index, unreadable(model))] += 1
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
            counts[(model.cls, unreadable(model))] += 1
    return counts


def walk_children(model, node):
    """The node modelCalculateRwDataLen's walk descends to from `node`: an LOD's Affects, a switch's
    Controls, nothing under a head placeholder, otherwise the node's Child."""
    opcode = model.opcode(node)
    if opcode == LOD:
        return model.ref(model.ref(node + 4) + 8)
    if opcode == SWITCH:
        return model.ref(model.ref(node + 4))
    if opcode == HEAD:
        return None
    return model.ref(node + 0x14)


def count_rw_first(models):
    """Per class, the opcode of the first node the read-write walk gives a record: the record at index 0,
    which modelGetNodeRwData returns for a node that has none."""
    counts = collections.Counter()
    for model in models:
        try:
            stack, seen, first = [model.root], set(), None
            while stack and first is None:
                node = stack.pop()
                if node is None or node in seen:
                    continue
                seen.add(node)
                if model.opcode(node) in RW_OPCODES:
                    first = model.opcode(node)
                    break
                stack.append(model.ref(node + 0x0c))
                stack.append(walk_children(model, node))
            counts[(model.cls, EMPTY if first is None else opcode_name(first))] += 1
        except ScanError:
            counts[(model.cls, unreadable(model))] += 1
    return counts


def shadow_header(model, record):
    return [model.ref(record + 0x14)]


def collision_links(model, record):
    count, vertices = model.s16(record + 0x0e), model.ref(record + 0x10)
    if vertices is None:
        return []
    links = (model.ref(vertices + v * VERTEX_SIZE + 8) for v in range(count))
    return [link for link in links if link is not None]


def count_textures(models):
    """Per model with an inline texture entry, (class, name, 'inline k of n'); the others by class."""
    counts = collections.Counter()
    for model in models:
        table = model.num_switches * 4
        try:
            words = [model.u16(table + i * TEXTURE_SIZE) << 16 | model.u16(table + i * TEXTURE_SIZE + 2)
                     for i in range(model.num_textures)]
        except ScanError:
            counts[(model.cls, unreadable(model))] += 1
            continue
        inline = sum(word >> 24 == 5 for word in words)
        if inline:
            counts[(model.cls, model.name, 'inline %d of %d' % (inline, len(words)))] += 1
        else:
            counts[(model.cls, 'no inline entry')] += 1
    return counts


def count_intros(rom, filelist, match, items=False):
    """Intro record types in the setups whose path matches; None when no setup matches."""
    counts, setups = collections.Counter(), 0
    for offset, size, path in read_filelist(filelist):
        if '/setup/' not in path or not re.search(match, path):
            continue
        data = inflate(rom[offset:offset + size])
        setups += 1
        at, item_records = struct.unpack_from('>I', data, 8)[0], 0
        while True:
            kind = struct.unpack_from('>i', data, at)[0]
            if kind == 9:
                break
            if kind not in INTRO_WORDS:
                counts['%s (%s)' % (UNREADABLE, os.path.basename(path))] += 1
                break
            counts[INTRO_NAMES[kind]] += 1
            if items and kind == 1:
                item_records += 1
                counts['ITEM right=%d left=%d demo=%d' % struct.unpack_from('>iii', data, at + 4)] += 1
            at += 4 * INTRO_WORDS[kind]
        if items:
            counts['setups with %d ITEM' % item_records] += 1
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
    parser.add_argument('--allow-missing', action='append', default=[], metavar='NAME',
                        help='a model file that may lack a MODELFILEHEADER line (repeat for each)')
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('roots', 'slots', 'shadows', 'links', 'textures', 'rwfirst'):
        command = sub.add_parser(name)
        command.add_argument('--class', dest='cls', choices=('chr', 'gun', 'prop'))
        command.add_argument('--match', help='regular expression a model name must match')
        command.add_argument('--exclude', help='regular expression a model name must not match')
        if name == 'slots':
            command.add_argument('--slots', required=True, help='slot numbers, e.g. 0,3-7')
    intros = sub.add_parser('intros')
    intros.add_argument('--match', default='mp_setup', help='regular expression a setup path must match')
    intros.add_argument('--items', action='store_true', help='also count ITEM records by their fields')
    return parser.parse_args(argv)


def report_unreadable(counts):
    """Names every unreadable row on stderr; True when there is none."""
    rows = sorted(str(key[-1]) for key in counts if str(key[-1]).startswith(UNREADABLE))
    for row in rows:
        print('lp64_model_scan: %s: the counts above are incomplete' % row, file=sys.stderr)
    return not rows


def check_headers(args):
    """True when the models without a header are exactly those --allow-missing names."""
    missing, allowed = set(missing_headers(args.filelist, args.assets)), set(args.allow_missing)
    for name in sorted(missing - allowed):
        print('lp64_model_scan: model %s has no MODELFILEHEADER line and was not scanned' % name, file=sys.stderr)
    for name in sorted(allowed - missing):
        print('lp64_model_scan: --allow-missing %s names no skipped model' % name, file=sys.stderr)
    return missing == allowed


def main(argv=None):
    args = parse_args(argv)
    if not args.rom or not Path(args.rom).is_file():
        print('lp64_model_scan: no ROM (pass --rom or set GE_ROM)', file=sys.stderr)
        return 1
    rom = Path(args.rom).read_bytes()
    if args.command == 'intros':
        found = count_intros(rom, args.filelist, args.match, args.items)
        if found is None:
            print('lp64_model_scan: no setup matches %r' % args.match, file=sys.stderr)
            return 1
        print('setups: %d' % found[0])
        print_counts({(k,): v for k, v in found[1].items()})
        return 0 if report_unreadable({(k,): v for k, v in found[1].items()}) else 1
    try:
        headers_ok = check_headers(args)
        models = select(load_models(rom, args.filelist, args.assets), args.cls, args.match, args.exclude)
    except ScanError as error:
        print('lp64_model_scan: %s' % error, file=sys.stderr)
        return 1
    if not models:
        print('lp64_model_scan: no model found (check --rom, --filelist, --assets and the filters)', file=sys.stderr)
        return 1
    print('models: %d (%s)' % (len(models), ', '.join('%s %d' % kv for kv in
                                                     sorted(collections.Counter(m.cls for m in models).items()))))
    if args.command == 'roots':
        counts = count_roots(models)
    elif args.command == 'slots':
        counts = count_slots(models, parse_slots(args.slots))
    elif args.command == 'rwfirst':
        counts = count_rw_first(models)
    elif args.command == 'textures':
        counts = count_textures(models)
    elif args.command == 'shadows':
        counts = count_targets(models, SHADOW, shadow_header)
    else:
        counts = count_targets(models, DLCOLLISION, collision_links)
    print_counts(counts)
    readable = report_unreadable(counts)
    return 0 if readable and headers_ok else 1


if __name__ == '__main__':
    sys.exit(main())
