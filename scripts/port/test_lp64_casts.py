"""ROM-free tests for the truncating-cast allowlist check."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('lp64_casts', Path(__file__).with_name('lp64_casts.py'))
casts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(casts)


class FindCasts(unittest.TestCase):
    def test_a_u32_cast_of_a_uintptr_t_cast_is_found_on_its_line(self):
        # Act / Assert
        self.assertEqual(casts.find_casts("x = (u32)(uintptr_t)p;"), [1])

    def test_a_plain_uintptr_t_cast_is_not_found(self):
        # Act / Assert
        self.assertEqual(casts.find_casts("x = (uintptr_t)p;"), [])

    def test_spaces_between_the_casts_are_allowed(self):
        # Act / Assert
        self.assertEqual(casts.find_casts("x = (s32) (intptr_t) p;"), [1])

    def test_every_truncating_target_type_is_found(self):
        # Arrange
        text = '\n'.join(f'a = ({t})(uintptr_t)p;' for t in
                         ('u32', 's32', 'int', 'unsigned', 'unsigned int', 'u16', 's16'))
        # Act / Assert
        self.assertEqual(casts.find_casts(text), [1, 2, 3, 4, 5, 6, 7])

    def test_a_widening_or_pointer_cast_is_not_found(self):
        # Act / Assert
        self.assertEqual(casts.find_casts("a = (u64)(uintptr_t)p; b = (void *)(uintptr_t)n; c = (uintptr_t)(u32)n;"), [])


class Check(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root/'src').mkdir()
        self.allowlist = self.root/'allowed.txt'
        for patch in (mock.patch.object(casts, 'ROOT', self.root),
                      mock.patch.object(casts, 'ALLOWLIST', self.allowlist),
                      mock.patch.object(casts, 'tracked_files', lambda root: ['src/a.c', 'src/b.c', 'README.md'])):
            patch.start()
            self.addCleanup(patch.stop)

    def run_main(self, *args):
        with contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()) as err:
            status = casts.main(list(args))
        return status, out.getvalue(), err.getvalue()

    def write(self, a_casts: int, b_casts: int, allowed: str):
        (self.root/'src'/'a.c').write_text('x = (u32)(uintptr_t)p;\n' * a_casts)
        (self.root/'src'/'b.c').write_text('x = (u32)(uintptr_t)p;\n' * b_casts)
        (self.root/'README.md').write_text('(u32)(uintptr_t)p')
        self.allowlist.write_text(allowed)

    def test_a_file_over_its_allowed_count_fails_naming_the_file_and_its_lines(self):
        # Arrange
        self.write(2, 0, 'src/a.c\t1\texisting before LP64-002\n')
        # Act
        status, out, err = self.run_main('check')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('src/a.c', out + err)
        self.assertIn('src/a.c:2', out + err)

    def test_equal_or_lower_counts_pass(self):
        # Arrange
        self.write(1, 1, 'src/a.c\t1\tr\nsrc/b.c\t3\tr\n')
        # Act
        status, out, err = self.run_main('check')
        # Assert
        self.assertEqual(status, 0, out + err)

    def test_a_file_absent_from_the_allowlist_with_one_cast_fails(self):
        # Arrange
        self.write(0, 1, 'src/a.c\t1\tr\n')
        # Act
        status, out, err = self.run_main('check')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('src/b.c', out + err)

    def test_files_outside_the_scanned_extensions_are_ignored(self):
        # Arrange
        self.write(0, 0, '')
        # Act
        status, out, err = self.run_main('check')
        # Assert
        self.assertEqual(status, 0, out + err)

    def test_list_prints_each_site(self):
        # Arrange
        self.write(1, 0, '')
        # Act
        status, out, err = self.run_main('list')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('src/a.c:1', out)


class Allowlist(unittest.TestCase):
    def test_parse_reads_path_count_and_skips_comments_and_blank_lines(self):
        # Act
        allowed = casts.parse_allowlist('# note\n\nsrc/a.c\t3\treason here\n')
        # Assert
        self.assertEqual(allowed, {'src/a.c': 3})

    def test_a_line_without_a_numeric_count_is_rejected(self):
        # Act / Assert
        with self.assertRaises(ValueError):
            casts.parse_allowlist('src/a.c\tmany\treason\n')


if __name__ == '__main__':
    unittest.main()
