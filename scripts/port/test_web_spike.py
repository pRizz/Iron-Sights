"""ROM-free tests for survey command preservation and immutable SDK selection."""
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from web_spike import load_toolchain
from web_spike_audit import DIAGNOSTIC_COMPATIBILITY, diagnostics, run_audit, survey_command


class WebSpikeTests(unittest.TestCase):
    def test_survey_preserves_source_defines_and_quoted_include(self):
        # Arrange
        entry = dict(command='emcc -DVERSION_US -I"/src/path with spaces" '
                     '-m32 -msse2 -mfpmath=sse -fwrapv -c /src/boss.c -o old.o')
        # Act
        command, removed = survey_command(entry, Path('/out/boss.o'))
        # Assert
        self.assertEqual(command, ['emcc', '-DVERSION_US', '-I/src/path with spaces',
                                  '-fwrapv', '-c', '/src/boss.c',
                                  *DIAGNOSTIC_COMPATIBILITY, '-o', '/out/boss.o'])
        self.assertEqual(removed, ['-m32', '-msse2', '-mfpmath=sse'])

    def test_survey_removes_only_documented_assembler_include_and_layout_flags(self):
        # Arrange
        entry = dict(command='emcc -malign-data=abi -fno-toplevel-reorder '
                     '-Wa,-I/src/assets -fno-strict-aliasing -c /src/segments.c')
        # Act
        command, removed = survey_command(entry, Path('/out/segments.o'))
        # Assert
        self.assertEqual(len(removed), 3)
        self.assertIn('-fno-strict-aliasing', command)
        self.assertIn('/src/segments.c', command)

    def test_errors_are_deduplicated_without_promoting_warnings(self):
        # Arrange
        output = 'a: error: incompatible type\nb: warning: implicit function\n' \
                 'c: error: incompatible type\nd: fatal error: missing.h file not found\n'
        # Act
        errors = diagnostics(output)
        # Assert
        self.assertEqual(errors, ['incompatible type', 'missing.h file not found'])

    def test_toolchain_rejects_mutable_or_mismatched_image(self):
        # Arrange
        config = dict(schema=1, sdk_version='6.0.13', image='emscripten/emsdk:latest')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'toolchain.json'
            path.write_text(json.dumps(config), encoding='utf-8')
            # Act / Assert
            with self.assertRaisesRegex(ValueError, 'immutable digest'):
                load_toolchain(path)
            config['image'] = 'emscripten/emsdk:6.0.12@sha256:' + 'a' * 64
            path.write_text(json.dumps(config), encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'immutable digest'):
                load_toolchain(path)

    def test_repository_toolchain_is_pinned(self):
        # Arrange / Act
        config = load_toolchain()
        # Assert
        self.assertEqual(config['sdk_version'], '6.0.13')
        self.assertIn('@sha256:', config['image'])

    def test_empty_source_set_cannot_pass_the_survey(self):
        # Arrange
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            database = root / 'compile_commands.json'
            database.write_text('[]', encoding='utf-8')
            # Act / Assert
            with self.assertRaisesRegex(ValueError, 'no sources'):
                run_audit(database, root / 'audit', root, 1)

    def test_mixed_survey_keeps_failure_status_and_per_source_evidence(self):
        # Arrange
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            entries = [dict(file=str(root / name), directory=str(root),
                            command=shlex.join(['emcc', '-c', str(root / name)]))
                       for name in ('good.c', 'bad.c')]
            database = root / 'compile_commands.json'
            database.write_text(json.dumps(entries), encoding='utf-8')

            def compiler(command, **kwargs):
                failed = any(token.endswith('bad.c') for token in command)
                return subprocess.CompletedProcess(command, int(failed),
                    stdout='error: missing declaration\n' if failed else '')

            # Act
            with patch('web_spike_audit.subprocess.run', side_effect=compiler):
                result = run_audit(database, root / 'audit', root, 2)
            # Assert
            self.assertEqual((result['total'], result['compiled'], result['failed']), (2, 1, 1))
            failure = result['sources'][0]
            self.assertEqual(failure['source'], 'bad.c')
            self.assertEqual(failure['errors'], ['missing declaration'])
            self.assertIn('missing declaration', (root / failure['log']).read_text())
            self.assertEqual(json.loads((root / 'audit.json').read_text())['failed'], 1)


if __name__ == '__main__':
    unittest.main()
