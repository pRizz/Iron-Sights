"""ROM-free tests for the `narrowing` and `unprototyped` commands of lp64_diagnostics.py.

No real compiler: each build directory's entries run a stand-in script that prints what the source
file it is given asks for, and only when the command line carries the flags the real run needs."""
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

# A source file is: exit status, a line each of stderr up to `--aux--`, then the -aux-info text.
FAKE_COMPILER = """import os, sys
source = next(arg for arg in sys.argv[1:] if arg.endswith(('.c', '.cpp')))
status, _, rest = open(source).read().partition('\\n')
stderr, _, aux = rest.partition('--aux--\\n')
if '-Wconversion' in sys.argv and os.environ.get('LC_ALL') != 'C':
    sys.exit('narrowing must run under LC_ALL=C')
needs = [flag for flag in ('-Wconversion', '-Wimplicit-function-declaration') if flag in sys.argv]
if needs or '-aux-info' in sys.argv:
    sys.stderr.write(stderr)
if '-aux-info' in sys.argv:
    open(sys.argv[sys.argv.index('-aux-info') + 1], 'w').write(aux)
sys.exit(int(status))
"""
GATED = ['-Werror=int-conversion', '-Werror=pointer-to-int-cast', '-Werror=int-to-pointer-cast']
U32 = "'u32' {aka 'unsigned int'}"


class TreeCase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        (self.folder/'cc.py').write_text(FAKE_COMPILER)
        self.narrowing_allowed = self.folder/'narrowing.txt'
        self.unprototyped_allowed = self.folder/'unprototyped.txt'

    def build_dir(self, name: str, files: dict[str, str]) -> Path:
        tree = self.folder/name
        (tree/'generated').mkdir(parents=True)
        (tree/'CMakeCache.txt').write_text('GE_POINTER_BITS:STRING=64\n')
        (tree/'generated'/'port_buildinfo.h').write_text('#define X 1\n')
        entries = []
        for source, text in files.items():
            (self.folder/source).write_text(text)
            entries.append(dict(directory=str(tree), file=str(self.folder/source),
                                arguments=[sys.executable, str(self.folder/'cc.py'), *GATED, '-o', 'x.o', '-c',
                                           str(self.folder/source)]))
        (tree/'compile_commands.json').write_text(json.dumps(entries))
        return tree

    def run_main(self, *args):
        with contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()) as err, \
                mock.patch.object(diag, 'ROOT', self.folder), \
                mock.patch.object(diag, 'NARROWING_ALLOWLIST', self.narrowing_allowed), \
                mock.patch.object(diag, 'UNPROTOTYPED_ALLOWLIST', self.unprototyped_allowed):
            status = diag.main([str(arg) for arg in args])
        return status, out.getvalue(), err.getvalue()

    def warning(self, source: str, line: int, text: str, quoted: str = '') -> str:
        shown = f"    {line} | {quoted}\n" if quoted else ''
        return f"{self.folder}/{source}:{line}:7: warning: {text} [-Wconversion]\n{shown}"


class Narrowing(TreeCase):
    def conversion(self, source: str, line: int, from_type: str, quoted: str = 'x = p;') -> str:
        return self.warning(source, line, f"conversion from {from_type} to {U32} may change value", quoted)

    def test_a_uintptr_t_narrowed_to_u32_is_listed_with_its_source_text(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': "0\n" + self.conversion('a.c', 4, "'uintptr_t' {aka 'long unsigned int'}", 'n = p;')})
        # Act
        status, out, err = self.run_main('narrowing', tree)
        # Assert
        self.assertEqual(status, 0, err)
        self.assertEqual(out.strip(), 'a.c:4:7: uintptr_t -> u32 | n = p;')

    def test_intptr_t_and_romptr_t_sources_are_listed_too(self):
        # Arrange
        text = "0\n" + self.conversion('a.c', 1, "'intptr_t' {aka 'long int'}") + \
            self.conversion('a.c', 2, "'romptr_t' {aka 'long unsigned int'}")
        tree = self.build_dir('t', {'a.c': text})
        # Act
        status, out, err = self.run_main('narrowing', tree)
        # Assert
        self.assertEqual([line.split(' | ')[0] for line in out.splitlines()],
                         ['a.c:1:7: intptr_t -> u32', 'a.c:2:7: romptr_t -> u32'])

    def test_a_long_source_is_listed_only_with_all(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': "0\n" + self.conversion('a.c', 3, "'long int'")})
        # Act
        plain = self.run_main('narrowing', tree)
        every = self.run_main('narrowing', '--all', tree)
        # Assert
        self.assertEqual(plain[1], '')
        self.assertEqual(every[1].split(' | ')[0], 'a.c:3:7: long int -> u32')

    def test_all_lists_a_size_t_and_a_64_bit_fixed_type(self):
        # Arrange
        text = "0\n" + self.conversion('a.c', 1, "'size_t' {aka 'long unsigned int'}") + \
            self.conversion('a.c', 2, "'u64' {aka 'long long unsigned int'}")
        tree = self.build_dir('t', {'a.c': text})
        # Act
        status, out, err = self.run_main('narrowing', '--all', tree)
        # Assert
        self.assertEqual(len(out.splitlines()), 2)

    def test_a_site_two_trees_both_see_is_listed_once(self):
        # Arrange
        files = {'a.c': "0\n" + self.conversion('a.c', 4, "'uintptr_t' {aka 'long unsigned int'}")}
        first, second = self.build_dir('one', files), self.build_dir('two', files)
        # Act
        status, out, err = self.run_main('narrowing', first, second)
        # Assert
        self.assertEqual(len(out.splitlines()), 1)

    def test_a_compile_that_fails_is_not_measured(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': f"1\n{self.folder}/a.c:1:1: error: broken\n"})
        # Act
        status, out, err = self.run_main('narrowing', tree)
        # Assert
        self.assertEqual(status, 2)
        self.assertIn('a.c', err)

    def check(self, allowed: str, text: str):
        self.narrowing_allowed.write_text(allowed)
        tree = self.build_dir('t', {'a.c': "0\n" + text, 'b.c': "0\n"})
        return self.run_main('narrowing', '--check', tree)

    def test_check_passes_when_every_count_matches(self):
        # Arrange
        text = self.conversion('a.c', 1, "'uintptr_t' {aka 'long unsigned int'}") + \
            self.conversion('a.c', 2, "'uintptr_t' {aka 'long unsigned int'}")
        # Act
        status, out, err = self.check('# note\na.c\t2\texisting before LP64-003\n', text)
        # Assert
        self.assertEqual(status, 0, out + err)
        self.assertIn('OK: 2 narrowings in 1 files', out)

    def test_check_fails_naming_a_file_with_more_than_allowed(self):
        # Arrange
        text = self.conversion('a.c', 1, "'uintptr_t' {aka 'long unsigned int'}") + \
            self.conversion('a.c', 2, "'uintptr_t' {aka 'long unsigned int'}")
        # Act
        status, out, err = self.check('a.c\t1\texisting\n', text)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('a.c: 2 narrowings, allowed 1', out)

    def test_check_fails_naming_a_file_with_fewer_than_allowed(self):
        # Arrange
        text = self.conversion('a.c', 1, "'uintptr_t' {aka 'long unsigned int'}")
        # Act
        status, out, err = self.check('a.c\t3\texisting\n', text)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('a.c: 1 narrowings, allowed 3; lower the allowlist line to 1', out)

    def test_check_fails_for_an_allowlisted_file_that_has_none(self):
        # Act
        status, out, err = self.check('b.c\t1\texisting\n', '')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('b.c: 0 narrowings, allowed 1; remove the allowlist line', out)

    def test_check_does_not_count_the_all_only_types(self):
        # Arrange
        text = self.conversion('a.c', 1, "'long int'")
        # Act
        status, out, err = self.check('', text)
        # Assert
        self.assertEqual(status, 0, out + err)

    def test_check_treats_a_missing_allowlist_as_empty(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': "0\n", 'b.c': "0\n"})
        # Act
        status, out, err = self.run_main('narrowing', '--check', tree)
        # Assert
        self.assertEqual(status, 0, out + err)
        self.assertIn('OK: 0 narrowings in 0 files', out)

    def test_check_against_a_missing_allowlist_fails_naming_each_place(self):
        # Arrange
        text = self.conversion('a.c', 1, "'uintptr_t' {aka 'long unsigned int'}")
        tree = self.build_dir('t', {'a.c': "0\n" + text})
        # Act
        status, out, err = self.run_main('narrowing', '--check', tree)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('a.c: 1 narrowings, allowed 0', out)
        self.assertIn('  a.c:1:', out)

    def test_a_malformed_allowlist_line_exits_2(self):
        # Act
        status, out, err = self.check('a.c\tmany\texisting\n', '')
        # Assert
        self.assertEqual(status, 2)
        self.assertIn('allowlist line 1', err)


class Unprototyped(TreeCase):
    def implicit(self, source: str, line: int, name: str) -> str:
        return (f"{self.folder}/{source}:{line}:3: warning: implicit declaration of function '{name}' "
                "[-Wimplicit-function-declaration]\n")

    def test_a_call_to_a_function_taking_a_pointer_is_listed_with_its_prototype(self):
        # Arrange
        text = "0\n" + self.implicit('a.c', 5, 'f') + "--aux--\n/* a.c:1:NF */ extern void f (char *); \n"
        tree = self.build_dir('t', {'a.c': text})
        # Act
        status, out, err = self.run_main('unprototyped', tree)
        # Assert
        self.assertEqual(status, 0, err)
        self.assertEqual(out.strip(), 'a.c:5:3: f | extern void f (char *)')

    def test_a_call_to_a_function_returning_a_pointer_is_listed(self):
        # Arrange
        text = "0\n" + self.implicit('a.c', 5, 'f') + "--aux--\n/* a.c:1:NF */ extern char *f (int); \n"
        tree = self.build_dir('t', {'a.c': text})
        # Act
        status, out, err = self.run_main('unprototyped', tree)
        # Assert
        self.assertIn('a.c:5:3: f | extern char *f (int)', out)

    def test_a_call_to_a_function_taking_a_64_bit_integer_is_listed(self):
        # Arrange
        text = "0\n" + self.implicit('a.c', 5, 'f') + "--aux--\n/* a.c:1:NF */ extern void f (long unsigned int); \n"
        tree = self.build_dir('t', {'a.c': text})
        # Act
        status, out, err = self.run_main('unprototyped', tree)
        # Assert
        self.assertIn('a.c:5:3: f |', out)

    def test_a_call_to_a_function_of_plain_ints_is_not_listed(self):
        # Arrange
        text = "0\n" + self.implicit('a.c', 5, 'g') + "--aux--\n/* a.c:1:NF */ extern int g (int); \n"
        tree = self.build_dir('t', {'a.c': text})
        # Act
        status, out, err = self.run_main('unprototyped', tree)
        # Assert
        self.assertEqual((status, out), (0, ''))

    def test_a_name_with_no_definition_in_the_tree_is_not_listed(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': "0\n" + self.implicit('a.c', 5, 'printf') + "--aux--\n"})
        # Act
        status, out, err = self.run_main('unprototyped', tree)
        # Assert
        self.assertEqual((status, out), (0, ''))

    def test_a_declaration_without_a_definition_does_not_count(self):
        # Arrange: `C` marks a declaration, `F` a definition
        text = "0\n" + self.implicit('a.c', 5, 'f') + "--aux--\n/* a.c:1:NC */ extern void f (char *); \n"
        tree = self.build_dir('t', {'a.c': text})
        # Act
        status, out, err = self.run_main('unprototyped', tree)
        # Assert
        self.assertEqual(out, '')

    def test_a_definition_in_another_file_is_found(self):
        # Arrange
        tree = self.build_dir('t', {
            'a.c': "0\n" + self.implicit('a.c', 5, 'f') + "--aux--\n",
            'b.c': "0\n--aux--\n/* b.c:9:NF */ extern void f (char *); \n"})
        # Act
        status, out, err = self.run_main('unprototyped', tree)
        # Assert
        self.assertEqual(out.strip(), 'a.c:5:3: f | extern void f (char *)')

    def test_cpp_files_are_not_compiled(self):
        # Arrange
        tree = self.build_dir('t', {'a.cpp': "1\n"})
        # Act
        status, out, err = self.run_main('unprototyped', tree)
        # Assert
        self.assertEqual(status, 2)
        self.assertIn('no C compile commands', err)

    def check(self, allowed: str):
        self.unprototyped_allowed.write_text(allowed)
        text = "0\n" + self.implicit('a.c', 5, 'f') + self.implicit('a.c', 8, 'f') + \
            "--aux--\n/* a.c:1:NF */ extern void f (char *); \n"
        return self.run_main('unprototyped', '--check', self.build_dir('t', {'a.c': text}))

    def test_check_passes_when_the_count_matches(self):
        # Act
        status, out, err = self.check('a.c\t2\texisting before LP64-003\n')
        # Assert
        self.assertEqual(status, 0, out + err)
        self.assertIn('OK: 2 unprototyped calls in 1 files', out)

    def test_check_fails_when_the_count_differs(self):
        # Act
        status, out, err = self.check('a.c\t1\texisting\n')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('a.c: 2 unprototyped calls, allowed 1', out)


if __name__ == '__main__':
    unittest.main()
