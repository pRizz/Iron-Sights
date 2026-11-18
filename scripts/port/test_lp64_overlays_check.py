"""ROM-free tests for the overlay lister's `check` and its reviewed list (LP64-005 Task 3)."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_overlays as overlays  # noqa: E402
from test_lp64_overlays import HEADER, INDEX, INDEX_KEY, wrap  # noqa: E402

RUN_KEY = 'src/bondtypes.h:holder:run:Gfx dl[2]; +2 fillers'


class Check(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        (self.root/'src/game').mkdir(parents=True)
        (self.root/'src/bondtypes.h').write_text(HEADER)
        (self.root/'src/game/a.c').write_text(wrap(INDEX))
        self.files = ['src/bondtypes.h', 'src/game/a.c']

    def tearDown(self):
        self.directory.cleanup()

    def run_check(self, reviewed, no_defects=False):
        """Return (status, output) of `check` against `reviewed`."""
        sites = overlays.list_sites(self.root, self.files)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = overlays.report(sites, overlays.parse_reviewed(reviewed), no_defects)
        return status, out.getvalue()

    def reviewed_text(self, *keys_):
        return '# header\n' + ''.join(f'{key}\ttyped\tthe object is of that type\n' for key in keys_)

    def test_every_site_reviewed_passes(self):
        # Arrange
        reviewed = self.reviewed_text(RUN_KEY, INDEX_KEY)
        # Act
        status, out = self.run_check(reviewed)
        # Assert
        self.assertEqual((status, out.strip().splitlines()[0]), (0, 'OK: 2 sites, all reviewed'))

    def test_a_site_missing_from_the_list_fails(self):
        # Act
        status, out = self.run_check(self.reviewed_text(RUN_KEY))
        # Assert
        self.assertEqual(status, 1)
        self.assertIn(f'unreviewed: {INDEX_KEY}', out)

    def test_a_stale_line_fails(self):
        # Arrange
        reviewed = self.reviewed_text(RUN_KEY, INDEX_KEY, 'src/game/a.c:gone:index:((u8*)x)[1]')
        # Act
        status, out = self.run_check(reviewed)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('stale: src/game/a.c:gone:index:((u8*)x)[1]', out)

    def test_a_changed_text_is_unreviewed_and_its_old_line_stale(self):
        # Arrange
        reviewed = self.reviewed_text(RUN_KEY, INDEX_KEY)
        (self.root/'src/game/a.c').write_text(wrap('n = ((u16 *) x)[3];'))
        # Act
        status, out = self.run_check(reviewed)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('unreviewed: src/game/a.c:f:index:((u16*)x)[3]', out)
        self.assertIn(f'stale: {INDEX_KEY}', out)

    def test_a_new_stored_cast_is_unreviewed(self):
        # Arrange
        reviewed = self.reviewed_text(RUN_KEY, INDEX_KEY)
        (self.root/'src/game/a.c').write_text(wrap(INDEX + '\nu16 *p = (u16 *) x;'))
        # Act
        status, out = self.run_check(reviewed)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('unreviewed: src/game/a.c:f:stored:p=(u16*)x', out)

    def test_a_repeated_key_in_the_listing_is_refused(self):
        # Arrange
        site = overlays.list_sites(self.root, self.files)[0]
        # Act
        with contextlib.redirect_stdout(io.StringIO()):
            status = overlays.report([site, site], {site.key: ('typed', 'r')})
        # Assert
        self.assertEqual(status, 2)

    def test_no_sites_is_not_ok(self):
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(overlays.report([], {}), 2)

    def test_a_run_declared_in_a_c_file_is_listed(self):
        # Arrange
        (self.root/'src/game/a.c').write_text('struct local { Model m; s32 unk04; };\n' + wrap(INDEX))
        # Act
        found = [site.key for site in overlays.list_sites(self.root, self.files)]
        # Assert
        self.assertIn('src/game/a.c:local:run:Model m; +1 fillers', found)

    def test_a_line_with_an_unknown_verdict_is_refused(self):
        with self.assertRaises(ValueError):
            overlays.parse_reviewed(f'{INDEX_KEY}\tfixed\tgone\n')

    def test_a_line_without_a_reason_is_refused(self):
        with self.assertRaises(ValueError):
            overlays.parse_reviewed(f'{INDEX_KEY}\ttyped\t\n')

    def test_the_checked_in_list_parses(self):
        # Act / Assert
        overlays.parse_reviewed(overlays.REVIEWED.read_text())


class Defects(unittest.TestCase):
    """A `defect` line records a site found wrong, until its fix takes the site off the list."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        (self.root/'src/game').mkdir(parents=True)
        (self.root/'src/bondtypes.h').write_text(HEADER)
        (self.root/'src/game/a.c').write_text(wrap(INDEX))
        self.sites = overlays.list_sites(self.root, ['src/bondtypes.h', 'src/game/a.c'])

    def tearDown(self):
        self.directory.cleanup()

    def run_check(self, index_line, no_defects=False):
        """Return (status, output) of `check` with the run typed and the index site's line `index_line`."""
        reviewed = overlays.parse_reviewed(f'{RUN_KEY}\ttyped\tdeclared so\n{index_line}\n')
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = overlays.report(self.sites, reviewed, no_defects)
        return status, out.getvalue()

    def test_a_defect_owned_by_a_task_is_accepted(self):
        # Act
        reviewed = overlays.parse_reviewed(f'{INDEX_KEY}\tdefect\tTask 4: reads unk04 at 4, at 8 at 64 bits\n')
        # Assert
        self.assertEqual(reviewed[INDEX_KEY][0], 'defect')

    def test_a_strict_defect_is_accepted(self):
        # Act
        reviewed = overlays.parse_reviewed(f'{INDEX_KEY}\tdefect\tstrict: in gunfire.c\n')
        # Assert
        self.assertEqual(reviewed[INDEX_KEY][0], 'defect')

    def test_an_undecided_defect_is_accepted(self):
        # Act
        reviewed = overlays.parse_reviewed(f'{INDEX_KEY}\tdefect\tundecided: the caller is unknown\n')
        # Assert
        self.assertEqual(reviewed[INDEX_KEY][0], 'defect')

    def test_a_defect_without_an_owner_is_refused(self):
        with self.assertRaises(ValueError):
            overlays.parse_reviewed(f'{INDEX_KEY}\tdefect\treads the wrong member\n')

    def test_a_defect_owned_by_a_task_without_a_number_is_refused(self):
        with self.assertRaises(ValueError):
            overlays.parse_reviewed(f'{INDEX_KEY}\tdefect\tTask: reads the wrong member\n')

    def test_check_counts_the_defects_after_its_ok_line(self):
        # Act
        status, out = self.run_check(f'{INDEX_KEY}\tdefect\tTask 4: wrong at 64 bits')
        # Assert
        self.assertEqual((status, out.strip().splitlines()[:2]), (0, ['OK: 2 sites, all reviewed', '1 defect lines']))

    def test_check_counts_the_defects_after_its_failure_line(self):
        # Arrange
        stale = 'src/game/a.c:gone:index:((u8*)x)[1]\tdefect\tTask 4: gone'
        # Act
        status, out = self.run_check(f'{INDEX_KEY}\tdefect\tTask 4: wrong\n{stale}')
        # Assert
        self.assertEqual((status, out.strip().splitlines()[-2]), (1, '2 defect lines'))

    def test_check_counts_no_defects_as_zero(self):
        # Act
        status, out = self.run_check(f'{INDEX_KEY}\ttyped\tdeclared so')
        # Assert
        self.assertEqual((status, out.strip().splitlines()[-2]), (0, '0 defect lines'))

    def test_check_counts_the_measured_lines_after_the_defects(self):
        # Arrange
        measured = f'{INDEX_KEY}\ttyped\tmeasured: the ROM holds no other form (lp64_model_scan.py roots)'
        # Act
        status, out = self.run_check(measured)
        # Assert
        self.assertEqual((status, out.strip().splitlines()[1:]), (0, ['0 defect lines', '1 measured lines']))

    def test_a_reason_using_the_word_later_is_not_measured(self):
        # Act
        status, out = self.run_check(f'{INDEX_KEY}\ttyped\tthe offsets, measured: 4 and 8')
        # Assert
        self.assertEqual((status, out.strip().splitlines()[-1]), (0, '0 measured lines'))

    def test_a_defect_measured_after_its_owner_is_counted(self):
        # Act
        status, out = self.run_check(f'{INDEX_KEY}\tdefect\tTask 4: measured: none (lp64_setup_scan.py types)')
        # Assert
        self.assertEqual((status, out.strip().splitlines()[-1]), (0, '1 measured lines'))

    def test_a_measured_reason_without_a_scanner_command_is_refused_by_line(self):
        # Act
        with self.assertRaises(ValueError) as caught:
            overlays.parse_reviewed(f'# header\n{INDEX_KEY}\ttyped\tmeasured: a scratch reader found none\n')
        # Assert
        self.assertIn('reviewed line 2', str(caught.exception))
        self.assertIn('lp64_model_scan.py', str(caught.exception))

    def test_check_counts_no_measured_lines_as_zero(self):
        # Act
        status, out = self.run_check(f'{INDEX_KEY}\ttyped\tdeclared so')
        # Assert
        self.assertEqual((status, out.strip().splitlines()[-1]), (0, '0 measured lines'))

    def test_no_defects_fails_on_a_defect_and_names_it(self):
        # Act
        status, out = self.run_check(f'{INDEX_KEY}\tdefect\tTask 4: wrong at 64 bits', no_defects=True)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn(f'defect: {INDEX_KEY}\tTask 4: wrong at 64 bits', out)

    def test_no_defects_passes_without_a_defect(self):
        # Act
        status, _ = self.run_check(f'{INDEX_KEY}\ttyped\tdeclared so', no_defects=True)
        # Assert
        self.assertEqual(status, 0)

    def test_no_defects_is_a_check_option(self):
        # Act
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as err:
            with self.assertRaises(SystemExit):
                overlays.main(['list', '--no-defects'])
        # Assert
        self.assertIn('--no-defects is a check option', err.getvalue())

    def test_the_help_and_the_reviewed_header_describe_the_defect_verdict(self):
        # Act
        header = [line for line in overlays.REVIEWED.read_text().splitlines() if line.startswith('#')]
        # Assert
        self.assertIn('defect', overlays.__doc__)
        self.assertTrue(any(line.lstrip('# ').startswith('defect ') for line in header))


if __name__ == '__main__':
    unittest.main()
