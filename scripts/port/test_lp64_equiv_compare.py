"""ROM-free tests for lp64_equiv.py compare (LP64-006 Tasks 5 and 7): run folders written by hand, with the
record `run` leaves (equiv.json), compared under lp64_equiv_expected.txt's rules."""
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_equiv  # noqa: E402
from test_lp64_equiv import StandIns, sim  # noqa: E402

AGREED = {'ran': True, 'agreed': True}


class Compare(StandIns):
    def run_folder(self, label: str, trace: str, case: str = 'stage-dam', profile: str = 'default',
                   **record: object) -> Path:
        """A run folder holding CASE's trace in run1 and a record of two runs that agreed, changed by RECORD."""
        folder = self.folder/label
        (folder/'run1').mkdir(parents=True)
        (folder/'run1'/f'{case}.sim').write_text(trace)
        written = {'profile': profile, 'video': False, 'repeat': 2, 'cases': [case], 'verdicts': {case: AGREED}}
        written.update(record)
        (folder/lp64_equiv.RECORD).write_text(json.dumps({key: value for key, value in written.items()
                                                          if value is not None}))
        return folder

    def test_compare_names_first_difference_and_field(self):
        # Arrange
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(seed_at={2: '00000000000000ff'}))

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam sim i686:a64 first differs at 2 (seed)', out)

    def test_compare_fails_an_exception_that_now_matches(self):
        # Arrange
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim())
        expect = self.canned('expected.txt', 'i686:a64 stage-dam default sim differs-from 2 open: a seed\n')

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam sim i686:a64 identical (3)', out)
        self.assertIn('FAIL: stage-dam sim i686:a64 is recorded as differing from 2, but it is identical', out)

    def test_compare_fails_a_difference_not_listed(self):
        # Arrange
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(seed_at={3: '00000000000000ff'}))
        expect = self.canned('expected.txt', '# nothing differs\n')

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: stage-dam sim i686:a64 differs at 3, which the file does not record', out)

    def test_compare_accepts_a_recorded_difference(self):
        # Arrange
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(seed_at={2: '00000000000000ff'}))
        expect = self.canned('expected.txt', 'i686:a64 stage-dam default sim differs-from 2 explained: a seed; '
                                             'removed-by: sim-seeded\n')

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', '--expect', str(expect), str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: 1 cases, 1 streams, 0 identical, 1 as recorded', out)

    def test_compare_refuses_a_folder_with_a_failed_case(self):
        # Arrange: both builds' runs failed at the same tick, so their traces would match
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(), verdicts={'stage-dam': {'ran': False, 'agreed': None}})

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn(f'error: {b} is refused: stage-dam did not run', out)
        self.assertNotIn('identical', out)

    def test_compare_refuses_a_single_run(self):
        # Arrange
        a = self.run_folder('i686', sim(), repeat=1, verdicts={'stage-dam': {'ran': True, 'agreed': None}})
        b = self.run_folder('a64', sim())

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn(f'error: {a} is refused: it ran each case once (--repeat 1)', out)

    def test_compare_refuses_a_folder_without_an_agreement_verdict(self):
        # Arrange: a record from before the verdicts were written
        a = self.run_folder('i686', sim())
        b = self.run_folder('a64', sim(), verdicts=None)

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn(f'error: {b} is refused: its record has no agreement verdict', out)

    def test_compare_refuses_a_case_that_did_not_agree_with_itself(self):
        # Arrange
        a = self.run_folder('i686', sim(), verdicts={'stage-dam': {'ran': True, 'agreed': False}})
        b = self.run_folder('a64', sim())

        # Act
        status, out = self.equiv('compare', '--streams', 'sim', str(a), str(b))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn(f'error: {a} is refused: stage-dam did not agree with itself', out)

    def test_compare_names_a_byte_stream_difference_by_its_offset(self):
        # Arrange: a snd stream has no reader yet (Task 9), so it is compared byte for byte
        a = self.run_folder('i686', sim(), video=True)
        b = self.run_folder('a64', sim(), video=True)
        (a/'run1'/'stage-dam.wav').write_bytes(b'\x01\x02\x03\x04')
        (b/'run1'/'stage-dam.wav').write_bytes(b'\x01\x02\xff\x04')

        # Act
        status, out = self.equiv('compare', '--streams', 'snd', str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam snd i686:a64 first differs at 2 (byte)', out)

    def test_compare_names_a_digest_difference_by_its_frame_and_field(self):
        # Arrange
        a = self.run_folder('i686', sim(), video=True)
        b = self.run_folder('a64', sim(), video=True)
        frame = 'F n={} tick={} draws=1 tris=2 vtx={} tex=0:811c9dc5 state=0383debb post=e5d4f569\n'
        (a/'run1'/'stage-dam.gfx').write_text(frame.format(1, 1, '3bee89fb') + frame.format(2, 2, '3bee89fb'))
        (b/'run1'/'stage-dam.gfx').write_text(frame.format(1, 1, '3bee89fb') + frame.format(2, 2, '00000000'))

        # Act
        status, out = self.equiv('compare', '--streams', 'gfx', str(a), str(b))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam gfx i686:a64 first differs at 2 (vtx)', out)

    def test_compare_reads_a_demo_digest_as_its_playback(self):
        # Arrange: the replay starts one frame later and at another timer in the second folder
        frame = 'F n={} tick={} draws=1 tris=2 vtx=3bee89fb tex=0:811c9dc5 state=0383debb post=e5d4f569{}\n'
        a = self.run_folder('i686', sim(), case='dam1', video=True)
        b = self.run_folder('a64', sim(), case='dam1', video=True)
        (a/'run1'/'dam1.gfx').write_text(frame.format(1, 1, '') + frame.format(2, 2, ' demo=1'))
        (b/'run1'/'dam1.gfx').write_text(frame.format(1, 4, '') + frame.format(2, 6, '') + frame.format(3, 9, ' demo=1'))

        # Act
        status, out = self.equiv('compare', '--streams', 'gfx', str(a), str(b))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('dam1 gfx i686:a64 identical (1)', out)


if __name__ == '__main__':
    unittest.main()
