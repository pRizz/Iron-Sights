"""ROM-free tests for lp64_lto.py, the link-time type mismatch list and its allowlist."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('lp64_lto', Path(__file__).with_name('lp64_lto.py'))
lto = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lto)

ROOT = '/home/dev/project'
# What GCC 13.3 prints for a return type that differs (copied from a scratch link's log).
RETURN_TYPE = f"""{ROOT}/src/game/fr.c:12:14: warning: type of 'viGetPerspNorm' does not match original declaration [-Wlto-type-mismatch]
   12 | extern float viGetPerspNorm(void);
      |              ^
{ROOT}/src/game/gunfire.c:40:5: note: return value type mismatch
   40 | u16 viGetPerspNorm(void) {{
      |     ^
{ROOT}/src/game/gunfire.c:40:5: note: type 'short unsigned int' should match type 'float'
{ROOT}/src/game/gunfire.c:40:5: note: 'viGetPerspNorm' was previously declared here
"""
ARRAY = f"""{ROOT}/port/src/animdata.c:66:17: warning: type of 'animation_table_ptrs2' does not match original declaration [-Wlto-type-mismatch]
   66 | extern intptr_t animation_table_ptrs2[];
      |                 ^
{ROOT}/src/game/initanitable.c:220:25: note: 'animation_table_ptrs2' was previously declared here
  220 | struct ModelAnimation *animation_table_ptrs2[] = {{
      |                         ^
{ROOT}/src/game/initanitable.c:220:25: note: code may be misoptimized unless '-fno-strict-aliasing' is used
"""

LINKED = '[100%] Built target ge\n'


def write(folder: Path, name: str, text: str) -> Path:
    path = folder/name
    path.write_text(text)
    return path


class ListMismatches(unittest.TestCase):
    def test_list_reads_a_return_type_mismatch(self):
        # Act
        found = lto.parse_log(RETURN_TYPE, ROOT)
        # Assert
        self.assertEqual(lto.format_lines(found), [
            "viGetPerspNorm: src/game/fr.c:12 extern float viGetPerspNorm(void); | "
            "src/game/gunfire.c:40 u16 viGetPerspNorm(void) { -- return value type mismatch; "
            "type 'short unsigned int' should match type 'float'"])

    def test_list_reads_a_variable_whose_note_quotes_the_definition_after_it(self):
        # Act
        found = lto.parse_log(ARRAY, ROOT)
        # Assert
        self.assertEqual(lto.format_lines(found), [
            "animation_table_ptrs2: port/src/animdata.c:66 extern intptr_t animation_table_ptrs2[]; | "
            "src/game/initanitable.c:220 struct ModelAnimation *animation_table_ptrs2[] = {"])

    def test_a_warning_printed_twice_is_listed_once(self):
        # Act
        found = lto.parse_log(RETURN_TYPE + RETURN_TYPE, ROOT)
        # Assert
        self.assertEqual(len(lto.format_lines(found)), 1)


class CheckAllowlist(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)

    def run_check(self, logs: list[str], allowed: str = None) -> tuple[int, str]:
        paths = [write(self.folder, f'build{index}.log', text + LINKED) for index, text in enumerate(logs)]
        allowlist = self.folder/'allowed.txt'
        if allowed is not None:
            allowlist.write_text(allowed)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            status = lto.check(paths, allowlist, ROOT)
        return status, output.getvalue()

    def test_check_fails_an_unlisted_symbol(self):
        # Act
        status, output = self.run_check([RETURN_TYPE], '# none\n')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('viGetPerspNorm: not allowed', output)

    def test_check_fails_a_listed_symbol_that_is_gone(self):
        # Act
        status, output = self.run_check([RETURN_TYPE], 'viGetPerspNorm strict tier\ntanf libm built-in\n')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('tanf: allowed but no longer mismatched; remove its line', output)

    def test_check_passes_when_the_logs_match_the_list_exactly(self):
        # Act
        status, output = self.run_check([RETURN_TYPE, ARRAY],
                                        'viGetPerspNorm strict tier\nanimation_table_ptrs2 array without its bound\n')
        # Assert
        self.assertEqual((status, output.strip()), (0, 'OK: 2 mismatches, all allowed'))

    def test_a_missing_allowlist_allows_none(self):
        # Act
        status, output = self.run_check([ARRAY])
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('animation_table_ptrs2: not allowed', output)

    def test_an_allowlist_line_without_a_reason_is_refused(self):
        # Act / Assert
        with self.assertRaisesRegex(ValueError, 'line 1'):
            lto.parse_allowlist('viGetPerspNorm\n')

    def test_a_log_without_a_finished_link_is_refused(self):
        # Arrange
        log = write(self.folder, 'partial.log', 'ninja: build stopped: subcommand failed.\n')
        # Act / Assert
        with self.assertRaisesRegex(ValueError, 'no finished link'):
            lto.read_logs([log], ROOT)


if __name__ == '__main__':
    unittest.main()
