#!/usr/bin/env python3
"""LP64-006's secondary picture checks (Task 8): drawn pixels, beside the picture digest.

    python3 scripts/port/lp64_equiv_pictures.py shots [--tick TICK] DIR_A DIR_B
    python3 scripts/port/lp64_equiv_pictures.py gallery --rom ROM [--timeout SECONDS] [--keep-shots]
        --output DIR EXECUTABLE
    python3 scripts/port/lp64_equiv_pictures.py gallery-compare DIR_A DIR_B

shots: DIR_A and DIR_B are folders of `lp64_equiv.py run --video --draw --shots TICK`. For each case
of DIR_A's record (equiv.json), its run 1 screenshot at the pinned tick (DIR/run1/CASE.shots/
shot_TICK.bmp, TICK the record's `shots` unless --tick) against DIR_B's: `CASE: identical` or
`CASE: N of M pixels differ, largest channel difference D`. Exit 0 when every case is identical,
1 when one differs, 2 when a screenshot is missing or unreadable or the two differ in size (such a
case is `CASE: FAIL (reason)`: neither equal nor different).

gallery: the hand gallery's capture mode (GE_HAND_GALLERY=1, hand_gallery.py's FIXTURE_INI and the
variables hand_gallery.run_game sets, read from it by capture_environment) on EXECUTABLE, offscreen
(SDL_VIDEODRIVER=offscreen), in DIR/game/ (a copy of the executable, removed when it ends; HOME and
the icon cache inside DIR). The game's output goes to DIR/gallery.engine.log and
DIR/gallery.stdout.log; a progress line every 12 captures gives the count and the seconds so far.
When all hand_gallery.CASES cases' two samples are written, DIR/gallery.txt gets one line a capture,
`CASE SAMPLE SHA256 CELL` (the SHA-256 of its RGB pixels, as the gallery's pixels_sha256), and the
screenshots are deleted unless --keep-shots. A run past --timeout (3,600 s by default) is stopped
and fails with the count it reached. These are ROM-derived pictures and hashes: they stay under
build/lp64/. hand_gallery.py is imported, never edited, and its `generate` is never called.

gallery-compare: DIR_A/gallery.txt against DIR_B/gallery.txt, each a full list: one line a capture
that differs, then `N of 216 captures differ (C of 108 cases)`. Exit 0 when none differs, 1 when
one does, 2 when a list is missing or incomplete.

The rule (the design's): a case whose pixels differ while its digest (CASE.gfx) is identical is the
rasterizer's and is recorded with its size; different digests are a port defect or an explained
difference (lp64_equiv_expected.txt).
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

import hand_gallery  # noqa: E402

RECORD = 'equiv.json'
GALLERY_FILE = 'gallery.txt'
SAMPLES = (1, 2)
CAPTURES = hand_gallery.CASES * len(SAMPLES)
DEFAULT_TIMEOUT = 3600
PROGRESS_EVERY = 12
# The game's lines for a capture (port/src/joy_port.c and gfx_sdl2.cpp), as hand_gallery.py reads them.
CAPTURE_LINE = re.compile(r'hand-gallery capture case=(\d+) sample=(\d+)')
WROTE_LINE = re.compile(r'gfx: wrote (shot_\d+\.bmp)')
COMPLETE_LINE = f'hand-gallery complete cases={hand_gallery.CASES} samples={CAPTURES}'


class Unreadable(Exception):
    """A picture or a list that is missing, torn or not of the kind expected."""


# ---- pictures -----------------------------------------------------------------------------------
def read_bmp(path: Path) -> tuple[int, int, bytes]:
    """(width, height, pixels) of an uncompressed 24-bit BMP, the pixels as BGR rows from the bottom
    (a row order and channel order both pictures share, so only equality and size matter)."""
    try:
        data = path.read_bytes()
    except OSError as error:
        raise Unreadable(f'no {path.name} in {path.parent}') from error
    if len(data) < 54 or data[:2] != b'BM':
        raise Unreadable(f'{path} is not a BMP')
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height, planes, bits, compression = struct.unpack_from('<iiHHI', data, 18)
    if width <= 0 or height == 0 or planes != 1 or bits != 24 or compression:
        raise Unreadable(f'{path} is not an uncompressed 24-bit BMP')
    stride = (width * 3 + 3) & ~3
    if offset + stride * abs(height) > len(data):
        raise Unreadable(f'{path} is torn')
    rows = b''.join(data[offset + y * stride:offset + y * stride + width * 3] for y in range(abs(height)))
    return width, abs(height), rows


def pixel_difference(a: bytes, b: bytes) -> tuple[int, int]:
    """(pixels that differ, largest channel difference) between two equal-sized BGR pixel strings."""
    if a == b:
        return 0, 0
    count, largest = 0, 0
    for at in range(0, len(a), 3):
        if a[at:at + 3] != b[at:at + 3]:
            count += 1
            largest = max(largest, *(abs(x - y) for x, y in zip(a[at:at + 3], b[at:at + 3])))
    return count, largest


def shot_verdict(case: str, path_a: Path, path_b: Path) -> tuple[int, str]:
    """(status, line) for one case: 0 identical, 1 differs, 2 fails."""
    try:
        width_a, height_a, pixels_a = read_bmp(path_a)
        width_b, height_b, pixels_b = read_bmp(path_b)
    except Unreadable as error:
        return 2, f'{case}: FAIL ({error})'
    if (width_a, height_a) != (width_b, height_b):
        return 2, f'{case}: FAIL ({width_a}x{height_a} against {width_b}x{height_b})'
    count, largest = pixel_difference(pixels_a, pixels_b)
    if count == 0:
        return 0, f'{case}: identical'
    return 1, f'{case}: {count} of {width_a * height_a} pixels differ, largest channel difference {largest}'


def run_record(folder: Path) -> dict:
    try:
        return json.loads((folder/RECORD).read_text())
    except (OSError, ValueError) as error:
        raise Unreadable(f'{folder} has no readable {RECORD}') from error


def command_shots(args: argparse.Namespace) -> int:
    try:
        record = run_record(args.dir_a)
    except Unreadable as error:
        print(f'FAIL: {error}')
        return 2
    tick = args.tick if args.tick is not None else record.get('shots')
    if not isinstance(tick, int) or tick < 1:
        print(f'FAIL: {args.dir_a}/{RECORD} names no screenshot tick (a run without --shots): give --tick')
        return 2
    name = f'shot_{tick:05d}.bmp'
    statuses = []
    for case in record.get('cases', []):
        status, line = shot_verdict(case, *(folder/'run1'/f'{case}.shots'/name for folder in (args.dir_a, args.dir_b)))
        statuses.append(status)
        print(line, flush=True)
    if not statuses:
        print(f'FAIL: {args.dir_a}/{RECORD} names no case')
        return 2
    identical = statuses.count(0)
    summary = f'{len(statuses)} cases, {identical} identical, {statuses.count(1)} differ, {statuses.count(2)} failed'
    print(('OK: ' if identical == len(statuses) else 'FAIL: ') + summary)
    return max(statuses)


# ---- the gallery --------------------------------------------------------------------------------
def capture_environment(rom: Path, cache: Path) -> dict[str, str]:
    """The environment hand_gallery.run_game gives a gallery capture, read by calling it with a
    process that records its environment and ends at once (so the two cannot drift), and
    SDL_VIDEODRIVER=offscreen, which needs no display (Mesa's software GL)."""
    recorded: dict[str, str] = {}

    class Recorder:
        returncode = 0

        def __init__(self, _argv, env=None, **_options):
            recorded.update(env or {})

        def poll(self):
            return 0

        def wait(self, timeout=None):
            return 0

    stand_in = SimpleNamespace(Popen=Recorder, CREATE_NO_WINDOW=0, STARTUPINFO=None, STARTF_USESHOWWINDOW=0,
                               TimeoutExpired=subprocess.TimeoutExpired)
    with tempfile.TemporaryDirectory(prefix='midas-gallery-env-') as temporary:
        game = Path(temporary)/'game'
        game.mkdir()
        with mock.patch.object(hand_gallery, 'subprocess', stand_in):
            hand_gallery.run_game(game/'IronSights', rom, game, cache, 'probe', gallery=True)
    return dict(recorded, SDL_VIDEODRIVER='offscreen')


def captures_in(log: str) -> list[tuple[int, int, str]]:
    """(case, sample, screenshot file) for each capture the engine log names, in order."""
    found, maybe_pending = [], None
    for line in log.splitlines():
        if match := CAPTURE_LINE.search(line):
            maybe_pending = int(match[1]), int(match[2])
        elif (match := WROTE_LINE.search(line)) and maybe_pending is not None:
            found.append((*maybe_pending, match[1]))
            maybe_pending = None
    return found


def run_capture(executable: Path, game: Path, env: dict[str, str], output: Path, timeout: float) -> tuple[bool, str]:
    """Runs the capture in GAME until it exits or TIMEOUT seconds pass, printing progress; (whether it
    exited 0 in time, the engine log)."""
    engine = output/'gallery.engine.log'
    start, reported = time.monotonic(), 0
    with (output/'gallery.stdout.log').open('wb') as out, engine.open('wb') as errors:
        process = subprocess.Popen([str(executable)], cwd=game, env=env, stdout=out, stderr=errors)
        try:
            while process.poll() is None:
                elapsed = time.monotonic() - start
                count = len(captures_in(engine.read_text(errors='replace')))
                if count >= reported + PROGRESS_EVERY:
                    reported = count - count % PROGRESS_EVERY
                    print(f'  {count} of {CAPTURES} captures in {elapsed:.0f} s', flush=True)
                if elapsed > timeout:
                    print(f'FAIL: stopped at {timeout:.0f} s with {count} of {CAPTURES} captures', flush=True)
                    return False, engine.read_text(errors='replace')
                time.sleep(1)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    print(f'  the game exited {process.returncode} after {time.monotonic() - start:.0f} s', flush=True)
    return process.returncode == 0, engine.read_text(errors='replace')


def gallery_lines(log: str, game: Path) -> list[str]:
    """One `CASE SAMPLE SHA256 CELL` line a capture, in the gallery's order; Unreadable unless the log
    holds every case's two samples once and the gallery's closing line."""
    cells = {case['id']: case['file'] for case in hand_gallery.cases()}
    hashes: dict[tuple[int, int], str] = {}
    for case, sample, name in captures_in(log):
        if (case, sample) in hashes or case not in cells or sample not in SAMPLES:
            raise Unreadable(f'capture case={case} sample={sample} is repeated or outside the gallery')
        hashes[case, sample] = hashlib.sha256(hand_gallery.bmp_pixels(game/name)).hexdigest()
    if len(hashes) != CAPTURES or COMPLETE_LINE not in log:
        raise Unreadable(f'{len(hashes)} of {CAPTURES} captures written, '
                         f'{"with" if COMPLETE_LINE in log else "without"} the line {COMPLETE_LINE!r}')
    return [f'{case} {sample} {hashes[case, sample]} {cells[case]}' for case in cells for sample in SAMPLES]


def command_gallery(args: argparse.Namespace) -> int:
    if args.timeout <= 0:
        print('error: --timeout takes a number of seconds above 0', file=sys.stderr)
        return 2
    executable = args.executable.resolve(strict=True)
    output = args.output.resolve()
    game, home = output/'game', output/'home'
    for folder in (game, home):
        if folder.exists():
            print(f'error: {folder} exists: give a fresh --output', file=sys.stderr)
            return 2
        folder.mkdir(parents=True)
    copy = game/executable.name
    shutil.copy2(executable, copy)
    (game/'midas.ini').write_text(hand_gallery.FIXTURE_INI, encoding='utf-8')
    env = dict(capture_environment(args.rom.resolve(strict=True), output/'icons'), HOME=str(home))
    try:
        exited, log = run_capture(copy, game, env, output, args.timeout)
    finally:
        copy.unlink(missing_ok=True)
    try:
        lines = gallery_lines(log, game)
    except (Unreadable, ValueError, OSError) as error:
        print(f'FAIL: {error}; see {output}/gallery.engine.log')
        return 1
    if not exited:
        print(f'FAIL: the game did not exit 0; see {output}/gallery.engine.log')
        return 1
    (output/GALLERY_FILE).write_text('\n'.join(lines) + '\n')
    repeated = sum(1 for at in range(0, len(lines), 2) if lines[at].split()[2] == lines[at + 1].split()[2])
    if not args.keep_shots:
        for shot in game.glob('shot_*.bmp'):
            shot.unlink()
    print(f'OK: {CAPTURES} captures in {output/GALLERY_FILE}; {repeated} of {hand_gallery.CASES} cases '
          'repeat their pose between samples')
    return 0


def read_gallery(folder: Path) -> dict[tuple[int, int], tuple[str, str]]:
    """{(case, sample): (hash, cell)} from FOLDER/gallery.txt; Unreadable unless every capture is there."""
    try:
        text = (folder/GALLERY_FILE).read_text()
    except OSError as error:
        raise Unreadable(f'no {GALLERY_FILE} in {folder}') from error
    entries = {}
    for line in text.splitlines():
        fields = line.split()
        if len(fields) != 4 or not fields[0].isdigit() or not fields[1].isdigit():
            raise Unreadable(f'{folder/GALLERY_FILE}: unreadable line {line!r}')
        entries[int(fields[0]), int(fields[1])] = fields[2], fields[3]
    wanted = {(case['id'], sample) for case in hand_gallery.cases() for sample in SAMPLES}
    if set(entries) != wanted:
        raise Unreadable(f'{folder/GALLERY_FILE} holds {len(set(entries) & wanted)} of {CAPTURES} captures')
    return entries


def command_gallery_compare(args: argparse.Namespace) -> int:
    try:
        a, b = read_gallery(args.dir_a), read_gallery(args.dir_b)
    except Unreadable as error:
        print(f'FAIL: {error}')
        return 2
    differing = [key for key in sorted(a) if a[key][0] != b[key][0]]
    for case, sample in differing:
        print(f'case {case} sample {sample} ({a[case, sample][1]}) differs')
    cases = len({case for case, _ in differing})
    print(f'{len(differing)} of {CAPTURES} captures differ ({cases} of {hand_gallery.CASES} cases)')
    return 1 if differing else 0


# ---- the command line ---------------------------------------------------------------------------
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    shots = sub.add_parser('shots', help='compare each case\'s screenshot at its pinned tick')
    shots.add_argument('--tick', type=int, metavar='TICK', help='the screenshot\'s tick (the record\'s --shots)')
    shots.add_argument('dir_a', type=Path, metavar='DIR_A')
    shots.add_argument('dir_b', type=Path, metavar='DIR_B')
    gallery = sub.add_parser('gallery', help='run the hand gallery\'s capture offscreen and hash its captures')
    gallery.add_argument('--rom', type=Path, required=True)
    gallery.add_argument('--timeout', type=float, default=DEFAULT_TIMEOUT, metavar='SECONDS')
    gallery.add_argument('--keep-shots', action='store_true', help='keep the screenshots in DIR/game/')
    gallery.add_argument('--output', type=Path, required=True, metavar='DIR')
    gallery.add_argument('executable', type=Path, metavar='EXECUTABLE')
    compare = sub.add_parser('gallery-compare', help='count the captures whose hashes differ')
    compare.add_argument('dir_a', type=Path, metavar='DIR_A')
    compare.add_argument('dir_b', type=Path, metavar='DIR_B')
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    command = {'shots': command_shots, 'gallery': command_gallery, 'gallery-compare': command_gallery_compare}
    return command[args.command](args)


if __name__ == '__main__':
    sys.exit(main())
