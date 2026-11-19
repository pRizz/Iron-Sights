"""The `sound` set of lp64_play.py and what its recordings must hold (LP64-005 Task 10).

Each case runs a video executable with GE_AUDIO_CAPTURE naming DIR/CASE.wav, the mixed output
that port/src/port_audio_capture.c records before the unfocused-window mute, whether or not a device
plays it (the play tool's SDL_AUDIODRIVER=dummy opens SDL's dummy device, which takes queued audio
at the device's rate). A case fails on any `audio frame skipped` line (port/src/audio_pc.c drops such
a frame as silence and prints the line once), on a 10-second health line that counts a refusal (every
refused frame, ADPCM block header and resample span), and on a recording that is not 22,050 Hz 16-bit stereo,
lasts less than 10 seconds or peaks below 1,000: the planning run's recording of Dam had a peak of 0.
A health line is printed only for a window of 10 s or more with a dry queue, a frame over the cap or a
refusal, so a log need not have one (a healthy run on a fast host prints none), and a block-header or
resample refusal in the run's last window of under 10 s is seen by nothing.
`lp64_play.py sound DIR` prints each recording's length, peak and share of non-zero samples, which
are compared with the 32-bit reference's recordings rather than held to a floor. Recordings are
ROM-derived: they stay in the output folder and are never committed.
"""
from array import array
from dataclasses import dataclass
from pathlib import Path
import re
import sys
import wave

from lp64_play_cases import LAUNCHED_STAGE, Case, Required, frame_limit

SOUND_FRAMES = 1800
SAMPLE_RATE = 22050  # port/src/audio_pc.c, PORT_AUDIO_RATE
CHANNELS = 2
SAMPLE_BYTES = 2
MIN_SECONDS = 10.0
MIN_PEAK = 1000
# port/src/audio_pc.c, portAcmdValidate: "ge: audio: WARNING - audio frame skipped (silent): ..."
FRAME_SKIPPED = re.compile(r'audio frame skipped')
# port/src/audio_pc.c, portAudioHealth: "ge: audio: last %u s: queue ran dry %u times, %u frames over the
# cap dropped, %u frames skipped as invalid | ..."; the last count is s_StatSkipped.
HEALTH_REFUSED = re.compile(r'ge: audio: last \d+ s: .*, [1-9]\d* frames skipped as invalid')
# port/src/port_audio_capture.c, captureOpen: "ge: audio capture: recording %s\n"
CAPTURE_OPEN = re.compile(r'ge: audio capture: recording ')


@dataclass(frozen=True)
class Recording:
    """What a capture holds: its format and, over every sample of both channels, the largest
    magnitude and the share that are not zero."""
    rate: int
    channels: int
    sample_bytes: int
    seconds: float
    peak: int
    nonzero_share: float


def read_recording(path: Path) -> Recording:
    """The figures of the WAV file at PATH; raises ValueError when it cannot be read as one."""
    try:
        with wave.open(str(path), 'rb') as recording:
            rate, channels = recording.getframerate(), recording.getnchannels()
            sample_bytes, frames = recording.getsampwidth(), recording.getnframes()
            data = recording.readframes(frames)
    except (OSError, EOFError, wave.Error) as error:
        raise ValueError(f'{path.name} is not a readable WAV file: {error}') from error
    # The bytes read, not the header's frame count: a truncated file must not pass on its header's claim.
    seconds = len(data) / (rate * channels * sample_bytes) if rate and channels and sample_bytes else 0.0
    if sample_bytes != SAMPLE_BYTES:
        return Recording(rate, channels, sample_bytes, seconds, 0, 0.0)
    samples = array('h')
    samples.frombytes(data[:len(data) - len(data) % SAMPLE_BYTES])
    if sys.byteorder == 'big':
        samples.byteswap()  # WAV samples are little-endian
    peak = max((abs(sample) for sample in samples), default=0)
    share = (len(samples) - samples.count(0)) / len(samples) if samples else 0.0
    return Recording(rate, channels, sample_bytes, seconds, peak, share)


def capture_path(output: Path, case: Case) -> Path:
    """Where CASE records: OUTPUT/CASE.wav."""
    return (output/f'{case.name}.wav').resolve()


def capture_problems(path: Path) -> list[str]:
    """Why the recording at PATH does not show sound running, or nothing when it does."""
    if not path.is_file():
        return [f'no recording at {path.name}']
    try:
        recording = read_recording(path)
    except ValueError as error:
        return [str(error)]
    found = []
    if (recording.rate, recording.channels, recording.sample_bytes) != (SAMPLE_RATE, CHANNELS, SAMPLE_BYTES):
        found.append(f'{path.name}: {recording.rate} Hz, {recording.channels} channels, '
                     f'{recording.sample_bytes * 8}-bit; expected {SAMPLE_RATE} Hz 16-bit stereo')
    if recording.seconds < MIN_SECONDS:
        found.append(f'{path.name}: {recording.seconds:.2f} s recorded; expected at least {MIN_SECONDS:.0f} s')
    if recording.peak < MIN_PEAK:
        found.append(f'{path.name}: peak {recording.peak}; expected at least {MIN_PEAK}')
    return found


def describe(name: str, recording: Recording) -> str:
    """One line of `lp64_play.py sound`: NAME: SECONDS s, peak PEAK, non-zero SHARE%."""
    return (f'{name}: {recording.seconds:.2f} s, peak {recording.peak}, '
            f'non-zero {recording.nonzero_share * 100:.1f}% ({recording.rate} Hz, {recording.channels} ch)')


def sound_report(folder: Path) -> int:
    """Prints a line for each recording in FOLDER (its *.wav files, by name); 0 when every one was
    read, 2 when there is none or one cannot be read."""
    paths = sorted(folder.glob('*.wav'))
    if not paths:
        print(f'error: no recording (*.wav) in {folder}', file=sys.stderr)
        return 2
    status = 0
    for path in paths:
        try:
            print(describe(path.stem, read_recording(path)))
        except ValueError as error:
            print(f'error: {error}', file=sys.stderr)
            status = 2
    return status


def sound_case(name: str, env: tuple[tuple[str, str], ...], launched: tuple[Required, ...]) -> Case:
    required = (Required('ge: audio capture: recording', CAPTURE_OPEN),) + launched + (frame_limit(SOUND_FRAMES),)
    return Case(name, 'sound', env, SOUND_FRAMES, required, forbidden=(FRAME_SKIPPED, HEALTH_REFUSED), capture=True)


def sound_cases() -> list[Case]:
    """The title with no launch variable, and Dam."""
    return [
        sound_case('capture-title', (), ()),
        sound_case('capture-dam', (('GE_STAGE', 'dam'), ('GE_SKIPINTRO', '1')), LAUNCHED_STAGE),
    ]
