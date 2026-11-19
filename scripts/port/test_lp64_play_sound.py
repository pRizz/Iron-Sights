"""ROM-free tests for lp64_play.py's `sound` set and `sound` command (LP64-005 Task 10), through
stand-in executables that print a log and write a recording made here from invented numbers to the
GE_AUDIO_CAPTURE path they are given."""
import contextlib
import io
import math
from pathlib import Path
import stat
import struct
import sys
import tempfile
import unittest
import wave

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_play  # noqa: E402

BOOT = 'ge007 pc port: booting (GE_FRAMES=n limits the run)\n'
VIDEO_UP = 'ge: video: window + GL context up\n'
RECORDING = 'ge: audio capture: recording /tmp/x/capture-title.wav\n'
FRAME_LIMIT = 'ge: frame limit reached: frames=1800 gfxtasks=900\n'


def health(skipped: int) -> str:
    """port/src/audio_pc.c's 10-second health line with SKIPPED refusals."""
    return (f'ge: audio: last 10 s: queue ran dry 3 times, 2 frames over the cap dropped, {skipped} frames skipped'
            ' as invalid | longest gap between pumps 900 ms | queue target 52 ms\n')


TITLE_GOOD = BOOT + VIDEO_UP + RECORDING + health(0) + health(0) + FRAME_LIMIT
DAM_GOOD = (BOOT + VIDEO_UP + RECORDING + 'ge: GE_STAGE direct launch: levelid=33 diff=0\n'
            'ge: load_bg_file levelid=33 stan=Tbg_dam_all_p_stanZ file=0x1 record=0x2 first=0x3\n' + health(0)
            + FRAME_LIMIT)
SKIPPED = 'ge: audio: WARNING - audio frame skipped (silent): command 15 op 0x04 has address 0xAAAC00000000\n'


def write_wave(path: Path, seconds: float, level: int, rate: int = 22050, channels: int = 2,
               every: int = 1) -> None:
    """A recording of a 441 Hz sine wave at LEVEL, every EVERY-th frame sounding and the rest zero."""
    frames = int(seconds * rate)
    data = bytearray()
    for index in range(frames):
        value = int(level * math.sin(math.tau * 441 * index / rate)) if index % every == 0 else 0
        data += struct.pack('<h', value) * channels
    with wave.open(str(path), 'wb') as recording:
        recording.setnchannels(channels)
        recording.setsampwidth(2)
        recording.setframerate(rate)
        recording.writeframes(bytes(data))


class StandIns(unittest.TestCase):
    """A temporary folder with a stand-in ROM, stand-in executables and an output folder."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.rom = self.folder/'rom.z64'
        self.rom.write_bytes(b'not a rom')
        self.output = self.folder/'out'

    def fake(self, log: str, recording: Path | None) -> Path:
        """A stand-in executable: copies RECORDING to $GE_AUDIO_CAPTURE when there is one, prints LOG."""
        index = len(list(self.folder.iterdir()))
        log_path = self.folder/f'canned{index}.log'
        log_path.write_text(log)
        copy = f'cp "{recording}" "$GE_AUDIO_CAPTURE"\n' if recording is not None else ''
        path = self.folder/f'IronSights{index}'
        path.write_text(f'#!/bin/sh\n{copy}cat "{log_path}" >&2\nexit 0\n')
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def wave(self, name: str, seconds: float = 12.0, level: int = 8000, **options) -> Path:
        path = self.folder/name
        write_wave(path, seconds, level, **options)
        return path

    def play(self, executable: Path, only: str, *extra: str) -> tuple[int, str]:
        return self.main('run', '--rom', str(self.rom), '--set', 'sound', '--only', only, '--jobs', '1',
                         *extra, '--output', str(self.output), str(executable))

    def main(self, *argv: str) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            try:
                status = lp64_play.main(list(argv))
            except SystemExit as stop:
                status = stop.code
        return status, out.getvalue()


class SoundSet(StandIns):
    def test_title_with_a_loud_recording_passes(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, self.wave('title.wav'))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 0, output)
        self.assertIn('capture-title: PASS', output)

    def test_dam_with_its_launch_lines_and_a_loud_recording_passes(self):
        # Arrange
        exe = self.fake(DAM_GOOD, self.wave('dam.wav'))

        # Act
        status, output = self.play(exe, 'capture-dam', '--video')

        # Assert
        self.assertEqual(status, 0, output)

    def test_a_skipped_audio_frame_fails(self):
        # Arrange
        exe = self.fake(TITLE_GOOD + SKIPPED, self.wave('title.wav'))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('audio frame skipped', output)

    def test_a_health_line_with_refused_audio_fails(self):
        # Arrange
        exe = self.fake(BOOT + VIDEO_UP + RECORDING + health(0) + health(12) + FRAME_LIMIT, self.wave('title.wav'))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('12 frames skipped as invalid', output)

    def test_a_log_without_a_health_line_fails(self):
        # Arrange
        exe = self.fake(BOOT + VIDEO_UP + RECORDING + FRAME_LIMIT, self.wave('title.wav'))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('missing line: an audio health line', output)

    def test_health_lines_that_all_report_no_refusal_pass(self):
        # Arrange
        exe = self.fake(BOOT + VIDEO_UP + RECORDING + health(0) + health(0) + health(0) + FRAME_LIMIT,
                        self.wave('title.wav'))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 0, output)

    def test_a_truncated_recording_is_measured_by_what_it_holds(self):
        # Arrange: the header claims 12 s, the file holds the first 4 s of it
        path = self.wave('title.wav')
        path.write_bytes(path.read_bytes()[:44 + 4 * 22050 * 4])
        exe = self.fake(TITLE_GOOD, path)

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('4.00 s recorded; expected at least 10 s', output)

    def test_an_empty_recording_fails(self):
        # Arrange
        path = self.folder/'title.wav'
        path.write_bytes(b'')
        exe = self.fake(TITLE_GOOD, path)

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('capture-title.wav is not a readable WAV file', output)

    def test_a_recording_that_is_not_a_wave_file_fails(self):
        # Arrange
        path = self.folder/'title.wav'
        path.write_bytes(b'OggS' + bytes(4000))
        exe = self.fake(TITLE_GOOD, path)

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('capture-title.wav is not a readable WAV file', output)

    def test_an_8_bit_recording_fails(self):
        # Arrange
        path = self.folder/'title.wav'
        with wave.open(str(path), 'wb') as recording:
            recording.setnchannels(2)
            recording.setsampwidth(1)
            recording.setframerate(22050)
            recording.writeframes(bytes([200, 56]) * 22050 * 12)
        exe = self.fake(TITLE_GOOD, path)

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('2 channels, 8-bit; expected 22050 Hz 16-bit stereo', output)

    def test_a_silent_recording_fails(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, self.wave('title.wav', level=0))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('peak 0; expected at least 1000', output)

    def test_a_quiet_recording_fails(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, self.wave('title.wav', level=999))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('expected at least 1000', output)

    def test_a_short_recording_fails(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, self.wave('title.wav', seconds=9.5))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('9.50 s recorded; expected at least 10 s', output)

    def test_a_mono_recording_fails(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, self.wave('title.wav', channels=1))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('expected 22050 Hz 16-bit stereo', output)

    def test_a_recording_at_another_rate_fails(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, self.wave('title.wav', rate=44100))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('44100 Hz', output)

    def test_no_recording_fails(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, None)

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('no recording at capture-title.wav', output)

    def test_a_case_without_the_capture_line_fails(self):
        # Arrange
        exe = self.fake(BOOT + VIDEO_UP + FRAME_LIMIT, self.wave('title.wav'))

        # Act
        status, output = self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('missing line: ge: audio capture: recording', output)

    def test_the_recording_is_kept_in_the_output_folder(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, self.wave('title.wav'))

        # Act
        self.play(exe, 'capture-title', '--video')

        # Assert
        self.assertTrue((self.output/'capture-title.wav').is_file())

    def test_the_set_needs_video(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, self.wave('title.wav'))

        # Act
        status, output = self.play(exe, 'capture-title')

        # Assert
        self.assertEqual(status, 2)
        self.assertIn('the sound set needs --video', output)

    def test_the_capture_path_is_refused_from_env(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, self.wave('title.wav'))

        # Act
        status, output = self.play(exe, 'capture-title', '--video', '--env', 'GE_AUDIO_CAPTURE=/tmp/x.wav')

        # Assert
        self.assertEqual(status, 2)
        self.assertIn('--env GE_AUDIO_CAPTURE is refused', output)

    def test_the_capture_path_is_refused_from_env_on_a_set_that_does_not_record(self):
        # Arrange
        exe = self.fake(TITLE_GOOD, None)

        # Act
        status, output = self.main('run', '--rom', str(self.rom), '--set', 'demos', '--only', 'dam1', '--env',
                                   'GE_AUDIO_CAPTURE=/tmp/x.wav', '--output', str(self.output), str(exe))

        # Assert
        self.assertEqual(status, 2)
        self.assertIn('--env GE_AUDIO_CAPTURE is refused', output)


class SoundReport(StandIns):
    def test_prints_length_peak_and_nonzero_share(self):
        # Arrange
        self.output.mkdir()
        write_wave(self.output/'capture-title.wav', 12.0, 8000, every=4)

        # Act
        status, output = self.main('sound', str(self.output))

        # Assert
        self.assertEqual(status, 0, output)
        self.assertRegex(output, r'capture-title: 12\.00 s, peak 79\d\d, non-zero 2\d\.\d% \(22050 Hz, 2 ch\)')

    def test_a_silent_recording_reports_peak_zero(self):
        # Arrange
        self.output.mkdir()
        write_wave(self.output/'capture-dam.wav', 11.0, 0)

        # Act
        status, output = self.main('sound', str(self.output))

        # Assert
        self.assertEqual(status, 0, output)
        self.assertIn('capture-dam: 11.00 s, peak 0, non-zero 0.0%', output)

    def test_a_recording_long_only_by_zero_padding_passes_and_reports_its_share(self):
        # Arrange: 1.2 s of sound and 10.8 s of zeros; the judge holds length and peak, not the share
        path = self.wave('title.wav', seconds=1.2)
        with wave.open(str(path), 'rb') as recording:
            sound = recording.readframes(recording.getnframes())
        with wave.open(str(path), 'wb') as recording:
            recording.setnchannels(2)
            recording.setsampwidth(2)
            recording.setframerate(22050)
            recording.writeframes(sound + bytes(4 * int(10.8 * 22050)))
        exe = self.fake(TITLE_GOOD, path)

        # Act
        status, output = self.play(exe, 'capture-title', '--video')
        report_status, report = self.main('sound', str(self.output))

        # Assert
        self.assertEqual((status, report_status), (0, 0), output + report)
        self.assertRegex(report, r'capture-title: 12\.00 s, peak \d+, non-zero (9\.\d|10\.0)%')

    def test_a_folder_without_recordings_fails(self):
        # Arrange
        self.output.mkdir()

        # Act
        status, output = self.main('sound', str(self.output))

        # Assert
        self.assertEqual(status, 2)
        self.assertIn('no recording', output)


if __name__ == '__main__':
    unittest.main()
