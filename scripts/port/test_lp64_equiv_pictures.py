"""ROM-free tests for lp64_equiv_pictures.py (LP64-006 Task 8): screenshots written by hand as small
BMP files in the folders `lp64_equiv.py run --draw --shots TICK` leaves, gallery hash lists written by
hand, and the gallery's capture variables read from hand_gallery.py."""
import contextlib
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hand_gallery  # noqa: E402
import lp64_equiv_pictures  # noqa: E402
import lp64_play_run  # noqa: E402

TICK = 1800
GREY = (128, 128, 128)


def bmp(width: int, height: int, pixels: dict[tuple[int, int], tuple[int, int, int]]) -> bytes:
    """A 24-bit bottom-up BMP as gfx_sdl2.cpp writes one, GREY but for PIXELS ((x, y) from the top: RGB)."""
    stride = (width * 3 + 3) & ~3
    rows = []
    for y in reversed(range(height)):
        row = b''.join(bytes(reversed(pixels.get((x, y), GREY))) for x in range(width))
        rows.append(row + b'\0' * (stride - width * 3))
    data = b''.join(rows)
    header = struct.pack('<2sIHHI', b'BM', 54 + len(data), 0, 0, 54)
    info = struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, len(data), 0, 0, 0, 0)
    return header + info + data


class Pictures(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)

    def tool(self, *argv: str) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            try:
                status = lp64_equiv_pictures.main(list(argv))
            except SystemExit as stop:
                status = stop.code
        return status, out.getvalue()

    def run_folder(self, label: str, shots: dict[str, bytes | None]) -> Path:
        """A run folder of `lp64_equiv.py run --draw --shots TICK`: each case's screenshot at TICK in
        run1/CASE.shots/ (none for a case given None) and the record naming the cases and TICK."""
        folder = self.folder/label
        for case, data in shots.items():
            (folder/'run1'/f'{case}.shots').mkdir(parents=True)
            if data is not None:
                (folder/'run1'/f'{case}.shots'/f'shot_{TICK:05d}.bmp').write_bytes(data)
        (folder/'equiv.json').write_text(json.dumps({'shots': TICK, 'cases': list(shots)}))
        return folder

    def gallery_file(self, label: str, changed: set[tuple[int, int]]) -> Path:
        """DIR/gallery.txt for every capture of the gallery, CHANGED ((case, sample)) given another hash."""
        folder = self.folder/label
        folder.mkdir()
        lines = [f'{case["id"]} {sample} {"f" * 64 if (case["id"], sample) in changed else "0" * 64} {case["file"]}'
                 for case in hand_gallery.cases() for sample in (1, 2)]
        (folder/lp64_equiv_pictures.GALLERY_FILE).write_text('\n'.join(lines) + '\n')
        return folder

    def test_shots_reports_pixel_count_and_largest_difference(self):
        # Arrange
        a = self.run_folder('i686', {'dam': bmp(4, 4, {}), 'facility': bmp(4, 4, {})})
        b = self.run_folder('x64', {'dam': bmp(4, 4, {(0, 0): (128, 131, 128), (3, 2): (120, 128, 128)}),
                                    'facility': bmp(4, 4, {})})

        # Act
        status, out = self.tool('shots', str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam: 2 of 16 pixels differ, largest channel difference 8', out)
        self.assertIn('facility: identical', out)

    def test_shots_of_identical_folders_pass(self):
        # Arrange
        a = self.run_folder('i686', {'dam': bmp(4, 4, {(1, 1): (1, 2, 3)})})
        b = self.run_folder('x64', {'dam': bmp(4, 4, {(1, 1): (1, 2, 3)})})

        # Act
        status, out = self.tool('shots', str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: 1 cases, 1 identical', out)

    def test_missing_shot_fails_not_differs(self):
        # Arrange
        a = self.run_folder('i686', {'dam': bmp(4, 4, {})})
        b = self.run_folder('x64', {'dam': None})

        # Act
        status, out = self.tool('shots', str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('dam: FAIL', out)
        self.assertIn('shot_01800.bmp', out)
        self.assertNotIn('pixels differ', out)

    def test_shots_fails_when_the_folders_ran_other_cases(self):
        # Arrange
        a = self.run_folder('i686', {'dam': bmp(4, 4, {})})
        b = self.run_folder('x64', {'dam': bmp(4, 4, {}), 'facility': bmp(4, 4, {})})

        # Act
        status, out = self.tool('shots', str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('FAIL', out)
        self.assertNotIn('identical', out)

    def test_shots_fails_when_the_folders_took_other_ticks(self):
        # Arrange
        a = self.run_folder('i686', {'dam': bmp(4, 4, {})})
        b = self.run_folder('x64', {'dam': bmp(4, 4, {})})
        (b/'equiv.json').write_text(json.dumps({'shots': TICK * 2, 'cases': ['dam']}))

        # Act
        status, out = self.tool('shots', str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn(f'--shots {TICK * 2}', out)
        self.assertNotIn('identical', out)

    def test_gallery_with_a_missing_rom_leaves_no_copy(self):
        # Arrange
        executable = self.folder/'IronSights'
        executable.write_bytes(b'not a game')
        output = self.folder/'gallery'

        # Act
        status, out = self.tool('gallery', '--rom', str(self.folder/'missing.z64'), '--output', str(output),
                                str(executable))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('missing.z64', out)
        self.assertFalse((output/'game'/executable.name).exists())

    def test_gallery_compare_counts_differing_cases(self):
        # Arrange
        a = self.gallery_file('i686', set())
        b = self.gallery_file('x64', {(0, 1), (0, 2), (107, 2)})

        # Act
        status, out = self.tool('gallery-compare', str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn(f'case 0 sample 1 ({hand_gallery.cases()[0]["file"]}) differs', out)
        self.assertIn('3 of 216 captures differ (2 of 108 cases)', out)

    def test_gallery_compare_fails_an_incomplete_list(self):
        # Arrange
        a = self.gallery_file('i686', set())
        b = self.gallery_file('x64', set())
        lines = (b/lp64_equiv_pictures.GALLERY_FILE).read_text().splitlines()
        (b/lp64_equiv_pictures.GALLERY_FILE).write_text('\n'.join(lines[:-1]) + '\n')

        # Act
        status, out = self.tool('gallery-compare', str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('215 of 216 captures', out)

    def test_gallery_environment_is_hand_gallerys_capture_mode(self):
        # Arrange
        rom, cache = self.folder/'rom.z64', self.folder/'icons'

        # Act
        env = lp64_equiv_pictures.capture_environment(rom, cache)

        # Assert
        self.assertEqual((env['GE_HAND_GALLERY'], env['GE_WINDOW'], env['GE_ROM'], env['GE_ICON_CACHE_DIR']),
                         ('1', '960x540', str(rom), str(cache)))
        self.assertEqual(env['SDL_VIDEODRIVER'], lp64_play_run.host_video_driver())

    def test_gallery_environment_takes_the_hosts_driver_and_hidden_window(self):
        # Arrange
        rom, cache = self.folder/'rom.z64', self.folder/'icons'
        mac = lp64_play_run.host_video_variables('darwin')
        linux = lp64_play_run.host_video_variables('linux')

        # Act
        with mock.patch.object(lp64_play_run, 'VIDEO_VARIABLES', mac):
            on_mac = lp64_equiv_pictures.capture_environment(rom, cache)
        with mock.patch.object(lp64_play_run, 'VIDEO_VARIABLES', linux):
            on_linux = lp64_equiv_pictures.capture_environment(rom, cache)

        # Assert: the gallery's own window size, never the play runs' 640x480
        self.assertEqual((on_mac['SDL_VIDEODRIVER'], on_mac['GE_WINDOW_HIDDEN'], on_mac['GE_WINDOW']),
                         ('cocoa', '1', '960x540'))
        self.assertEqual(on_linux['SDL_VIDEODRIVER'], 'offscreen')
        self.assertNotIn('GE_WINDOW_HIDDEN', on_linux)


if __name__ == '__main__':
    unittest.main()
