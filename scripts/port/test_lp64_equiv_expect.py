"""ROM-free tests of lp64_equiv.py compare's hold on its expectation file (LP64-006 final review): an
`explained:` line's pins (`sha256=` of a video run's stream, `headless-sha256=` of a headless run's), the
`pins` command that prints fresh ones, the lines an invocation does not exercise, and what the runs'
records say they were made from."""
from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lp64_equiv_testkit import RunFolders, sim  # noqa: E402

SEEDED = {2: '00000000000000ff'}
EXPLAINED = 'i686:a64 stage-dam default sim differs-from 2 explained: a seed; removed-by: sim-seeded\n'
HEX = re.compile(r'[0-9a-f]{64}')


class Pins(RunFolders):
    """A video run's pin, `sha256=`: the folders here are made with --video."""

    def run_folder(self, label: str, trace: str, **record: object) -> Path:
        return super().run_folder(label, trace, video=True, **record)

    def pinned(self, a: Path, b: Path) -> str:
        """EXPLAINED as `pins` prints it for folders A and B."""
        expect = self.canned('unpinned.txt', EXPLAINED)
        status, out = self.equiv('pins', '--streams', 'sim', '--expect', str(expect), str(a), str(b))
        self.assertEqual(status, 0, out)
        return next(line for line in out.splitlines() if line.startswith('i686:a64 ')) + '\n'

    def test_pins_prints_the_line_with_the_newer_streams_hash(self):
        # Arrange
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(seed_at=SEEDED))

        # Act
        line = self.pinned(a, b)

        # Assert
        self.assertRegex(line, r'^i686:a64 stage-dam default sim differs-from 2 sha256=[0-9a-f]{64} explained: ')
        self.assertEqual((self.folder/'unpinned.txt').read_text(), EXPLAINED)

    def test_a_pinned_line_whose_stream_is_unchanged_passes(self):
        # Arrange
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(seed_at=SEEDED))
        expect = self.canned('expected.txt', self.pinned(a, b))

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: 1 cases, 1 streams, 0 identical, 1 as recorded', out)

    def test_a_pinned_line_fails_when_the_stream_changes_after_its_first_difference(self):
        # Arrange: the newer build's stream changes again at tick 3, after the recorded difference at 2
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(seed_at=SEEDED))
        expect = self.canned('expected.txt', self.pinned(a, b))
        (b/'run1'/'stage-dam.sim').write_text(sim(seed_at={**SEEDED, 3: '00000000000000ee'}))

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertRegex(out, r'FAIL: stage-dam sim i686:a64 differs at 2 as recorded, but its stream is not the '
                              r'one line 1 of \S+ pins for a video run \(sha256 [0-9a-f]{64}\)')

    def test_an_unpinned_line_still_accepts_its_first_difference_alone(self):
        # Arrange: no pin, so a later change goes unchecked, as before the pins
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(seed_at={**SEEDED, 3: '00000000000000ee'}))
        expect = self.canned('expected.txt', EXPLAINED)

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)

    def test_a_pin_on_a_line_that_is_not_explained_is_refused(self):
        # Arrange
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(seed_at=SEEDED))
        expect = self.canned('expected.txt', 'i686:a64 stage-dam default sim differs-from 2 sha256=' + 'a' * 64
                             + ' host: a seed\n')

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('only an explained line carries a pin', out)


class HeadlessPins(RunFolders):
    """A headless run's pin, `headless-sha256=`, beside a video run's (Task 17, scope A): a pin is per
    kind of run, the newer folder's `video` in its record."""

    def pins(self, expect_text: str, a: Path, b: Path) -> str:
        expect = self.canned('unpinned.txt', expect_text)
        status, out = self.equiv('pins', '--streams', 'sim', '--expect', str(expect), str(a), str(b))
        self.assertEqual(status, 0, out)
        return next(line for line in out.splitlines() if line.startswith('i686:a64 ')) + '\n'

    def seeded_pair(self, video: bool) -> tuple[Path, Path]:
        return (self.run_folder('i686', sim(), video=video),
                self.run_folder('a64', sim(seed_at=SEEDED), video=video))

    def test_pins_of_a_headless_run_prints_a_headless_pin(self):
        # Arrange
        a, b = self.seeded_pair(video=False)

        # Act
        line = self.pins(EXPLAINED, a, b)

        # Assert
        self.assertRegex(line, r'^i686:a64 stage-dam default sim differs-from 2 headless-sha256=[0-9a-f]{64} '
                               r'explained: ')

    def test_pins_of_a_headless_run_keeps_the_video_pin_on_the_line(self):
        # Arrange
        a, b = self.seeded_pair(video=False)
        video = 'f' * 64
        expect = EXPLAINED.replace('differs-from 2 ', f'differs-from 2 sha256={video} ')

        # Act
        line = self.pins(expect, a, b)

        # Assert
        self.assertRegex(line, rf'differs-from 2 sha256={video} headless-sha256=[0-9a-f]{{64}} explained: ')

    def test_pins_of_a_video_run_keeps_the_headless_pin_on_the_line(self):
        # Arrange
        a, b = self.seeded_pair(video=True)
        headless = 'e' * 64
        expect = EXPLAINED.replace('differs-from 2 ', f'differs-from 2 headless-sha256={headless} ')

        # Act
        line = self.pins(expect, a, b)

        # Assert
        self.assertRegex(line, rf'differs-from 2 sha256=[0-9a-f]{{64}} headless-sha256={headless} explained: ')

    def test_a_headless_run_passes_its_headless_pin(self):
        # Arrange
        a, b = self.seeded_pair(video=False)
        expect = self.canned('expected.txt', self.pins(EXPLAINED, a, b))

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: 1 cases, 1 streams, 0 identical, 1 as recorded', out)

    def test_a_headless_run_fails_a_headless_pin_its_stream_does_not_match(self):
        # Arrange
        a, b = self.seeded_pair(video=False)
        expect = self.canned('expected.txt', EXPLAINED.replace('differs-from 2 ', 'differs-from 2 headless-sha256='
                                                               + 'e' * 64 + ' '))

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertRegex(out, r'FAIL: stage-dam sim i686:a64 differs at 2 as recorded, but its stream is not the '
                              r'one line 1 of \S+ pins for a headless run \(headless-sha256 [0-9a-f]{64}\)')

    def test_a_headless_run_judges_a_line_pinned_only_for_video_by_its_first_index(self):
        # Arrange: the video pin is another stream's; a headless run does not check it
        a, b = self.seeded_pair(video=False)
        expect = self.canned('expected.txt', EXPLAINED.replace('differs-from 2 ', 'differs-from 2 sha256='
                                                               + 'f' * 64 + ' '))

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('note: line 1 has no pin for a headless run; judged by its first index alone', out)

    def test_a_video_run_judges_a_line_pinned_only_for_headless_by_its_first_index(self):
        # Arrange
        a, b = self.seeded_pair(video=True)
        expect = self.canned('expected.txt', EXPLAINED.replace('differs-from 2 ', 'differs-from 2 headless-sha256='
                                                               + 'e' * 64 + ' '))

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('note: line 1 has no pin for a video run; judged by its first index alone', out)

    def test_a_video_run_checks_its_video_pin_and_not_the_headless_one(self):
        # Arrange: a right video pin beside a wrong headless one
        a, b = self.seeded_pair(video=True)
        video = HEX.search(self.pins(EXPLAINED, a, b)).group(0)
        expect = self.canned('expected.txt', EXPLAINED.replace(
            'differs-from 2 ', f'differs-from 2 sha256={video} headless-sha256={"e" * 64} '))

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertNotIn('no pin', out)

    def test_the_pins_in_the_other_order_are_refused(self):
        # Arrange
        a, b = self.seeded_pair(video=False)
        expect = self.canned('expected.txt', EXPLAINED.replace(
            'differs-from 2 ', f'differs-from 2 headless-sha256={"e" * 64} sha256={"f" * 64} '))

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('[sha256=HEX] [headless-sha256=HEX]', out)

    def test_a_headless_pin_on_a_line_that_is_not_explained_is_refused(self):
        # Arrange
        a, b = self.seeded_pair(video=False)
        expect = self.canned('expected.txt', 'i686:a64 stage-dam default sim differs-from 2 headless-sha256='
                             + 'e' * 64 + ' host: a seed\n')

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('only an explained line carries a pin', out)


class Unexercised(RunFolders):
    def test_compare_counts_and_names_the_lines_it_did_not_exercise(self):
        # Arrange: one line for the pair compared, one for a pair and one for a profile it does not have
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(seed_at=SEEDED))
        expect = self.canned('expected.txt', EXPLAINED
                             + 'i686:x64 stage-dam default sim differs-from 2 open: another pair\n'
                             + 'i686:a64 stage-dam all sim differs-from 2 open: another profile\n')

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('note: 2 of 3 expectation lines not exercised (a pair, profile, case or stream not '
                      'compared here):', out)
        self.assertIn('  line 2: i686:x64 stage-dam default sim', out)
        self.assertIn('  line 3: i686:a64 stage-dam all sim', out)

    def test_compare_says_nothing_when_every_line_is_exercised(self):
        # Arrange
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(seed_at=SEEDED))
        expect = self.canned('expected.txt', EXPLAINED)

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertNotIn('not exercised', out)


class Provenance(RunFolders):
    def test_compare_says_when_two_folders_name_different_commits(self):
        # Arrange
        a = self.run_folder('i686', sim(), commit='a' * 40)
        b = self.run_folder('a64', sim(), commit='b' * 40)

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn(f'note: the folders ran at different commits: i686 {"a" * 40}, a64 {"b" * 40}', out)

    def test_folders_from_before_the_commit_was_recorded_still_compare(self):
        # Arrange
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(), commit='b' * 40)

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('note: the folders ran at different commits: i686 (not recorded), a64 ' + 'b' * 40, out)
        self.assertTrue(re.search(r'stage-dam sim i686:a64 identical', out))


if __name__ == '__main__':
    unittest.main()
