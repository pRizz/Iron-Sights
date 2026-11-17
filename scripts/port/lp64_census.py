"""Compare what the 32- and 64-bit mission smokes load, and list what the 64-bit one walked (LP64-004).

    python3 scripts/port/lp64_census.py compare DIR32 DIR64
    python3 scripts/port/lp64_census.py types DIR

DIR32 and DIR64 are the output folders of audit_buffers.py's --mission-smoke run with --fixed-tick
(one MISSION.log a mission), from a 32-bit and a 64-bit executable. The flag runs one game field a
frame (GE_FIXEDTICK=1), so both widths reach the same point of each mission in its frames: on
wall-clock time the emulated 32-bit run, being slower, ended the intro sooner and loaded the
first-person weapon and left hand that the native 64-bit run had not reached yet. It also sets
GE_LP64_CENSUS=1, which `types` reads.

compare: for each mission, the multiset of the `ge: romdata:` lines that name a model, a setup, a
stan, a bg or a text bank, with the counts they print. Both widths print these lines with the same
text: the 32-bit in-place passes, and the 64-bit converters for models, setups and stans (counts of
the file's records: nodes, vertices and lists; prop list words, intro words and pads; tiles, rooms and
the bytes the tiles span), the bg and text passes being the same code at both widths. Lines are
compared with their runs of spaces made one, and in any order. A mission passes only if its two
multisets are equal and its 32-bit set holds at least one line of each of the five kinds, so that a
smoke whose logs lack the lines does not pass by comparing nothing. Left out of the comparison: every
other line, among them the `ge: lp64:` lines only a 64-bit build prints (the bg tables' converted
bytes, the audio bank and music table sizes), its `ge: romconv: setup NAME image:` line, the `seg` and
`brief` lines, and the `texture table` line. Exit 0 only if every mission passes; otherwise each
failing mission prints its first line found at one width only (in that log's order), and the
status is 1.

types: the model node opcodes and the setup prop record types the missions loaded, from the
`ge: census:` lines the 64-bit converters print when GE_LP64_CENSUS=1 (audit_buffers.py's smoke sets
it with --fixed-tick): `ge: census: model NAME opcodes OP=N ...` for each model converted,
`ge: census: setup NAME props TYPE=N ...` for each setup. It prints, for each opcode and type, the
records loaded and the file loads that held them (a model loaded twice counts twice), then the ones no
mission loaded. Exit 1 if the logs hold no census line.
"""
import argparse
from collections import Counter
import importlib.util
from pathlib import Path
import re
import sys

_spec = importlib.util.spec_from_file_location('audit_buffers', Path(__file__).with_name('audit_buffers.py'))
audit_buffers = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(audit_buffers)
MISSIONS = audit_buffers.MISSIONS

KINDS = ('model', 'setup', 'stan', 'bg', 'text')
ROMDATA_LINE = re.compile(r'^ge: romdata: (model|setup|stan|bg|text) +\S.*$', re.MULTILINE)
CENSUS_LINE = re.compile(r'^ge: census: (model|setup) (\S+) (opcodes|props)((?: \d+=\d+)*)\s*$', re.MULTILINE)

# bondconstants.h's MODELNODE_OPCODE and PROPDEF_TYPE, in order.
OPCODES = ('NULL', 'HEADER', 'GROUP', 'OP03', 'DL', 'OP05', 'OP06', 'OP07', 'LOD', 'BSP', 'BBOX', 'OP11',
           'GUNFIRE', 'SHADOW', 'OP14', 'INTERLINK', 'OP16', 'OP17', 'SWITCH', 'OP19', 'OP20', 'GROUPSIMPLE',
           'DLPRIMARY', 'HEAD', 'DLCOLLISION')
PROP_TYPES = ('NOTHING', 'DOOR', 'DOOR_SCALE', 'PROP', 'KEY', 'ALARM', 'CCTV', 'MAGAZINE', 'COLLECTABLE',
              'GUARD', 'MONITOR', 'MULTI_MONITOR', 'RACK', 'AUTOGUN', 'LINK', 'DEBRIS', 'UNK16', 'HAT',
              'GUARD_ATTRIBUTE', 'SWITCH', 'AMMO', 'ARMOUR', 'TAG', 'OBJECTIVE_START', 'OBJECTIVE_END',
              'OBJECTIVE_DESTROY_OBJECT', 'OBJECTIVE_COMPLETE_CONDITION', 'OBJECTIVE_FAIL_CONDITION',
              'OBJECTIVE_COLLECT_OBJECT', 'OBJECTIVE_DEPOSIT_OBJECT', 'OBJECTIVE_PHOTOGRAPH', 'OBJECTIVE_NULL',
              'OBJECTIVE_ENTER_ROOM', 'OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM', 'OBJECTIVE_COPY_ITEM',
              'WATCH_MENU_OBJECTIVE_TEXT', 'GAS_RELEASING', 'RENAME', 'LOCK_DOOR', 'VEHICHLE', 'AIRCRAFT', 'UNK41',
              'GLASS', 'SAFE', 'SAFE_ITEM', 'TANK', 'CAMERAPOS', 'TINTED_GLASS', 'END')


def census_lines(text: str) -> list[str]:
    """The compared lines of one log, in its order, each from its kind on with its spaces made one."""
    return [' '.join(match.group(0).split()[2:]) for match in ROMDATA_LINE.finditer(text)]


def first_only(lines: list[str], surplus: Counter) -> str:
    return next(line for line in lines if surplus[line] > 0)


def compare_mission(mission: str, dir32: Path, dir64: Path) -> tuple[bool, str]:
    """Whether MISSION's two logs load the same, and the line that says so or how they differ."""
    paths = (dir32 / f'{mission}.log', dir64 / f'{mission}.log')
    missing = [str(path) for path in paths if not path.is_file()]
    if missing:
        return False, f'{mission}: no log {", ".join(missing)}'
    lines32, lines64 = (census_lines(path.read_text(encoding='utf-8', errors='replace')) for path in paths)
    absent = [kind for kind in KINDS if not any(line.startswith(kind + ' ') for line in lines32)]
    if absent:
        return False, f'{mission}: no {" line, no ".join(absent)} line in the 32-bit log'
    count32, count64 = Counter(lines32), Counter(lines64)
    if count32 == count64:
        kinds = ', '.join(f'{sum(1 for line in lines32 if line.startswith(kind + " "))} {kind}' for kind in KINDS)
        return True, f'{mission}: {len(lines32)} lines equal ({kinds})'
    report = []
    if count32 - count64:
        report.append(f'{mission}: only in the 32-bit log: {first_only(lines32, count32 - count64)}')
    if count64 - count32:
        report.append(f'{mission}: only in the 64-bit log: {first_only(lines64, count64 - count32)}')
    return False, '\n'.join(report)


def compare(dir32: Path, dir64: Path) -> int:
    results = [compare_mission(mission, dir32, dir64) for mission in MISSIONS]
    for _, line in results:
        print(line)
    passed = sum(1 for ok, _ in results if ok)
    print(f'census: {passed} of {len(MISSIONS)} missions load the same at both widths')
    return 0 if passed == len(MISSIONS) else 1


def tally(folder: Path) -> dict[str, tuple[Counter, Counter]]:
    """{'model'/'setup': (records by number, file loads by number)} over every mission log in FOLDER."""
    found = {'model': (Counter(), Counter()), 'setup': (Counter(), Counter())}
    for mission in MISSIONS:
        path = folder / f'{mission}.log'
        if not path.is_file():
            continue
        for match in CENSUS_LINE.finditer(path.read_text(encoding='utf-8', errors='replace')):
            records, files = found[match.group(1)]
            for pair in match.group(4).split():
                number, count = (int(part) for part in pair.split('='))
                records[number] += count
                files[number] += 1
    return found


def report_kind(title: str, names: tuple[str, ...], records: Counter, files: Counter) -> None:
    print(f'{title} loaded:')
    for number in sorted(records):
        name = names[number] if number < len(names) else 'unknown'
        print(f'  {number} {name}: {records[number]} in {files[number]} load{"s" if files[number] != 1 else ""}')
    unloaded = [f'{number} {name}' for number, name in enumerate(names) if records[number] == 0]
    print(f'{title} no mission loaded: {", ".join(unloaded) if unloaded else "none"}')


def types(folder: Path) -> int:
    found = tally(folder)
    if not any(found[kind][0] for kind in found):
        print(f'no census line in the mission logs of {folder} (was the 64-bit smoke run with --fixed-tick?)')
        return 1
    report_kind('model node opcodes', OPCODES, *found['model'])
    report_kind('setup prop record types', PROP_TYPES, *found['setup'])
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    both = commands.add_parser('compare', help='compare a 32-bit and a 64-bit smoke folder')
    both.add_argument('dir32', type=Path)
    both.add_argument('dir64', type=Path)
    one = commands.add_parser('types', help='list the opcodes and prop types a 64-bit smoke loaded')
    one.add_argument('dir', type=Path)
    options = parser.parse_args(argv)
    if options.command == 'compare':
        return compare(options.dir32, options.dir64)
    return types(options.dir)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
