"""Host-independent tests for macos_link_check.py, which reads `nm -m` text for strong
symbols a macOS link left to be looked up when the game starts."""
import contextlib
import importlib.util
import io
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('macos_link_check', Path(__file__).with_name('macos_link_check.py'))
link_check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(link_check)

# Lines as Apple's nm 27.2 prints them for the headless game (`nm -m IronSights`).
WEAK = '                 (undefined) weak external _CarmourguardZ (dynamically looked up)'
STRONG = '                 (undefined) external _missing (dynamically looked up)'
SYSTEM = '                 (undefined) external _printf (from libSystem)'
DEFINED = '0000000100003f20 (__TEXT,__text) external _main'


class StrongLookups(unittest.TestCase):
    def test_a_weak_reference_is_not_strong(self):
        # Arrange / Act
        names = link_check.strong_lookups(WEAK)
        # Assert
        self.assertEqual(names, [])

    def test_a_strong_reference_is_named(self):
        # Arrange / Act
        names = link_check.strong_lookups(STRONG)
        # Assert
        self.assertEqual(names, ['_missing'])

    def test_a_symbol_bound_to_a_library_is_ignored(self):
        # Arrange / Act
        names = link_check.strong_lookups(SYSTEM)
        # Assert
        self.assertEqual(names, [])

    def test_strong_names_come_in_the_order_of_the_text(self):
        # Arrange
        text = '\n'.join([WEAK, STRONG.replace('_missing', '_b'), DEFINED, SYSTEM, STRONG.replace('_missing', '_a')])
        # Act
        names = link_check.strong_lookups(text)
        # Assert
        self.assertEqual(names, ['_b', '_a'])


class Counts(unittest.TestCase):
    def test_lookups_counts_weak_and_strong_lines_only(self):
        # Arrange
        text = '\n'.join([WEAK, WEAK.replace('_CarmourguardZ', '_other'), STRONG, SYSTEM, DEFINED])
        # Act
        count = link_check.lookup_count(text)
        # Assert
        self.assertEqual(count, 3)


class Report(unittest.TestCase):
    def run_report(self, text):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = link_check.report(text)
        return status, out.getvalue()

    def test_all_weak_passes_with_the_count(self):
        # Arrange / Act
        status, output = self.run_report('\n'.join([WEAK, SYSTEM]))
        # Assert
        self.assertEqual(status, 0)
        self.assertEqual(output, 'OK: 1 weak references, 0 strong\n')

    def test_a_strong_lookup_fails_and_is_named(self):
        # Arrange / Act
        status, output = self.run_report('\n'.join([WEAK, STRONG]))
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('_missing', output)
        self.assertNotIn('OK:', output)


if __name__ == '__main__':
    unittest.main()
