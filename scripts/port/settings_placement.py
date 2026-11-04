#!/usr/bin/env python3
"""Keep Iron Sights settings on the Iron Sights pages (AGENTS.md, FORK-044).

A setting is upstream's when its key is in settings_upstream_keys.txt, the
option table at the inherited baseline. Every other key in port_options.c is
an Iron Sights setting: it belongs on the folder's Iron Sights subsections
(front.c port_sheet_rows_ironsights / _is_*) and the watch's (port_watchpages.c
sIronSights / sIs*), and on no upstream page. Upstream keys stay off those pages.

    python scripts/port/settings_placement.py            check the tree
    python scripts/port/settings_placement.py --refresh REF
                                                        rewrite the key list
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OPTIONS = 'port/src/port_options.c'
FOLDER = 'src/game/front.c'
WATCH = 'port/src/port_watchpages.c'
UPSTREAM_KEYS = Path(__file__).with_name('settings_upstream_keys.txt')

# Iron Sights keys deliberately absent from a menu, each with its reason.
INI_ONLY = {}
FOLDER_ONLY = {}
WATCH_ONLY = {}

OPTION_ROW = re.compile(r'^\s*(?:TOGGLET?|ENUM[RP]|INTR|TENTHS|PCT)\s*\(\s*"([a-z0-9_]+)"', re.M)
FOLDER_TABLE = re.compile(r'static const struct PortSheetRow (port_sheet_rows_\w+)\[\] = \{(.*?)\n\};', re.S)
WATCH_TABLE = re.compile(r'static const struct PortWatchRowDef (s\w+)\[\] = \{(.*?)\n\};', re.S)
FOLDER_KEY = re.compile(r'\{\s*SHROW_OPT,\s*"([a-z0-9_]+)"')
WATCH_KEY = re.compile(r'\{\s*PWROW_OPT,\s*"([a-z0-9_]+)"')


def option_keys(source):
    return set(OPTION_ROW.findall(source))


def menu_tables(source, table, key):
    return {name: set(key.findall(body)) for name, body in table.findall(source)}


def is_folder_page(name):
    return name == 'port_sheet_rows_ironsights' or name.startswith('port_sheet_rows_is_')


def is_watch_page(name):
    return name == 'sIronSights' or name.startswith('sIs')


def split_pages(tables, ours):
    """(keys on Iron Sights pages, {upstream page: keys})"""
    mine, theirs = set(), {}
    for name, keys in tables.items():
        if ours(name):
            mine |= keys
        else:
            theirs[name] = keys
    return mine, theirs


def placement_errors(options, upstream, folder, watch):
    """Every misplaced setting, as readable lines; empty when the menus agree."""
    fork = options - upstream
    folder_mine, folder_theirs = split_pages(folder, is_folder_page)
    watch_mine, watch_theirs = split_pages(watch, is_watch_page)
    errors = []
    for key in sorted(fork):
        if key in INI_ONLY:
            continue
        if key not in folder_mine and key not in WATCH_ONLY:
            errors.append(f'{key}: Iron Sights setting missing from the folder Iron Sights pages ({FOLDER})')
        if key not in watch_mine and key not in FOLDER_ONLY:
            errors.append(f'{key}: Iron Sights setting missing from the watch Iron Sights pages ({WATCH})')
    for menu, pages in ((FOLDER, folder_theirs), (WATCH, watch_theirs)):
        for name, keys in sorted(pages.items()):
            for key in sorted(keys & fork):
                errors.append(f'{key}: Iron Sights setting on upstream page {name} ({menu})')
    for menu, keys in ((FOLDER, folder_mine), (WATCH, watch_mine)):
        for key in sorted(keys & upstream):
            errors.append(f'{key}: upstream setting on an Iron Sights page ({menu})')
    return errors


def read_upstream(path=UPSTREAM_KEYS):
    lines = path.read_text(encoding='utf-8').splitlines()
    return {line.strip() for line in lines if line.strip() and not line.startswith('#')}


def refresh(ref):
    source = subprocess.check_output(['git', '-C', str(ROOT), 'show', f'{ref}:{OPTIONS}'], text=True)
    commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', ref], text=True).strip()
    header = [f'# Option keys in {OPTIONS} at the inherited upstream baseline {commit}.',
              '# Regenerate only when FORK_CHANGES.md deliberately advances that baseline:',
              f'#   python scripts/port/settings_placement.py --refresh <baseline>']
    UPSTREAM_KEYS.write_text('\n'.join(header + sorted(option_keys(source))) + '\n', encoding='utf-8')


def check(root=ROOT):
    read = lambda name: (root/name).read_text(encoding='utf-8')
    return placement_errors(option_keys(read(OPTIONS)), read_upstream(),
                            menu_tables(read(FOLDER), FOLDER_TABLE, FOLDER_KEY),
                            menu_tables(read(WATCH), WATCH_TABLE, WATCH_KEY))


def main(argv):
    if len(argv) == 3 and argv[1] == '--refresh':
        refresh(argv[2])
        return 0
    if len(argv) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    errors = check()
    for error in errors:
        print('settings placement: ' + error, file=sys.stderr)
    if errors:
        print('See AGENTS.md "Iron Sights settings" for where fork settings go.', file=sys.stderr)
        return 1
    print('Settings placement: Iron Sights settings are on the Iron Sights pages only.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
