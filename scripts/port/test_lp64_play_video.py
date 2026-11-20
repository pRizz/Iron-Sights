"""ROM-free tests for lp64_play.py run --video (LP64-005 Task 9): the GL line and the pictures a
video case must show, through the stand-in executables of test_lp64_play.py. Moved here from
test_lp64_play.py to keep that file under the plan's 500 lines."""
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_lp64_play import (BOOT, DAM1_START, DAM1_VIDEO, STOP_CITADEL, VIDEO_UP, StandIns,  # noqa: E402
                            demo_exit)


class Video(StandIns):
    def test_video_case_with_the_gl_line_and_its_pictures_passes(self):
        # Arrange
        exe = self.fake(DAM1_VIDEO + 'ge: frame limit reached: frames=3000 gfxtasks=30\n')

        # Act
        status, out = self.play(exe, 'demos', 'dam1', '--video')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('dam1: PASS', out)

    def test_video_case_with_fewer_pictures_than_required_fails(self):
        # Arrange
        exe = self.fake(DAM1_VIDEO + 'ge: frame limit reached: frames=3000 gfxtasks=3\n')

        # Act
        status, out = self.play(exe, 'demos', 'dam1', '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (ge: frame limit reached: frames=3000 gfxtasks=3: expected at least 30)', out)

    def test_video_case_without_the_gl_line_fails(self):
        # Arrange
        exe = self.fake(BOOT + DAM1_START + demo_exit(438) + 'ge: frame limit reached: frames=3000 gfxtasks=900\n')

        # Act
        status, out = self.play(exe, 'demos', 'dam1', '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (missing line: ge: video: window + GL context up)', out)

    def test_video_case_whose_frame_limit_line_lacks_gfxtasks_fails(self):
        # Arrange
        exe = self.fake(DAM1_VIDEO + 'ge: frame limit reached: frames=3000\n')

        # Act
        status, out = self.play(exe, 'demos', 'dam1', '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (missing line: ge: frame ... gfxtasks=N (the frame-limit line or the heartbeat))', out)

    def test_video_case_without_a_frame_limit_line_fails(self):
        # Arrange
        exe = self.fake(DAM1_VIDEO)

        # Act
        status, out = self.play(exe, 'demos', 'dam1', '--video')

        # Assert
        problems = json.loads((self.output/'play.json').read_text())['cases'][0]['problems']
        self.assertEqual(status, 1, out)
        self.assertIn('missing line: ge: frame ... gfxtasks=N (the frame-limit line or the heartbeat)', problems)

    def test_video_stop_with_the_gl_line_and_its_fatal_line_passes_without_a_frame_limit(self):
        # Arrange: a stop ends at its FATAL line, before any frame-limit line could name its pictures
        exe = self.fake(BOOT + VIDEO_UP + STOP_CITADEL, status=1)

        # Act
        status, out = self.play(exe, 'stops', 'citadel', '--video')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('stop-citadel: PASS', out)

    def test_video_case_that_ends_itself_counts_pictures_from_the_heartbeat(self):
        # Arrange: the soak exits after its lap, before any frame-limit line
        exe = self.fake(BOOT + VIDEO_UP + 'ge: frame 13620 | gfx tasks submitted 13490\n'
                        'ge: frame 13680 | gfx tasks submitted 13547\nge: soak: complete after 1 laps\n')

        # Act
        status, out = self.play(exe, 'attract', 'soak', '--video')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('soak: PASS (13547 pictures)', out)

    def test_video_stop_without_the_gl_line_fails(self):
        # Arrange
        exe = self.fake(BOOT + STOP_CITADEL, status=1)

        # Act
        status, out = self.play(exe, 'stops', 'citadel', '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stop-citadel: FAIL (missing line: ge: video: window + GL context up)', out)

    def test_headless_case_needs_neither_video_line(self):
        # Arrange
        exe = self.fake(BOOT + DAM1_START + demo_exit(438) + 'ge: frame limit reached: frames=3000 gfxtasks=3\n')

        # Act
        status, out = self.play(exe, 'demos', 'dam1')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('dam1: PASS', out)


if __name__ == '__main__':
    unittest.main()
