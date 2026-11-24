"""The comparing half of lp64_equiv.py (LP64-006 Task 5): a case's streams against themselves, two
seeds' streams, and run folders against each other under lp64_equiv_expected.txt.

A run folder is compared only when its record (equiv.json) shows two runs or more of every compared
case, each of which ran and agreed with itself; `compare` refuses any other folder (exit 2), an
equiv.json from before the verdicts were recorded included.

A stream's file sits beside CASE.log (STREAM_FILES). A demo's `sim` and `gfx` streams are its
playback: they are read as `sim-demo` and `gfx-demo` (lp64_equiv_streams.py), keyed by the
recording's tick and blind to the host's pace, though they are still printed as `sim` and `gfx`.

`snd` (Task 9) is judged only where the sound lock makes it the game's alone: a recording is compared
only when its case's log holds AUDIO_LOCKED (compare refuses the folder otherwise), and a demo's is
never judged (DEMO_SND_REASON): neither in its agreement with itself, nor between seeds, nor between
builds, where compare prints `CASE snd PAIR not judged: REASON` and leaves it out of the counts.

An `explained:` line may pin its stream (final review, I2): `sha256=HEX` after its index is the hash
of the newer build's stream as the comparison reads it (stream_sha256: a trace or digest by its
records, a demo's by its playback, a recording by its samples), so a pinned stream passes only while
everything after its first difference stays as it was; a line without the pin checks the first
difference alone. `pins` prints the file's explained lines with fresh hashes for the pairs given and
never writes the file: re-pinning is a deliberate edit. With --expect, compare also prints the lines
it did not exercise (a pair, profile, case or stream not compared), and it notes run folders whose
records name different commits.
"""
from dataclasses import dataclass, replace
import hashlib
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
# audio_pc.c, once, when GE_AUDIO_LOCK locks a comparison build's sound to game fields.
AUDIO_LOCKED = 'ge: audio: locked to game fields'
# Why a demo's recording is not judged (measured in Task 9: two runs of dam1 and of facility1 on
# cmp-a64-video and on cmp-i686-video, whose sim-demo and gfx-demo streams agreed, recorded
# different sound, facility1 at different lengths).
DEMO_SND_REASON = ("a paced demo: the host's pace sets its fields a recording tick, and the lock mixes one "
                   'audio frame a field, so two runs of one build record different sound')


def judged(case: str, stream: str) -> bool:
    """Whether CASE's STREAM is judged at all: not a demo's recording (DEMO_SND_REASON)."""
    return not (stream == 'snd' and is_demo(case))


def stream_kind(case: str, stream: str) -> str:
    """The reader kind of CASE's STREAM: `sim-demo` or `gfx-demo` for a demo's trace or digest."""
    return f'{stream}-demo' if stream in ('sim', 'gfx') and is_demo(case) else stream


def case_difference(case: str, stream: str, folder_a: Path, folder_b: Path) -> Difference | None:
    """The first difference of CASE's STREAM between two run folders, by its reader. Raises Unreadable
    for a missing, empty or torn file."""
    name = f'{case}{STREAM_FILES[stream]}'
    return first_difference(stream_kind(case, stream), folder_a/name, folder_b/name)


def unlocked(case: str, folder: Path) -> bool:
    """Whether CASE's log in FOLDER lacks AUDIO_LOCKED: its recording followed the host's clock."""
    log = folder/f'{case}.log'
    return not log.is_file() or AUDIO_LOCKED not in log.read_text(errors='replace').splitlines()


def self_disagreements(cases: list[str], output: Path, streams: tuple[str, ...], repeat: int) -> dict[str, str]:
    """For each case of CASES whose later runs in OUTPUT/runK differ from run 1 (or cannot be read),
    the line `FAIL: CASE STREAM differs from itself at I` naming its first such stream."""
    found = {}
    for case in cases:
        for stream in (stream for stream in streams if judged(case, stream)):
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
    for stream in (stream for stream in streams if judged(case, stream)):
        try:
            difference = case_difference(case, stream, folder_a, folder_b)
        except Unreadable as error:
            return False, f'FAIL: {stream} cannot be compared: {error}'
        (same if difference is None else differ).append(stream)
    if same:
        return False, f'FAIL: {joined(same)} {"is" if len(same) == 1 else "are"} the same between seeds'
    return True, f'OK: {joined(differ)} differ{"s" if len(differ) == 1 else ""} between seeds'


# ---- run folders against each other -------------------------------------------------------------
# A line of lp64_equiv_expected.txt: PAIR CASE PROFILE STREAM differs-from I [sha256=HEX] KIND: REASON,
# KIND one of EXPECT_KINDS; an `explained:` reason ends with `; removed-by: RUN`, and only an
# explained line carries the pin.
EXPECT_KINDS = ('explained', 'open', 'strict', 'host')
EXPECT_LINE = re.compile(r'^(\S+:\S+) (\S+) (\S+) (\S+) differs-from (\d+)(?: sha256=([0-9a-f]{64}))? ('
                         + '|'.join(EXPECT_KINDS) + r'): (.+)$')


@dataclass(frozen=True)
class Expected:
    index: int
    kind: str
    reason: str
    line: int = 0
    maybe_sha256: str | None = None


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
            raise ValueError(f'{path}:{number}: not PAIR CASE PROFILE STREAM differs-from I [sha256=HEX] KIND: REASON')
        pair, case, profile, stream, index, maybe_sha256, kind, reason = match.groups()
        if maybe_sha256 is not None and kind != 'explained':
            raise ValueError(f'{path}:{number}: only an explained line carries sha256=')
        if profile not in PROFILES or stream not in STREAM_FILES:
            raise ValueError(f'{path}:{number}: unknown profile {profile!r} or stream {stream!r}')
        if kind == 'explained' and '; removed-by: ' not in reason:
            raise ValueError(f'{path}:{number}: an explained difference names the run that removes it '
                             '(`; removed-by: RUN`)')
        key = (pair, case, profile, stream)
        if key in expected:
            raise ValueError(f'{path}:{number}: {" ".join(key)} is listed twice')
        expected[key] = Expected(int(index), kind, reason, number, maybe_sha256)
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


def stream_sha256(case: str, stream: str, folder: Path) -> str:
    """The SHA-256 of CASE's STREAM in FOLDER as the comparison reads it: a recording's samples, else one
    line a record (kind, then each field as name=value), so a demo's playback hashes without its pace."""
    path = folder/f'{case}{STREAM_FILES[stream]}'
    read = READERS[stream_kind(case, stream)](path)
    if stream == 'snd':
        return hashlib.sha256(read.pcm).hexdigest()
    digest = hashlib.sha256()
    for record in read:
        digest.update((' '.join([record.kind] + [f'{name}={value}' for name, value in record.fields]) + '\n')
                      .encode('ascii'))
    return digest.hexdigest()


def pin_failure(label: str, expected: Expected, maybe_expect: Path | None, case: str, stream: str,
                folder: Path) -> str | None:
    """Why a recorded difference's stream in FOLDER is not the one its line pins; None if it is or the
    line has no pin."""
    if expected.maybe_sha256 is None:
        return None
    found = stream_sha256(case, stream, folder)
    if found == expected.maybe_sha256:
        return None
    return (f'FAIL: {label} differs at {expected.index} as recorded, but its stream is not the one line '
            f'{expected.line} of {maybe_expect} pins (sha256 {found})')


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
    """The records of CASE's STREAM file in FOLDER (samples for a recording)."""
    return len(READERS[stream_kind(case, stream)](folder/f'{case}{STREAM_FILES[stream]}'))


def commit_note(folders: list[Path], records: list[dict]) -> str | None:
    """A line naming each folder's commit when the records name more than one (a record from before
    commits were recorded counts as `(not recorded)`), else None."""
    commits = [record.get('commit') or '(not recorded)' for record in records]
    if len(set(commits)) < 2:
        return None
    return 'note: the folders ran at different commits: ' + ', '.join(
        f'{folder.name} {commit}' for folder, commit in zip(folders, commits))


def unexercised_note(expected: dict[tuple[str, str, str, str], Expected], used: set) -> list[str]:
    """The lines that say how many of EXPECTED's lines this comparison did not reach, and which."""
    left = sorted((item.line, key) for key, item in expected.items() if key not in used)
    if not left:
        return []
    return ([f'note: {len(left)} of {len(expected)} expectation lines not exercised (a pair, profile, case '
             'or stream not compared here):'] + [f'  line {line}: {" ".join(key)}' for line, key in left])


def usable(folders: list[Path], streams: list[str], maybe_expect: Path | None) \
        -> tuple[list[dict], dict[tuple[str, str, str, str], Expected]] | None:
    """The folders' records and the expectations, or None after saying why the input is unusable."""
    try:
        records = [read_record(folder) for folder in folders]
        expected = read_expected(maybe_expect) if maybe_expect is not None else {}
    except ValueError as error:
        print(f'error: {error}', file=sys.stderr)
        return None
    for folder, record in zip(folders, records):
        reason = refusal(record, records[0]['cases'])
        if reason is not None:
            print(f'error: {folder} is refused: {reason}', file=sys.stderr)
            return None
    if any(record['profile'] != records[0]['profile'] for record in records):
        print('error: the run folders ran different profiles', file=sys.stderr)
        return None
    if any(stream in VIDEO_STREAMS for stream in streams) and not all(record['video'] for record in records):
        print('error: gfx and snd need run folders made with --video', file=sys.stderr)
        return None
    for folder in folders if 'snd' in streams else ():
        maybe_case = next((case for case in records[0]['cases']
                           if judged(case, 'snd') and unlocked(case, folder/'run1')), None)
        if maybe_case is not None:
            print(f'error: {folder} is refused: its {maybe_case}.wav was recorded without the sound lock '
                  f'({maybe_case}.log lacks {AUDIO_LOCKED})', file=sys.stderr)
            return None
    return records, expected


def compare_folders(folders: list[Path], streams: list[str], maybe_expect: Path | None) -> int:
    """`compare`: each later folder's run 1 against the first's, printing a line a case and stream;
    0 when everything is identical or as MAYBE_EXPECT records, 1 otherwise, 2 for unusable input."""
    found = usable(folders, streams, maybe_expect)
    if found is None:
        return 2
    records, expected = found
    base, profile = folders[0], records[0]['profile']
    maybe_note = commit_note(folders, records)
    if maybe_note is not None:
        print(maybe_note)
    total, identical, recorded, failures, used = 0, 0, 0, [], set()
    for folder in folders[1:]:
        pair = f'{base.name}:{folder.name}'
        for case in records[0]['cases']:
            for stream in streams:
                label = f'{case} {stream} {pair}'
                if not judged(case, stream):
                    print(f'{label} not judged: {DEMO_SND_REASON}')
                    continue
                total += 1
                try:
                    difference = case_difference(case, stream, base/'run1', folder/'run1')
                except Unreadable as error:
                    failures.append(f'FAIL: {label} cannot be compared: {error}')
                    continue
                if difference is None:
                    print(f'{label} identical ({record_count(case, stream, base/"run1")})')
                else:
                    print(f'{label} first differs at {difference.index} ({difference.field})')
                key = (pair, case, profile, stream)
                used.add(key)
                failure = failure_of(label, difference, expected.get(key), maybe_expect is not None)
                if failure is None and difference is not None and key in expected:
                    failure = pin_failure(label, expected[key], maybe_expect, case, stream, folder/'run1')
                if failure is not None:
                    failures.append(failure)
                elif difference is None:
                    identical += 1
                else:
                    recorded += 1
    for line in unexercised_note(expected, used):
        print(line)
    for failure in failures:
        print(failure)
    if failures:
        print(f'FAIL: {len(failures)} of {total} streams are not '
              + ('as recorded' if maybe_expect is not None else 'identical'))
        return 1
    print(f'OK: {len(records[0]["cases"])} cases, {total} streams, {identical} identical, {recorded} as recorded')
    return 0


def pin_lines(folders: list[Path], streams: list[str], expect: Path) -> int:
    """`pins`: prints each explained line of EXPECT whose pair, profile, case and stream these folders
    compare, with `sha256=` of the newer folder's stream as it is now; it never writes EXPECT. 0, or 2
    for unusable input."""
    found = usable(folders, streams, expect)
    if found is None:
        return 2
    records, expected = found
    base, profile, printed = folders[0], records[0]['profile'], 0
    for folder in folders[1:]:
        pair = f'{base.name}:{folder.name}'
        for case in records[0]['cases']:
            for stream in streams:
                item = expected.get((pair, case, profile, stream))
                if item is None or item.kind != 'explained' or not judged(case, stream):
                    continue
                fresh = replace(item, maybe_sha256=stream_sha256(case, stream, folder/'run1'))
                print(f'{pair} {case} {profile} {stream} differs-from {fresh.index} sha256={fresh.maybe_sha256} '
                      f'{fresh.kind}: {fresh.reason}')
                printed += 1
    print(f'# {printed} explained lines pinned from {", ".join(folder.name for folder in folders)}; '
          f'{expect} is unchanged', file=sys.stderr)
    return 0
