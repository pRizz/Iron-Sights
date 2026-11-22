"""Readers of the streams LP64-006 compares between builds, and the first
difference between two files of one stream.

`sim` (Task 3) is the simulation trace port/src/port_simtrace.c writes with
GE_SIMTRACE (port/include/port_simtrace.h has its format): a `T` line a tick,
and with GE_SIMTRACE_DETAIL `G` (guard) and `M` (moving prop) lines after the
T line of their tick. While a recording replays, a T line ends with `demo=K`, the
recording's tick (Task 5).

`sim-demo` is a demo's stream: only the T lines that carry `demo=`, keyed by K
and compared on every field but `n` and `timer`, which a paced run's host sets.
A trace with no such line is unreadable, and a K repeated in one file cannot be
keyed. Playbacks of unequal length differ at the first K one of them lacks
(field `demo`).

`gfx` (Task 7) is the picture digest port/src/port_gfx_digest.cpp writes with
GE_GFXDIGEST (port/include/port_gfx_digest.h has its format): an `F` line a
frame, and with GE_GFXDIGEST_DETAIL a `C` line a call before the F line of its
frame. A difference names the frame (an F line's n) and the field (`C.` before
a call line's). `gfx-demo` is a demo's picture stream as `sim-demo` is its
simulation's: the F lines that carry `demo=`, keyed by K and compared on every
field but `n` and `tick`. Measured on dam1 (cmp-a64-video, two paced runs):
one F line for each K.

`snd` (Task 9) is the mixed sound GE_AUDIO_CAPTURE records
(port/src/port_audio_capture.c): a 16-bit PCM WAV, 22,050 Hz stereo. Under
the sound lock (port_audio_lock.h) frame k of FRAME_SAMPLES samples (from 0)
is the one mixed for field k + 1 counted from the lock's first pump. A
difference names the first differing sample (its index from 0 is the index),
its channel, frame and field (`R frame=1 field=2`); its line is the frame
from 1. A file that is not such a WAV is unreadable; two files whose common
part is equal but whose lengths differ differ at the shorter one's end (field
`length`, values the sample counts).

A file with no line, a last line without a newline, a line that does not
parse, or a file that ends where the other goes on is neither equal nor
different: it raises Unreadable.

    python3 scripts/port/lp64_equiv_streams.py sim|sim-demo|gfx|gfx-demo|snd FILE_A FILE_B

prints `identical: N lines` (for snd, `N samples`; exit 0) or the first difference (exit 1); an
unreadable file exits 2.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import struct
import sys

HEX8 = r'[0-9a-f]{8}'
FLOATS3 = rf'{HEX8},{HEX8},{HEX8}'
SUM = rf'\d+:{HEX8}'

# Each record kind's fields in order: (name, value pattern). A T line's
# player fields (pI) sit between `players` and `chrs`, one for each player.
T_HEAD = (('n', r'\d+'), ('timer', r'-?\d+'), ('seed', r'[0-9a-f]{16}'), ('draws', r'-?\d+'),
          ('players', r'[0-4]'))
T_TAIL = (('chrs', SUM), ('props', SUM), ('rooms', SUM))
# Last on a T line, only while a recording replays: the recording's tick.
DEMO = ('demo', r'\d+')
# The fields a demo's stream leaves out: the host's pace sets them.
DEMO_PACED = ('n', 'timer')
PLAYER = re.compile(rf'p[0-3]={FLOATS3},{HEX8},{HEX8}')
# A picture digest's frame line and call line.
F_FIELDS = (('n', r'\d+'), ('tick', r'-?\d+'), ('draws', r'\d+'), ('tris', r'\d+'), ('vtx', HEX8),
            ('tex', SUM), ('state', HEX8), ('post', HEX8))
C_FIELDS = (('n', r'\d+'), ('k', r'\d+'), ('entry', r'[a-z0-9_]+'), ('part', r'vtx|tex|state|post|none'),
            ('hash', HEX8))
# The fields a demo's picture stream leaves out: the host's pace sets them.
GFX_PACED = ('n', 'tick')
# The record kinds a stream counts by; others (detail) carry the count of the one they belong to.
MAIN_KINDS = ('T', 'F')
DETAIL = {
    'G': (('n', r'\d+'), ('slot', r'\d+'), ('chrnum', r'-?\d+'), ('action', r'\d+'), ('flags', HEX8),
          ('pos', FLOATS3), ('damage', HEX8)),
    'M': (('n', r'\d+'), ('index', r'\d+'), ('slot', r'\d+'), ('type', r'\d+'), ('flags', r'[0-9a-f]{2}'),
          ('pos', FLOATS3)),
}


class Unreadable(Exception):
    """A stream file that is missing, empty, torn or malformed."""


@dataclass(frozen=True)
class Difference:
    """The first place two files of a stream differ: `index` is the tick (a
    sim line's n), `field` the field's name (`G.` or `M.` before a detail
    line's), `a` and `b` its values (each `name=value` when the two files
    hold different fields there, as `p0` against `p1`), `line` the 1-based
    line number."""
    index: int
    field: str
    a: str
    b: str
    line: int


@dataclass(frozen=True)
class Record:
    kind: str
    tick: int
    fields: tuple[tuple[str, str], ...]


def _fields(path: Path, number: int, tokens: list[str], expected: tuple[tuple[str, str], ...]) \
        -> list[tuple[str, str]]:
    if len(tokens) != len(expected):
        raise Unreadable(f'{path}:{number}: {len(tokens)} fields where {len(expected)} were expected')
    fields = []
    for token, (name, pattern) in zip(tokens, expected):
        key, sep, value = token.partition('=')
        if key != name or not sep or not re.fullmatch(pattern, value):
            raise Unreadable(f'{path}:{number}: `{token}` is not {name}={pattern}')
        fields.append((key, value))
    return fields


def _sim_record(path: Path, number: int, text: str) -> Record:
    kind, *tokens = text.split(' ')
    if kind in DETAIL:
        fields = _fields(path, number, tokens, DETAIL[kind])
        return Record(kind, int(fields[0][1]), tuple(fields))
    if kind != 'T' or len(tokens) < len(T_HEAD) + len(T_TAIL):
        raise Unreadable(f'{path}:{number}: not a simulation trace line')
    head = _fields(path, number, tokens[:len(T_HEAD)], T_HEAD)
    demo = []
    if tokens[-1].startswith('demo='):
        demo = _fields(path, number, tokens[-1:], (DEMO,))
        tokens = tokens[:-1]
    players = tokens[len(T_HEAD):-len(T_TAIL)]
    if len(players) != int(head[-1][1]) or not all(PLAYER.fullmatch(p) for p in players):
        raise Unreadable(f'{path}:{number}: players={head[-1][1]} but its player fields read {players}')
    tail = _fields(path, number, tokens[-len(T_TAIL):], T_TAIL)
    return Record('T', int(head[0][1]), tuple(head + [tuple(p.split('=', 1)) for p in players] + tail + demo))


def _lines(path: Path) -> list[str]:
    """A stream file's lines, raising Unreadable for a missing, empty, torn or non-ASCII file."""
    try:
        data = Path(path).read_bytes()
    except OSError as error:
        raise Unreadable(f'{path}: {error.strerror}') from error
    if not data:
        raise Unreadable(f'{path}: no line')
    if not data.endswith(b'\n'):
        raise Unreadable(f'{path}: the last line has no newline')
    try:
        lines = data.decode('ascii').split('\n')[:-1]
    except UnicodeDecodeError as error:
        raise Unreadable(f'{path}: not ASCII') from error
    return lines


def read_sim(path: Path) -> list[Record]:
    """Every record of a simulation trace, checked: T lines count from 1 and
    each detail line belongs to the T line before it."""
    records = []
    ticks = 0
    for number, text in enumerate(_lines(path), start=1):
        record = _sim_record(Path(path), number, text)
        if record.kind == 'T':
            ticks += 1
        if record.tick != ticks:
            raise Unreadable(f'{path}:{number}: n={record.tick} after {ticks} T lines')
        records.append(record)
    return records


def _gfx_record(path: Path, number: int, text: str) -> Record:
    kind, *tokens = text.split(' ')
    if kind == 'C':
        fields = _fields(path, number, tokens, C_FIELDS)
        return Record('C', int(fields[0][1]), tuple(fields))
    if kind != 'F':
        raise Unreadable(f'{path}:{number}: not a picture digest line')
    demo = []
    if tokens and tokens[-1].startswith('demo='):
        demo = _fields(path, number, tokens[-1:], (DEMO,))
        tokens = tokens[:-1]
    fields = _fields(path, number, tokens, F_FIELDS)
    return Record('F', int(fields[0][1]), tuple(fields + demo))


def read_gfx(path: Path) -> list[Record]:
    """Every record of a picture digest, checked: F lines count from 1 and each
    call line belongs to the F line after it."""
    records = []
    frames = 0
    for number, text in enumerate(_lines(path), start=1):
        record = _gfx_record(Path(path), number, text)
        if record.tick != frames + 1:
            raise Unreadable(f'{path}:{number}: n={record.tick} after {frames} F lines')
        frames += record.kind == 'F'
        records.append(record)
    return records


# Each demo kind: the reader of its file, the kind of line a replay marks and the fields its host's
# pace sets.
DEMO_KINDS = {'sim-demo': (read_sim, 'T', DEMO_PACED), 'gfx-demo': (read_gfx, 'F', GFX_PACED)}


def _playback(path: Path, kind: str = 'sim-demo') -> dict[int, tuple[int, Record]]:
    """The replay's lines of a demo stream of KIND by their demo tick, each with its line number, as
    records of kind D without the paced fields."""
    read, marked, paced = DEMO_KINDS[kind]
    playback: dict[int, tuple[int, Record]] = {}
    number = 0
    for record in read(path):
        number += 1
        fields = dict(record.fields)
        if record.kind != marked or 'demo' not in fields:
            continue
        tick = int(fields['demo'])
        if tick in playback:
            raise Unreadable(f'{path}:{number}: demo={tick} again (first on line {playback[tick][0]})')
        kept = tuple((name, value) for name, value in record.fields if name not in paced + ('demo',))
        playback[tick] = (number, Record('D', tick, kept))
    if not playback:
        raise Unreadable(f'{path}: no line of a replay (demo=)')
    return playback


def read_sim_demo(path: Path) -> list[Record]:
    """A demo's stream: its replay's records in demo-tick order."""
    return [record for _, (_, record) in sorted(_playback(path).items())]


def read_gfx_demo(path: Path) -> list[Record]:
    """A demo's picture stream: its replay's frames in demo-tick order."""
    return [record for _, (_, record) in sorted(_playback(path, 'gfx-demo').items())]


def _demo_difference(kind: str, path_a: Path, path_b: Path) -> Difference | None:
    a, b = _playback(path_a, kind), _playback(path_b, kind)
    for tick in sorted(a.keys() | b.keys()):
        if tick not in a or tick not in b:
            line = (a.get(tick) or b[tick])[0]
            return Difference(tick, 'demo', str(tick) if tick in a else '(none)',
                              str(tick) if tick in b else '(none)', line)
        (line, ra), (_, rb) = a[tick], b[tick]
        for (name, va), (nb, vb) in zip(ra.fields, rb.fields):
            if name != nb:
                return Difference(tick, name, f'{name}={va}', f'{nb}={vb}', line)
            if va != vb:
                return Difference(tick, name, va, vb, line)
        if len(ra.fields) != len(rb.fields):
            return Difference(tick, 'fields', str(len(ra.fields)), str(len(rb.fields)), line)
    return None


# A recording's format (port_audio_capture.c) and the samples of one audio frame: audi.c's
# g_FrameSize at 60 fields a second, 367.5 rounded up to a multiple of 16, which the lock mixes.
SND_RATE = 22050
SND_CHANNELS = 2
FRAME_SAMPLES = 368
SND_BLOCK = 1 << 16


@dataclass(frozen=True)
class Recording:
    """A recording's PCM data, four bytes a stereo sample (left then right, little-endian)."""
    pcm: bytes

    def __len__(self) -> int:
        return len(self.pcm) // 4


def read_snd(path: Path) -> Recording:
    """A recording, checked: RIFF WAVE, PCM 16-bit 22,050 Hz stereo, and a data chunk whose
    declared size is the bytes the file holds, a whole number of samples and at least one."""
    try:
        data = Path(path).read_bytes()
    except OSError as error:
        raise Unreadable(f'{path}: {error.strerror}') from error
    if len(data) < 12 or data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise Unreadable(f'{path}: not a RIFF WAVE file')
    offset, maybe_format, maybe_pcm = 12, None, None
    while offset + 8 <= len(data) and maybe_pcm is None:
        name, size = data[offset:offset + 4], struct.unpack_from('<I', data, offset + 4)[0]
        body = data[offset + 8:offset + 8 + size]
        if name == b'fmt ' and size >= 16:
            maybe_format = struct.unpack_from('<HHIIHH', body)
        elif name == b'data':
            if len(body) != size:
                raise Unreadable(f'{path}: torn: its data chunk declares {size} bytes and holds {len(body)}')
            maybe_pcm = body
        offset += 8 + size + (size & 1)
    if maybe_format is None or maybe_pcm is None:
        raise Unreadable(f'{path}: no fmt or data chunk')
    tag, channels, rate, _, _, bits = maybe_format
    if (tag, channels, rate, bits) != (1, SND_CHANNELS, SND_RATE, 16):
        raise Unreadable(f'{path}: not 16-bit PCM 22,050 Hz stereo (format {tag}, {channels} channels, '
                         f'{rate} Hz, {bits} bits)')
    if not maybe_pcm or len(maybe_pcm) % 4:
        raise Unreadable(f'{path}: {len(maybe_pcm)} bytes of data, not a whole number of samples (one or more)')
    return Recording(maybe_pcm)


def _snd_difference(path_a: Path, path_b: Path) -> Difference | None:
    a, b = read_snd(path_a).pcm, read_snd(path_b).pcm
    common = min(len(a), len(b))
    offset = 0
    while offset < common and a[offset:offset + SND_BLOCK] == b[offset:offset + SND_BLOCK]:
        offset += SND_BLOCK
    offset = next((i for i in range(offset, min(offset + SND_BLOCK, common)) if a[i] != b[i]), common)
    sample = offset // 4
    frame = sample // FRAME_SAMPLES
    if offset < common:
        right = offset % 4 >= 2
        channel = 4 * sample + (2 if right else 0)
        value_a, value_b = (struct.unpack_from('<h', pcm, channel)[0] for pcm in (a, b))
        return Difference(sample, f'{"R" if right else "L"} frame={frame} field={frame + 1}',
                          str(value_a), str(value_b), frame + 1)
    if len(a) != len(b):
        return Difference(sample, 'length', str(len(a) // 4), str(len(b) // 4), frame + 1)
    return None


READERS = {'sim': read_sim, 'sim-demo': read_sim_demo, 'gfx': read_gfx, 'gfx-demo': read_gfx_demo,
           'snd': read_snd}


def first_difference(kind: str, path_a: Path, path_b: Path) -> Difference | None:
    """The first difference between two files of stream KIND, or None when
    they hold the same records. Raises Unreadable as the module says."""
    if kind not in READERS:
        raise ValueError(f'unknown stream {kind!r}; known: {", ".join(READERS)}')
    if kind in DEMO_KINDS:
        return _demo_difference(kind, path_a, path_b)
    if kind == 'snd':
        return _snd_difference(path_a, path_b)
    a = READERS[kind](path_a)
    b = READERS[kind](path_b)
    for number, (ra, rb) in enumerate(zip(a, b), start=1):
        if ra == rb:
            continue
        prefix = '' if ra.kind in MAIN_KINDS else f'{ra.kind}.'
        if ra.kind != rb.kind:
            return Difference(ra.tick, 'kind', ra.kind, rb.kind, number)
        for (name, va), (nb, vb) in zip(ra.fields, rb.fields):
            if name != nb:  # a player in another slot: p0 against p1
                return Difference(ra.tick, prefix + name, f'{name}={va}', f'{nb}={vb}', number)
            if va != vb:
                return Difference(ra.tick, prefix + name, va, vb, number)
        # Every shared field is equal, so one record has more fields than the
        # other. The reader ties a T line's pI fields to its `players` value,
        # so this needs records the reader did not make; it is still a
        # difference, named by the first field only one side has.
        longer, side = (ra, 'a') if len(ra.fields) > len(rb.fields) else (rb, 'b')
        name, value = longer.fields[min(len(ra.fields), len(rb.fields))]
        extra = f'{name}={value}'
        return Difference(ra.tick, prefix + name, extra if side == 'a' else '(none)',
                          extra if side == 'b' else '(none)', number)
    if len(a) != len(b):
        short = path_a if len(a) < len(b) else path_b
        raise Unreadable(f'{short} is short: {min(len(a), len(b))} of {max(len(a), len(b))} lines')
    return None


def main(argv: list[str]) -> int:
    if len(argv) != 3 or argv[0] not in READERS:
        print(f'usage: lp64_equiv_streams.py {{{"|".join(READERS)}}} FILE_A FILE_B', file=sys.stderr)
        return 2
    kind, path_a, path_b = argv
    try:
        difference = first_difference(kind, Path(path_a), Path(path_b))
        count = len(READERS[kind](Path(path_a)))
    except Unreadable as error:
        print(f'unreadable: {error}')
        return 2
    if difference is None:
        print(f'identical: {count} {"samples" if kind == "snd" else "lines"}')
        return 0
    where = (f'sample {difference.index}' if kind == 'snd'
             else f'line {difference.line} tick {difference.index}')
    print(f'differs: {where} {difference.field}: {difference.a} != {difference.b}')
    return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
