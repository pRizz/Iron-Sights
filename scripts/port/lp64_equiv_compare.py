"""The comparing half of lp64_equiv.py (LP64-006 Task 5): a case's streams against themselves, two
seeds' streams, and run folders against each other under lp64_equiv_expected.txt.

A run folder is compared only when its record (equiv.json) shows two runs or more of every compared
case, each of which ran and agreed with itself; `compare` refuses any other folder (exit 2), an
equiv.json from before the verdicts were recorded included.

A stream's file sits beside CASE.log (STREAM_FILES). A demo's `sim` stream is its playback: it is
read as `sim-demo` (lp64_equiv_streams.py), keyed by the recording's tick and blind to the host's
pace, though it is still printed as `sim`. A stream without a reader yet (`gfx`, `snd` until Tasks
7 and 9) is compared byte for byte, its difference named by the offset (field `byte`).
"""
from dataclasses import dataclass
import json
from pathlib import Path
import re
import sys

from lp64_equiv_cases import is_demo
from lp64_equiv_profile import PROFILES
from lp64_equiv_streams import READERS, Difference, Unreadable, first_difference

RECORD = 'equiv.json'
STREAM_FILES = {'sim': '.sim', 'gfx': '.gfx', 'snd': '.wav'}
VIDEO_STREAMS = ('gfx', 'snd')


def stream_kind(case: str, stream: str) -> str:
    """The reader kind of CASE's STREAM: `sim-demo` for a demo's simulation trace."""
    return 'sim-demo' if stream == 'sim' and is_demo(case) else stream


def stream_difference(kind: str, path_a: Path, path_b: Path) -> Difference | None:
    """The first difference of two files of reader KIND, by its reader, or, for a kind without one
    yet, the first differing byte. Raises Unreadable for a missing, empty or torn file."""
    if kind in READERS:
        return first_difference(kind, path_a, path_b)
    try:
        a, b = Path(path_a).read_bytes(), Path(path_b).read_bytes()
    except OSError as error:
        raise Unreadable(f'{error.filename}: {error.strerror}') from error
    if not a or not b:
        raise Unreadable(f'{path_a if not a else path_b}: empty')
    if a == b:
        return None
    offset = next((index for index, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
    return Difference(offset, 'byte', a[offset:offset + 1].hex(), b[offset:offset + 1].hex(), 0)


def case_difference(case: str, stream: str, folder_a: Path, folder_b: Path) -> Difference | None:
    name = f'{case}{STREAM_FILES[stream]}'
    return stream_difference(stream_kind(case, stream), folder_a/name, folder_b/name)


def self_disagreements(cases: list[str], output: Path, streams: tuple[str, ...], repeat: int) -> dict[str, str]:
    """For each case of CASES whose later runs in OUTPUT/runK differ from run 1 (or cannot be read),
    the line `FAIL: CASE STREAM differs from itself at I` naming its first such stream."""
    found = {}
    for case in cases:
        for stream in streams:
            where = None
            for run in range(2, repeat + 1):
                try:
                    difference = case_difference(case, stream, output/'run1', output/f'run{run}')
                    where = None if difference is None else f'at {difference.index} ({difference.field}, run{run})'
                except Unreadable as error:
                    where = f'(cannot be read: {error})'
                if where is not None:
                    break
            if where is not None:
                found[case] = f'FAIL: {case} {stream} differs from itself {where}'
                break
    return found


def joined(names: list[str]) -> str:
    return names[0] if len(names) == 1 else ', '.join(names[:-1]) + ' and ' + names[-1]


def seed_verdict(case: str, folder_a: Path, folder_b: Path, streams: tuple[str, ...]) -> tuple[bool, str]:
    """Whether every stream of CASE differs between two seeds' run folders, and the line saying so."""
    same, differ = [], []
    for stream in streams:
        try:
            difference = case_difference(case, stream, folder_a, folder_b)
        except Unreadable as error:
            return False, f'FAIL: {stream} cannot be compared: {error}'
        (same if difference is None else differ).append(stream)
    if same:
        return False, f'FAIL: {joined(same)} {"is" if len(same) == 1 else "are"} the same between seeds'
    return True, f'OK: {joined(differ)} differ{"s" if len(differ) == 1 else ""} between seeds'


# ---- run folders against each other -------------------------------------------------------------
# A line of lp64_equiv_expected.txt: PAIR CASE PROFILE STREAM differs-from I KIND: REASON, KIND one
# of EXPECT_KINDS; an `explained:` reason ends with `; removed-by: RUN`.
EXPECT_KINDS = ('explained', 'open', 'strict', 'host')
EXPECT_LINE = re.compile(r'^(\S+:\S+) (\S+) (\S+) (\S+) differs-from (\d+) (' + '|'.join(EXPECT_KINDS)
                         + r'): (.+)$')


@dataclass(frozen=True)
class Expected:
    index: int
    kind: str
    reason: str


def read_expected(path: Path) -> dict[tuple[str, str, str, str], Expected]:
    """FILE's exceptions by (pair, case, profile, stream); raises ValueError for a line that is not
    one, an explanation without the run that removes its cause, or a key listed twice."""
    expected = {}
    for number, text in enumerate(path.read_text().splitlines(), start=1):
        text = text.strip()
        if not text or text.startswith('#'):
            continue
        match = EXPECT_LINE.match(text)
        if match is None:
            raise ValueError(f'{path}:{number}: not PAIR CASE PROFILE STREAM differs-from I KIND: REASON')
        pair, case, profile, stream, index, kind, reason = match.groups()
        if profile not in PROFILES or stream not in STREAM_FILES:
            raise ValueError(f'{path}:{number}: unknown profile {profile!r} or stream {stream!r}')
        if kind == 'explained' and '; removed-by: ' not in reason:
            raise ValueError(f'{path}:{number}: an explained difference names the run that removes it '
                             '(`; removed-by: RUN`)')
        key = (pair, case, profile, stream)
        if key in expected:
            raise ValueError(f'{path}:{number}: {" ".join(key)} is listed twice')
        expected[key] = Expected(int(index), kind, reason)
    return expected


def read_record(folder: Path) -> dict:
    try:
        return json.loads((folder/RECORD).read_text())
    except (OSError, ValueError) as error:
        raise ValueError(f'{folder}: no readable {RECORD} (not a run folder of lp64_equiv.py run)') from error


def refusal(record: dict, cases: list[str]) -> str | None:
    """Why a run folder with RECORD cannot stand for its build in a comparison of CASES: no stream is
    compared until two runs of the build gave the same file. None when it can."""
    verdicts = record.get('verdicts')
    if not isinstance(verdicts, dict):
        return 'its record has no agreement verdict (made before run recorded one; run it again)'
    if record.get('repeat', 1) < 2:
        return f'it ran each case once (--repeat {record.get("repeat", 1)}): nothing shows its runs agree'
    for case in cases:
        verdict = verdicts.get(case, {})
        if verdict.get('ran') is not True:
            return f'{case} did not run' if case in verdicts else f'{case} has no verdict'
        if verdict.get('agreed') is not True:
            return (f'{case} did not agree with itself' if verdict.get('agreed') is False
                    else f'{case} has no agreement verdict')
    return None


def failure_of(label: str, difference: Difference | None, maybe_expected: Expected | None,
               checking: bool) -> str | None:
    """Why the comparison LABEL is not as the file says (without one, not identical); None if it is."""
    if difference is None:
        if maybe_expected is None:
            return None
        return f'FAIL: {label} is recorded as differing from {maybe_expected.index}, but it is identical'
    if not checking:
        return f'FAIL: {label} differs at {difference.index}'
    if maybe_expected is None:
        return f'FAIL: {label} differs at {difference.index}, which the file does not record'
    if maybe_expected.index != difference.index:
        return f'FAIL: {label} differs at {difference.index}, recorded as differing from {maybe_expected.index}'
    return None


def record_count(case: str, stream: str, folder: Path) -> int:
    """The records of CASE's STREAM file in FOLDER (bytes for a stream without a reader)."""
    kind, path = stream_kind(case, stream), folder/f'{case}{STREAM_FILES[stream]}'
    return len(READERS[kind](path)) if kind in READERS else path.stat().st_size


def compare_folders(folders: list[Path], streams: list[str], maybe_expect: Path | None) -> int:
    """`compare`: each later folder's run 1 against the first's, printing a line a case and stream;
    0 when everything is identical or as MAYBE_EXPECT records, 1 otherwise, 2 for unusable input."""
    try:
        records = [read_record(folder) for folder in folders]
        expected = read_expected(maybe_expect) if maybe_expect is not None else {}
    except ValueError as error:
        print(f'error: {error}', file=sys.stderr)
        return 2
    base, profile = folders[0], records[0]['profile']
    for folder, record in zip(folders, records):
        reason = refusal(record, records[0]['cases'])
        if reason is not None:
            print(f'error: {folder} is refused: {reason}', file=sys.stderr)
            return 2
    if any(record['profile'] != profile for record in records):
        print('error: the run folders ran different profiles', file=sys.stderr)
        return 2
    if any(stream in VIDEO_STREAMS for stream in streams) and not all(record['video'] for record in records):
        print('error: gfx and snd need run folders made with --video', file=sys.stderr)
        return 2
    total, identical, recorded, failures = 0, 0, 0, []
    for folder in folders[1:]:
        pair = f'{base.name}:{folder.name}'
        for case in records[0]['cases']:
            for stream in streams:
                total += 1
                label = f'{case} {stream} {pair}'
                try:
                    difference = case_difference(case, stream, base/'run1', folder/'run1')
                except Unreadable as error:
                    failures.append(f'FAIL: {label} cannot be compared: {error}')
                    continue
                if difference is None:
                    print(f'{label} identical ({record_count(case, stream, base/"run1")})')
                else:
                    print(f'{label} first differs at {difference.index} ({difference.field})')
                failure = failure_of(label, difference, expected.get((pair, case, profile, stream)),
                                     maybe_expect is not None)
                if failure is not None:
                    failures.append(failure)
                elif difference is None:
                    identical += 1
                else:
                    recorded += 1
    for failure in failures:
        print(failure)
    if failures:
        print(f'FAIL: {len(failures)} of {total} streams are not '
              + ('as recorded' if maybe_expect is not None else 'identical'))
        return 1
    print(f'OK: {len(records[0]["cases"])} cases, {total} streams, {identical} identical, {recorded} as recorded')
    return 0
