#!/usr/bin/env python3
"""Checks for the hit-sound capture measurements."""
import unittest

from hit_sound_capture import Hit, Level, evaluate, parse_hits

LOG = """ge: audio capture: recording capture.wav
ge: hitprobe hit=0 metres=1.0 hit_sounds=0 native=32767 frame=100
ge: hitprobe hit=1 metres=40.0 hit_sounds=1 native=12510 frame=22150
"""


def level(metres, enabled, native, peak):
    return Level(Hit(metres, enabled, native, 0), peak)


class HitSoundCaptureTests(unittest.TestCase):
    def test_parses_probe_lines_only(self):
        self.assertEqual(parse_hits(LOG), [Hit(1.0, False, 32767, 100), Hit(40.0, True, 12510, 22150)])

    def test_accepts_raised_hits_and_faded_ones(self):
        levels = [level(1, False, 32767, 11500), level(40, False, 12510, 2000), level(40, True, 12510, 11300)]
        self.assertEqual(evaluate(levels), [])

    def test_rejects_a_raised_hit_short_of_point_blank(self):
        levels = [level(1, False, 32767, 11500), level(40, True, 12510, 7100)]
        self.assertEqual(evaluate(levels), ["40 m with Hit Sounds on peaks at 62% of point blank"])

    def test_rejects_a_far_hit_that_no_longer_fades(self):
        levels = [level(1, False, 32767, 11500), level(40, False, 12510, 11400), level(40, True, 12510, 11400)]
        self.assertEqual(evaluate(levels), ["40 m with Hit Sounds off did not fade (99% of point blank)"])

    def test_needs_both_settings(self):
        self.assertEqual(evaluate([level(1, False, 32767, 11500)]),
                         ["the capture needs hits with Hit Sounds both off and on"])


if __name__ == "__main__":
    unittest.main()
