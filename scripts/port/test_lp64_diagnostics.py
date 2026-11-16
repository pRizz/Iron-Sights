"""ROM-free tests for the 64-bit compile gate and the per-site listing. No real compiler: the
command-line tests run a stand-in compiler, a Python script that prints what its input file asks for."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
from typing import Optional
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


def keys(stderr: str) -> set:
    return set(diag.parse_with_text(stderr, Path('/w')))


class Parse(unittest.TestCase):
    def test_parse_keeps_tracked_kinds_and_errors_once_each(self):
        # Act
        found = keys(SAMPLE)
        # Assert
        self.assertEqual(found, {
            ('src/game/front.c', 1124, 13, 'int-conversion'),
            ('src/game/chr.c', 1700, 29, 'pointer-to-int-cast'),
            ('src/crash.c', 748, 11, 'error')})

    def test_int_to_pointer_cast_is_tracked(self):
        # Arrange
        text = "/w/a.c:3:4: warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]\n"
        # Act / Assert
        self.assertEqual(keys(text), {('a.c', 3, 4, 'int-to-pointer-cast')})

    def test_an_error_with_a_tracked_flag_counts_under_its_kind(self):
        # Arrange: the gate's -Werror= form and GCC 14's default error both name the flag
        for flag in ('-Wint-conversion', '-Werror=int-conversion'):
            text = f"/w/a.c:5:9: error: initialization of 'int' from 'void *' makes integer from pointer without a cast [{flag}]\n"
            with self.subTest(flag=flag):
                # Act / Assert
                self.assertEqual(keys(text), {('a.c', 5, 9, 'int-conversion')})


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


class GateCommand(unittest.TestCase):
    def test_gate_command_keeps_the_entrys_own_flags_and_only_drops_output(self):
        # Arrange
        entry = dict(file='/w/a.c', command='cc -w -Werror=int-conversion -MD -MT a.o -MF a.d -o a.o -c /w/a.c')
        # Act
        argv = diag.gate_command(entry)
        # Assert
        self.assertEqual(argv, ['cc', '-w', '-Werror=int-conversion', '/w/a.c', '-fsyntax-only'])

    def test_gate_command_covers_cxx_and_skips_other_files(self):
        # Act / Assert
        self.assertIsNotNone(diag.gate_command(dict(file='/w/gfx_pc.cpp', command='c++ -c /w/gfx_pc.cpp')))
        self.assertIsNone(diag.gate_command(dict(file='/w/icon.rc', command='windres /w/icon.rc')))


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


FAKE_COMPILER = """import sys
source = next(arg for arg in sys.argv[1:] if arg.endswith(('.c', '.cpp')))
status, _, stderr = open(source).read().partition('\\n')
sys.stderr.write(stderr)
sys.exit(int(status))
"""


LINE_COMPILER = """import sys
source = next(arg for arg in sys.argv[1:] if arg.endswith('.c'))
lines = open(source).read().split('\\n')
logical = 0
for physical, text in enumerate(lines, 1):
    logical += 1
    if text.startswith('#line '):
        logical = int(text.split()[1]) - 1
    if 'BAD' in text:
        # GCC reports the logical line and quotes the file's line of that number.
        quoted = lines[logical - 1] if logical <= len(lines) else ''
        sys.stderr.write(f'{source}:{logical}:1: warning: bad [-Wint-conversion]\\n    {logical} | {quoted}\\n')
"""


C_GATED = ['-Werror=int-conversion', '-Werror=pointer-to-int-cast', '-Werror=int-to-pointer-cast']
CXX_GATED = ['-Werror=int-to-pointer-cast']
GATED_FLAGS = {'.c': C_GATED, '.cpp': CXX_GATED}


class CommandLine(unittest.TestCase):
    """Runs `main` on build directories whose compiler is FAKE_COMPILER: each source holds the exit
    status on its first line and the compiler output to print after it."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        (self.folder/'cc.py').write_text(FAKE_COMPILER)

    def build_dir(self, name: str, files: dict[str, str], bits: int = 64, header: bool = True,
                  flags: Optional[dict[str, list[str]]] = None) -> Path:
        """Make a configured build directory whose compile_commands.json compiles `files` with FAKE_COMPILER.

        Each entry carries the gate's flags for its language (GATED_FLAGS) unless `flags` gives a source's own."""
        tree = self.folder/name
        (tree/'generated').mkdir(parents=True)
        (tree/'CMakeCache.txt').write_text(f'CMAKE_BUILD_TYPE:STRING=Debug\nGE_POINTER_BITS:STRING={bits}\n')
        if header:
            (tree/'generated'/'port_buildinfo.h').write_text('#define X 1\n')
        entries = []
        for source, text in files.items():
            (self.folder/source).write_text(text)
            own = (flags or {}).get(source, GATED_FLAGS[Path(source).suffix])
            entries.append(dict(directory=str(tree), file=str(self.folder/source),
                                arguments=[sys.executable, str(self.folder/'cc.py'), *own, '-o', 'x.o', '-c',
                                           str(self.folder/source)]))
        (tree/'compile_commands.json').write_text(json.dumps(entries))
        return tree

    def run_main(self, *args):
        with contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()) as err, \
                mock.patch.object(diag, 'ROOT', self.folder):
            status = diag.main([str(arg) for arg in args])
        return status, out.getvalue(), err.getvalue()

    def test_gate_exits_1_naming_each_failing_file_and_its_first_error(self):
        # Arrange
        tree = self.build_dir('t', {
            'a.c': "0\n",
            'b.c': f"1\n{self.folder}/b.c:3:1: error: cast to pointer from integer of different size [-Werror=int-to-pointer-cast]\n"
                   f"{self.folder}/b.c:9:1: error: second\n",
            'c.cpp': f"1\n{self.folder}/c.cpp:2:1: error: broken\n"})
        # Act
        status, out, err = self.run_main('gate', tree)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('b.c:3:1: error: cast to pointer from integer of different size', out)
        self.assertNotIn('second', out)
        self.assertIn('c.cpp:2:1: error: broken', out)
        self.assertNotIn('a.c', out)

    def test_gate_exits_0_for_a_clean_directory(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': f"0\n{self.folder}/a.c:1:1: warning: harmless [-Wfoo]\n", 'b.cpp': "0\n"})
        # Act
        status, out, err = self.run_main('gate', tree)
        # Assert
        self.assertEqual(status, 0, out + err)
        self.assertIn('2 files', out)

    def test_gate_exits_2_when_the_generated_header_is_missing(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': "0\n"}, header=False)
        # Act
        status, out, err = self.run_main('gate', tree)
        # Assert
        self.assertEqual(status, 2)
        self.assertIn('--target ge_buildinfo', err)
        self.assertIn(str(tree), err)

    def test_gate_exits_2_for_a_32_bit_configure(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': "0\n"}, bits=32)
        # Act
        status, out, err = self.run_main('gate', tree)
        # Assert
        self.assertEqual(status, 2)
        self.assertIn('GE_POINTER_BITS=64', err)

    def assert_refused_before_compiling(self, tree: Path, source: str, reason: str):
        """Run the gate on `tree`; require exit 2 naming `source` and `reason`, with nothing compiled."""
        # Act
        status, out, err = self.run_main('gate', tree)
        # Assert
        self.assertEqual(status, 2, out + err)
        refusal = next((line for line in err.splitlines() if source in line), '')
        self.assertIn(reason, refusal, err)
        self.assertNotIn('files compiled', out)

    def test_gate_refuses_a_c_entry_lacking_a_werror_flag(self):
        # Arrange: the source would fail if compiled, so a refusal must come first
        tree = self.build_dir('t', {'a.c': "0\n", 'b.c': "1\nb.c:1:1: error: compiled\n"},
                              flags={'b.c': ['-Werror=int-conversion', '-Werror=int-to-pointer-cast']})
        # Act / Assert
        self.assert_refused_before_compiling(tree, 'b.c', 'lacks -Werror=pointer-to-int-cast')

    def test_gate_refuses_a_c_entry_carrying_w(self):
        # Arrange
        tree = self.build_dir('t', {'b.c': "1\nb.c:1:1: error: compiled\n"}, flags={'b.c': [*C_GATED, '-w']})
        # Act / Assert
        self.assert_refused_before_compiling(tree, 'b.c', 'carries -w')

    def test_gate_refuses_a_cxx_entry_lacking_int_to_pointer_cast(self):
        # Arrange
        tree = self.build_dir('t', {'b.cpp': "1\nb.cpp:1:1: error: compiled\n"}, flags={'b.cpp': []})
        # Act / Assert
        self.assert_refused_before_compiling(tree, 'b.cpp', 'lacks -Werror=int-to-pointer-cast')

    def test_gate_refuses_a_cxx_entry_carrying_w(self):
        # Arrange
        tree = self.build_dir('t', {'b.cpp': "1\nb.cpp:1:1: error: compiled\n"}, flags={'b.cpp': ['-w', *CXX_GATED]})
        # Act / Assert
        self.assert_refused_before_compiling(tree, 'b.cpp', 'carries -w')

    def test_gate_compiles_every_directory_with_its_own_commands(self):
        # Arrange: one source, clean in the first tree, failing with the second tree's command
        clean = self.build_dir('one', {'a.c': "0\n"})
        failing = self.build_dir('two', {'b.c': "0\n"})
        (self.folder/'bad.py').write_text('import sys\nsys.stderr.write("a.c:1:1: error: only here\\n")\nsys.exit(1)\n')
        entries = json.loads((failing/'compile_commands.json').read_text())
        entries.append(dict(directory=str(failing), file=str(self.folder/'a.c'),
                            arguments=[sys.executable, str(self.folder/'bad.py'), *C_GATED, str(self.folder/'a.c')]))
        (failing/'compile_commands.json').write_text(json.dumps(entries))
        # Act
        status, out, err = self.run_main('gate', clean, failing)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('only here', out)

    def test_count_check_and_update_are_gone(self):
        for command in ('count', 'check', 'update'):
            with self.subTest(command=command), self.assertRaises(SystemExit):
                # Act / Assert
                self.run_main(command)

    def test_a_fatal_error_fails_sites_naming_the_file(self):
        # Arrange
        tree = self.build_dir('t', {
            'a.c': f"0\n{self.folder}/a.c:1:1: warning: x [-Wint-conversion]\n",
            'b.c': f"1\n{self.folder}/b.c:2:10: fatal error: port_buildinfo.h: No such file or directory\n"
                   "compilation terminated.\n"})
        # Act
        status, out, err = self.run_main('sites', '--build-dir', tree)
        # Assert
        self.assertEqual(status, 2)
        self.assertIn('b.c', err)

    def test_an_unparsed_driver_error_fails_sites_even_beside_tracked_warnings(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': f"1\n{self.folder}/a.c:1:1: warning: x [-Wint-conversion]\n"
                                           "gcc: error: unrecognized command-line option '-mfoo'\n"})
        # Act
        status, out, err = self.run_main('sites', '--build-dir', tree)
        # Assert
        self.assertEqual(status, 2)
        self.assertIn('-mfoo', err)

    def test_sites_lists_a_site_that_only_the_second_directory_sees(self):
        # Arrange: the same source compiles cleanly in one configuration and not in the other
        first = self.build_dir('one', {'a.c': "0\n"})
        second = self.build_dir('two', {})
        (self.folder/'video.py').write_text(
            f'import sys\nsys.stderr.write("{self.folder}/a.c:7:1: warning: v [-Wint-to-pointer-cast]\\n")\n')
        (second/'compile_commands.json').write_text(json.dumps([dict(
            directory=str(second), file=str(self.folder/'a.c'),
            arguments=[sys.executable, str(self.folder/'video.py'), str(self.folder/'a.c')])]))
        # Act
        status, out, err = self.run_main('sites', '--build-dir', first, '--build-dir', second)
        # Assert
        self.assertEqual(status, 0, err)
        self.assertEqual(out.strip(), 'a.c:7: int-to-pointer-cast')

    def test_sites_refuses_a_directory_missing_its_generated_header(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': "0\n"}, header=False)
        # Act
        status, out, err = self.run_main('sites', '--build-dir', tree)
        # Assert
        self.assertEqual(status, 2)
        self.assertIn('--target ge_buildinfo', err)

    def test_sites_messages_prints_the_message_and_source_text(self):
        # Arrange
        tree = self.build_dir('t', {'a.c': f"0\n{self.folder}/a.c:3:1: warning: bad thing [-Wint-conversion]\n"
                                           "    3 |   p = 1;\n"})
        # Act
        status, out, err = self.run_main('sites', '--messages', '--build-dir', tree)
        # Assert
        self.assertEqual(status, 0)
        self.assertEqual(out.strip(), 'a.c:3: int-conversion: bad thing | p = 1;')

    def line_file_tree(self) -> Path:
        compiler = self.folder/'linecc.py'  # run directly: the tool inserts options right after argv[0]
        compiler.write_text(f'#!{sys.executable}\n{LINE_COMPILER}')
        compiler.chmod(0o755)
        tree = self.build_dir('t', {})
        source = self.folder/'a.c'
        source.write_text('int a;\nint b;\n#line 100\nBAD here\n')
        (tree/'compile_commands.json').write_text(json.dumps([dict(directory=str(tree), file=str(source),
                                                                   arguments=[str(compiler), str(source)])]))
        return tree

    def test_sites_report_the_physical_line_of_a_file_with_line_directives(self):
        # Arrange
        tree = self.line_file_tree()
        # Act
        status, out, err = self.run_main('sites', '--messages', '--build-dir', tree)
        # Assert
        self.assertEqual(status, 0, err)
        self.assertEqual(out.strip(), 'a.c:4: int-conversion: bad | BAD here')

    def test_sites_without_messages_also_report_the_physical_line(self):
        # Arrange
        tree = self.line_file_tree()
        # Act
        status, out, err = self.run_main('sites', '--build-dir', tree)
        # Assert
        self.assertEqual(out.strip(), 'a.c:4: int-conversion')

    def test_sites_writes_nothing_beside_the_source(self):
        # Arrange
        tree = self.line_file_tree()
        before = sorted(p.name for p in self.folder.iterdir())
        # Act
        self.run_main('sites', '--build-dir', tree)
        # Assert
        self.assertEqual(sorted(p.name for p in self.folder.iterdir()), before)


class PhysicalCommand(unittest.TestCase):
    def test_the_copy_replaces_the_source_and_the_original_folder_is_searched_first(self):
        # Arrange
        argv = ['cc', '-iquote', 'other', '-I/inc', '/w/src/a.c', '-fsyntax-only']
        # Act
        result = diag.physical_command(argv, '/w/src/a.c', '/tmp/x/a.c')
        # Assert
        self.assertEqual(result, ['cc', '-iquote', '/w/src', '-iquote', 'other', '-I/inc', '/tmp/x/a.c', '-fsyntax-only'])

    def test_stripping_line_directives_keeps_every_other_line_in_place(self):
        # Act
        stripped = diag.strip_line_directives('a\n  #  line 100\nb\n#line 7 "f.c"\nc')
        # Assert
        self.assertEqual(stripped, 'a\n\nb\n\nc')
        self.assertTrue(diag.has_line_directives('x\n#line 5\n'))
        self.assertFalse(diag.has_line_directives('// #line 5\nint x;'))


if __name__ == '__main__':
    unittest.main()
