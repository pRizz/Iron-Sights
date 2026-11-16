"""ROM-free tests for the 64-bit diagnostics ratchet. No real compiler: the command-line tests run a
stand-in compiler, a Python script that prints the diagnostics its input file asks for."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

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


    def test_an_error_with_a_tracked_flag_counts_under_its_kind(self):
        # Arrange
        text = "/w/a.c:5:9: error: initialization of 'int' from 'void *' makes integer from pointer without a cast [-Wint-conversion]\n"
        # Act
        found = diag.parse(text, Path('/w'))
        # Assert
        self.assertEqual(found, {('a.c', 5, 9, 'int-conversion')})


class Sites(unittest.TestCase):
    FOUND = {('b.c', 7, 2, 'error'), ('a.c', 12, 4, 'int-conversion'), ('a.c', 3, 1, 'pointer-to-int-cast')}

    def test_sites_lists_every_diagnostic_in_file_and_line_order(self):
        # Act
        lines = diag.site_lines(self.FOUND)
        # Assert
        self.assertEqual(lines, ['a.c:3: pointer-to-int-cast', 'a.c:12: int-conversion', 'b.c:7: error'])

    def test_sites_can_be_limited_to_named_files(self):
        # Act
        lines = diag.site_lines(self.FOUND, ['b.c'])
        # Assert
        self.assertEqual(lines, ['b.c:7: error'])


class Messages(unittest.TestCase):
    TEXT = ("/w/a.c:3:9: warning: assignment to 'int *' from 'int' makes pointer from integer without a cast [-Wint-conversion]\n"
            "    3 |   p = 0x10;\n"
            "      |     ^\n"
            "/w/a.c:5:2: warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]\n"
            "    5 | \tq = (int *)n;\n")

    def test_parse_with_text_keeps_the_message_and_the_quoted_source_line(self):
        # Act
        found = diag.parse_with_text(self.TEXT, Path('/w'))
        # Assert
        self.assertEqual(found[('a.c', 3, 9, 'int-conversion')],
                         ("assignment to 'int *' from 'int' makes pointer from integer without a cast", 'p = 0x10;'))
        self.assertEqual(found[('a.c', 5, 2, 'int-to-pointer-cast')][1], 'q = (int *)n;')

    def test_a_site_without_a_quoted_line_has_an_empty_source(self):
        # Arrange
        text = "/w/a.c:3:9: warning: x [-Wint-conversion]\n/w/a.c:4:1: warning: y [-Wint-conversion]\n"
        # Act
        found = diag.parse_with_text(text, Path('/w'))
        # Assert
        self.assertEqual(found[('a.c', 3, 9, 'int-conversion')], ('x', ''))

    def test_message_lines_read_file_line_kind_message_and_source(self):
        # Arrange
        found = {('a.c', 3, 9, 'int-conversion'): ('bad', 'p = 1;'), ('b.c', 1, 1, 'error'): ('worse', '')}
        # Act
        lines = diag.site_lines(found, ['a.c'], with_messages=True)
        # Assert
        self.assertEqual(lines, ['a.c:3: int-conversion: bad | p = 1;'])


class Merge(unittest.TestCase):
    def test_a_source_listed_by_two_trees_keeps_the_first_trees_command(self):
        # Arrange
        first = [dict(file='/w/a.c', command='cc first')]
        second = [dict(file='/w/a.c', command='cc second'), dict(file='/w/b.c', command='cc second')]
        # Act
        merged = diag.merge_entries([first, second])
        # Assert
        self.assertEqual([(e['file'], e['command']) for e in merged],
                         [('/w/a.c', 'cc first'), ('/w/b.c', 'cc second')])


class Rise(unittest.TestCase):
    def test_a_higher_total_is_refused_with_both_figures(self):
        # Act
        message = diag.rise_refusal(10, 12)
        # Assert
        self.assertIn('10', message)
        self.assertIn('12', message)

    def test_an_equal_or_lower_total_is_not_refused(self):
        # Act / Assert
        self.assertIsNone(diag.rise_refusal(10, 10))
        self.assertIsNone(diag.rise_refusal(10, 3))


class DefaultInput(unittest.TestCase):
    def test_the_default_compile_commands_path_is_under_the_repository_root(self):
        self.assertEqual(diag.DEFAULT_COMPILE_COMMANDS,
                         Path(__file__).resolve().parents[2]/'build'/'lp64'/'diag'/'compile_commands.json')


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


class Errors(unittest.TestCase):
    def test_summary_lists_errors_by_file_only_for_files_with_errors(self):
        # Arrange
        found = {('b.c', 1, 1, 'error'), ('b.c', 2, 1, 'error'), ('b.c', 3, 1, 'int-conversion'),
                 ('a.c', 1, 1, 'int-conversion')}
        # Act
        summary = diag.summarize(found, 'gcc-13')
        # Assert
        self.assertEqual(summary['errors_by_file'], {'b.c': 2})

    def test_a_file_whose_errors_rise_is_a_regression_even_if_its_total_falls(self):
        # Arrange
        baseline = dict(report('gcc-13', {'a.c': 261}), errors_by_file={})
        current = dict(report('gcc-13', {'a.c': 1}), errors_by_file={'a.c': 1})
        # Act
        found = diag.regressions(baseline, current)
        # Assert
        self.assertEqual(len(found), 1)
        self.assertIn('a.c', found[0])

    def test_an_unchanged_error_count_is_not_a_regression(self):
        # Arrange
        baseline = dict(report('gcc-13', {'a.c': 5}), errors_by_file={'a.c': 2})
        current = dict(report('gcc-13', {'a.c': 5}), errors_by_file={'a.c': 2})
        # Act / Assert
        self.assertEqual(diag.regressions(baseline, current), [])


class CEntries(unittest.TestCase):
    def test_no_c_commands_raises_naming_the_file(self):
        # Arrange
        entries = [dict(file='/w/a.cpp', command='g++ -c /w/a.cpp')]
        # Act / Assert
        with self.assertRaises(RuntimeError) as caught:
            diag.c_entries(entries, Path('/w/compile_commands.json'))
        self.assertIn('/w/compile_commands.json', str(caught.exception))

    def test_an_empty_list_raises(self):
        # Arrange / Act / Assert
        with self.assertRaises(RuntimeError):
            diag.c_entries([], Path('x.json'))


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


FAKE_COMPILER = """import sys
source = next(arg for arg in sys.argv[1:] if arg.endswith('.c'))
status, _, stderr = open(source).read().partition('\\n')
sys.stderr.write(stderr)
sys.exit(int(status))
"""


class CommandLine(unittest.TestCase):
    """Runs `main` on a compile_commands.json whose compiler is FAKE_COMPILER: each C file holds the
    exit status on its first line and the compiler output to print after it."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        (self.folder/'cc.py').write_text(FAKE_COMPILER)
        self.baseline = self.folder/'baseline.json'
        for patch in (mock.patch.object(diag, 'BASELINE', self.baseline),
                      mock.patch.object(diag, 'compiler_id', lambda: 'gcc-13')):
            patch.start()
            self.addCleanup(patch.stop)

    def commands(self, files: dict[str, str]) -> Path:
        entries = []
        for name, text in files.items():
            (self.folder/name).write_text(text)
            entries.append(dict(directory=str(self.folder), file=str(self.folder/name),
                                arguments=[sys.executable, str(self.folder/'cc.py'), str(self.folder/name)]))
        path = self.folder/'compile_commands.json'
        path.write_text(json.dumps(entries))
        return path

    def run_main(self, *args):
        with contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()) as err:
            status = diag.main(list(args))
        return status, out.getvalue(), err.getvalue()

    def test_a_fatal_error_fails_every_command_naming_the_file(self):
        # Arrange
        path = self.commands({
            'a.c': f"0\n{self.folder}/a.c:1:1: warning: x [-Wint-conversion]\n",
            'b.c': f"1\n{self.folder}/b.c:2:10: fatal error: port_buildinfo.h: No such file or directory\n"
                   "compilation terminated.\n"})
        self.baseline.write_text(diag.render(diag.summarize(set(), 'gcc-13')))
        for command in ('count', 'check', 'update'):
            with self.subTest(command=command):
                # Act
                status, out, err = self.run_main(command, '--compile-commands', str(path))
                # Assert
                self.assertNotEqual(status, 0)
                self.assertIn('b.c', err)
                self.assertEqual(json.loads(self.baseline.read_text())['total'], 0)

    def test_an_unparsed_driver_error_fails_even_beside_tracked_warnings(self):
        # Arrange
        path = self.commands({'a.c': f"1\n{self.folder}/a.c:1:1: warning: x [-Wint-conversion]\n"
                                     "gcc: error: unrecognized command-line option '-mfoo'\n"})
        # Act
        status, out, err = self.run_main('count', '--compile-commands', str(path))
        # Assert
        self.assertNotEqual(status, 0)
        self.assertIn('-mfoo', err)

    def test_a_failing_check_prints_the_sites_of_each_file_that_rose(self):
        # Arrange
        path = self.commands({
            'a.c': f"0\n{self.folder}/a.c:3:1: warning: x [-Wint-conversion]\n",
            'b.c': f"0\n{self.folder}/b.c:9:1: warning: y [-Wint-conversion]\n"})
        self.baseline.write_text(diag.render(diag.summarize({('b.c', 9, 1, 'int-conversion')}, 'gcc-13')))
        with mock.patch.object(diag, 'ROOT', self.folder):
            # Act
            status, out, err = self.run_main('check', '--compile-commands', str(path))
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('a.c:3: int-conversion', out)
        self.assertNotIn('b.c:9', out)

    def test_two_compile_command_files_naming_one_source_measure_it_once_with_the_first_command(self):
        # Arrange
        text = f"0\n{self.folder}/a.c:3:1: warning: x [-Wint-conversion]\n"
        first = self.commands({'a.c': text})
        (self.folder/'bad.py').write_text('import sys\nsys.stderr.write("gcc: error: boom\\n")\nsys.exit(1)\n')
        second = self.folder/'second.json'
        second.write_text(json.dumps([dict(directory=str(self.folder), file=str(self.folder/'a.c'),
                                           arguments=[sys.executable, str(self.folder/'bad.py')])]))
        # Act
        status, out, err = self.run_main('count', '--compile-commands', str(first), '--compile-commands', str(second))
        # Assert
        self.assertEqual(status, 0, err)
        self.assertEqual(json.loads(out)['total'], 1)

    def test_sites_messages_prints_the_message_and_source_text(self):
        # Arrange
        path = self.commands({'a.c': f"0\n{self.folder}/a.c:3:1: warning: bad thing [-Wint-conversion]\n"
                                     "    3 |   p = 1;\n"})
        with mock.patch.object(diag, 'ROOT', self.folder):
            # Act
            status, out, err = self.run_main('sites', '--messages', '--compile-commands', str(path))
        # Assert
        self.assertEqual(status, 0)
        self.assertEqual(out.strip(), 'a.c:3: int-conversion: bad thing | p = 1;')

    def test_update_with_a_higher_total_exits_2_and_leaves_the_baseline_untouched(self):
        # Arrange
        path = self.commands({'a.c': f"0\n{self.folder}/a.c:3:1: warning: x [-Wint-conversion]\n"})
        self.baseline.write_text(diag.render(diag.summarize(set(), 'gcc-13')))
        before = self.baseline.read_bytes()
        with mock.patch.object(diag, 'ROOT', self.folder):
            # Act
            status, out, err = self.run_main('update', '--compile-commands', str(path))
        # Assert
        self.assertEqual(status, 2)
        self.assertEqual(self.baseline.read_bytes(), before)
        self.assertIn('--allow-rise', err)

    def test_update_with_allow_rise_writes_and_prints_the_reason(self):
        # Arrange
        path = self.commands({'a.c': f"0\n{self.folder}/a.c:3:1: warning: x [-Wint-conversion]\n"})
        self.baseline.write_text(diag.render(diag.summarize(set(), 'gcc-13')))
        with mock.patch.object(diag, 'ROOT', self.folder):
            # Act
            status, out, err = self.run_main('update', '--allow-rise', 'video configuration added',
                                             '--compile-commands', str(path))
        # Assert
        self.assertEqual(status, 0)
        self.assertEqual(json.loads(self.baseline.read_text())['total'], 1)
        self.assertIn('video configuration added', out + err)


if __name__ == '__main__':
    unittest.main()
