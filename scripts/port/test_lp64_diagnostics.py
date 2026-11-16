"""ROM-free, compiler-free tests for the 64-bit diagnostics ratchet's pure functions."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('lp64_diagnostics', Path(__file__).with_name('lp64_diagnostics.py'))
diag = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diag)

SAMPLE = (
    "/w/src/game/front.c:1124:13: warning: passing argument 1 of 'f' makes pointer from integer without a cast [-Wint-conversion]\n"
    "/w/src/game/front.c:1124:13: warning: passing argument 1 of 'f' makes pointer from integer without a cast [-Wint-conversion]\n"
    "/w/src/game/chr.c:1700:29: warning: cast from pointer to integer of different size [-Wpointer-to-int-cast]\n"
    "/w/src/game/chr.c:9:1: warning: unused variable 'x' [-Wunused-variable]\n"
    "/w/src/crash.c:748:11: error: array initializer must be an initializer list\n"
    "In file included from /w/src/game/chr.c:3:\n")


def report(compiler, by_file):
    return dict(compiler=compiler, total=sum(by_file.values()), by_file=by_file, by_kind={})


class Parse(unittest.TestCase):
    def test_parse_keeps_tracked_kinds_and_errors_once_each(self):
        # Arrange / Act
        found = diag.parse(SAMPLE, Path('/w'))
        # Assert
        self.assertEqual(found, {
            ('src/game/front.c', 1124, 13, 'int-conversion'),
            ('src/game/chr.c', 1700, 29, 'pointer-to-int-cast'),
            ('src/crash.c', 748, 11, 'error')})

    def test_int_to_pointer_cast_is_tracked(self):
        # Arrange
        text = "/w/a.c:3:4: warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]\n"
        # Act
        found = diag.parse(text, Path('/w'))
        # Assert
        self.assertEqual(found, {('a.c', 3, 4, 'int-to-pointer-cast')})


class SyntaxCommand(unittest.TestCase):
    def test_syntax_command_drops_32_bit_and_output_flags(self):
        # Arrange
        entry = dict(file='/w/src/a.c', command='gcc -m32 -msse2 -mfpmath=sse -w -DX -I/w/include -o a.o -c /w/src/a.c')
        # Act
        argv = diag.syntax_command(entry)
        # Assert
        for dropped in ('-m32', '-msse2', '-mfpmath=sse', '-w', '-c', '-o', 'a.o'):
            self.assertNotIn(dropped, argv)
        for kept in ('-DX', '-I/w/include', '/w/src/a.c', '-fsyntax-only', '-Wint-conversion'):
            self.assertIn(kept, argv)

    def test_syntax_command_drops_dependency_and_sanitizer_flags(self):
        # Arrange
        entry = dict(file='/w/a.c', command='gcc -MD -MT a.o -MF a.d -fsanitize=address -fno-omit-frame-pointer -c /w/a.c')
        # Act
        argv = diag.syntax_command(entry)
        # Assert
        for dropped in ('-MD', '-MT', 'a.o', '-MF', 'a.d', '-fsanitize=address'):
            self.assertNotIn(dropped, argv)
        self.assertIn('-fno-omit-frame-pointer', argv)

    def test_syntax_command_skips_cxx(self):
        # Arrange / Act / Assert
        self.assertIsNone(diag.syntax_command(dict(file='/w/port/fast3d/gfx_pc.cpp', command='g++ -c x')))

    def test_syntax_command_accepts_an_arguments_list(self):
        # Arrange
        entry = dict(file='/w/a.c', arguments=['cc', '-m32', '-c', '/w/a.c', '-o', 'a.o'])
        # Act
        argv = diag.syntax_command(entry)
        # Assert
        self.assertEqual(argv[0], 'cc')
        self.assertNotIn('-m32', argv)


class Summarize(unittest.TestCase):
    def test_summary_counts_by_kind_and_file(self):
        # Arrange
        found = diag.parse(SAMPLE, Path('/w'))
        # Act
        summary = diag.summarize(found, 'gcc-13')
        # Assert
        self.assertEqual(summary['total'], 3)
        self.assertEqual(summary['compiler'], 'gcc-13')
        self.assertEqual(summary['by_kind'], {'int-conversion': 1, 'pointer-to-int-cast': 1, 'error': 1})
        self.assertEqual(summary['by_file'], {'src/game/front.c': 1, 'src/game/chr.c': 1, 'src/crash.c': 1})


class Regressions(unittest.TestCase):
    def test_a_file_rising_is_a_regression_even_if_the_total_falls(self):
        # Arrange
        baseline = report('gcc-13', {'a.c': 2, 'b.c': 8})
        current = report('gcc-13', {'a.c': 3, 'b.c': 6})
        # Act
        found = diag.regressions(baseline, current)
        # Assert
        self.assertEqual(len(found), 1)
        self.assertIn('a.c', found[0])

    def test_a_new_file_with_diagnostics_is_a_regression(self):
        # Arrange
        baseline = report('gcc-13', {'a.c': 1})
        current = report('gcc-13', {'a.c': 1, 'new.c': 1})
        # Act
        found = diag.regressions(baseline, current)
        # Assert
        self.assertIn('new.c', found[0])

    def test_a_different_compiler_is_refused_not_compared(self):
        # Arrange
        baseline = report('gcc-13', {'a.c': 1})
        current = report('gcc-14', {'a.c': 1})
        # Act
        message = diag.regressions(baseline, current)
        # Assert
        self.assertEqual(len(message), 1)
        self.assertIn('gcc-13', message[0])
        self.assertIn('gcc-14', message[0])

    def test_fewer_diagnostics_is_not_a_regression(self):
        # Arrange
        baseline = report('gcc-13', {'a.c': 5})
        current = report('gcc-13', {'a.c': 1})
        # Act / Assert
        self.assertEqual(diag.regressions(baseline, current), [])


if __name__ == '__main__':
    unittest.main()
