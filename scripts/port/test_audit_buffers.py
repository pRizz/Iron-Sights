"""Check inventory classification and isolated mission audit verdicts."""
import json
from contextlib import redirect_stdout
from io import StringIO
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import audit_buffers as audit


class AuditTests(unittest.TestCase):
    def test_inventory_keeps_candidates_distinct(self):
        record = audit.source_record('port/src/example.c', b'// strcpy(candidate)\nmemcpy(a,b,4);\n')
        self.assertEqual(record['domain'], 'platform-fork')
        self.assertEqual(record['lines'], 2)
        self.assertEqual(record['sites'], [{'line': 1, 'kind': 'unbounded_text'},
                                           {'line': 2, 'kind': 'memory_copy'}])
        self.assertEqual(audit.domain('assets/model.c'), 'generated-assets')
        self.assertEqual(audit.domain('src/libultra/audio/load.c'), 'inherited-audio-library')
        self.assertEqual(audit.domain('port/web/input.c'), 'web-native-bridge')

    def test_mission_requires_launch_completion_and_clean_sanitizers(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            executable, rom = folder/'headless', folder/'rom'
            executable.write_bytes(b'test')
            rom.write_bytes(b'test')
            child_paths = []

            def run(args, cwd, env, stdout, **kwargs):
                child_paths.append(Path(args[0]))
                self.assertEqual(Path(cwd), Path(args[0]).parent)
                self.assertNotEqual(Path(cwd), folder)
                self.assertEqual(env['GE_STAGE'], 'dam')
                self.assertNotIn('GE_INPUTPLAY', env)
                stdout.write(sample)
                return type('Result', (), {'returncode': 0})()

            for sample, expected in [
                ('GE_STAGE direct launch: dam\nframe limit reached: frames=900', True),
                ('frame limit reached: frames=900', False),
                ('GE_STAGE direct launch: dam\nruntime error: bad', False),
                ('GE_STAGE direct launch: dam\nframe limit reached: frames=899', False),
            ]:
                with patch.object(audit, 'MISSIONS', ('dam',)), \
                        patch.object(audit.subprocess, 'run', side_effect=run), \
                        patch.dict(os.environ, {'GE_INPUTPLAY': 'user-input'}), redirect_stdout(StringIO()):
                    status = audit.mission_smoke(executable, rom, folder/'results.json', 900, 1)
                self.assertEqual(status, 0 if expected else 1)
                self.assertEqual(json.loads((folder/'results.json').read_text())[0]['passed'], expected)
            self.assertTrue(all(not path.exists() for path in child_paths))
            self.assertEqual(executable.read_bytes(), b'test')

    def smoke_env(self, **options):
        """The environment the one-mission smoke gives its child, with OPTIONS for mission_smoke."""
        seen = {}

        def run(args, cwd, env, stdout, **kwargs):
            seen.update(env)
            stdout.write('GE_STAGE direct launch: dam\nframe limit reached: frames=600')
            return type('Result', (), {'returncode': 0})()

        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            (folder/'headless').write_bytes(b'test')
            (folder/'rom').write_bytes(b'test')
            with patch.object(audit, 'MISSIONS', ('dam',)), patch.object(audit.subprocess, 'run', side_effect=run), \
                    redirect_stdout(StringIO()):
                audit.mission_smoke(folder/'headless', folder/'rom', folder/'results.json', 600, 1, **options)
        return seen

    def test_fixed_tick_runs_one_field_a_frame_with_the_census_lines(self):
        # Arrange: lp64_census.py compares two smokes, which load the same only on the same game time

        # Act
        seen = self.smoke_env(fixed_tick=True)

        # Assert
        self.assertEqual((seen['GE_FIXEDTICK'], seen['GE_LP64_CENSUS']), ('1', '1'))

    def test_default_smoke_keeps_the_game_clock(self):
        # Act
        seen = self.smoke_env()

        # Assert
        self.assertEqual(('GE_FIXEDTICK' in seen, 'GE_LP64_CENSUS' in seen), (False, False))

    def test_leak_mode_enables_lsan_and_rejects_leaks(self):
        self.assertEqual(audit.sanitizer_env(False), {'ASAN_OPTIONS': 'detect_leaks=0'})
        leaks = audit.sanitizer_env(True)
        self.assertEqual(leaks['ASAN_OPTIONS'], 'detect_leaks=1')
        self.assertIn('suppressions=' + str(audit.LSAN_SUPPRESSIONS), leaks['LSAN_OPTIONS'])
        self.assertTrue(audit.LSAN_SUPPRESSIONS.is_file())
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            executable, rom = folder/'headless', folder/'rom'
            executable.write_bytes(b'test')
            rom.write_bytes(b'test')

            def run(args, cwd, env, stdout, **kwargs):
                self.assertEqual(env['ASAN_OPTIONS'], 'detect_leaks=1')
                stdout.write(sample)
                return type('Result', (), {'returncode': 0})()

            for sample, expected in [
                ('GE_STAGE direct launch: dam\nframe limit reached: frames=900', True),
                ('GE_STAGE direct launch: dam\nframe limit reached: frames=900\n'
                 '==1==ERROR: LeakSanitizer: detected memory leaks', False),
            ]:
                with patch.object(audit, 'MISSIONS', ('dam',)), \
                        patch.object(audit.subprocess, 'run', side_effect=run), redirect_stdout(StringIO()):
                    status = audit.mission_smoke(executable, rom, folder/'results.json', 900, 1, leaks=True)
                self.assertEqual(status, 0 if expected else 1)


CLEAN_LOG = 'GE_STAGE direct launch: dam\nframe limit reached: frames=900\n'
POOL_LINE = 'ge: lp64: pool "room memory" exhausted (request 65536 bytes)'
REFUSAL_LINE = 'ge: lp64: cannot convert models "GtriggerZ": too large'


class RefusalLineTests(unittest.TestCase):
    """A 64-bit pool report or converter refusal (LP64-004 Task 2) fails the mission whatever its
    exit status, and the record names the first such line."""

    def smoke(self, sample, returncode=0):
        """Runs the one-mission smoke on SAMPLE as the log of a child that exits RETURNCODE; returns
        the status, the mission's record and what the smoke printed."""
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            executable, rom = folder/'headless', folder/'rom'
            executable.write_bytes(b'test')
            rom.write_bytes(b'test')

            def run(args, cwd, env, stdout, **kwargs):
                stdout.write(sample)
                return type('Result', (), {'returncode': returncode})()

            printed = StringIO()
            with patch.object(audit, 'MISSIONS', ('dam',)), \
                    patch.object(audit.subprocess, 'run', side_effect=run), redirect_stdout(printed):
                status = audit.mission_smoke(executable, rom, folder/'results.json', 900, 1)
            record = json.loads((folder/'results.json').read_text())[0]
        return status, record, printed.getvalue()

    def test_pool_line_fails_the_mission_naming_the_line(self):
        # Arrange
        sample = CLEAN_LOG + POOL_LINE + '\n' + POOL_LINE.replace('65536', '128') + '\n'

        # Act
        status, record, printed = self.smoke(sample)

        # Assert
        self.assertEqual((status, record['passed'], record['refusal']), (1, False, POOL_LINE))
        self.assertIn('dam: FAIL (' + POOL_LINE + ')', printed)

    def test_refused_conversion_fails_the_mission(self):
        # Arrange
        sample = CLEAN_LOG + REFUSAL_LINE + '\n'

        # Act
        status, record, _ = self.smoke(sample)

        # Assert
        self.assertEqual((status, record['passed'], record['refusal']), (1, False, REFUSAL_LINE))

    def test_clean_log_passes(self):
        # Act
        status, record, printed = self.smoke(CLEAN_LOG)

        # Assert
        self.assertEqual((status, record['passed'], record['refusal']), (0, True, None))
        self.assertIn('dam: PASS', printed)

    def test_informational_lp64_lines_pass(self):
        # Arrange: the reading of owner decision 3 (FORK-089, LP64-004 task 16): every 64-bit run
        # prints these, and none fails a mission
        informational = ('ge: lp64: address space OK: executable image 0x5555 stack 0x7fff\n'
                         'ge: lp64: audio bank sfxctl: 16448 bytes of run-time records\n'
                         'ge: lp64: audio bank instrumentsctl: 10520 bytes of run-time records\n'
                         'ge: lp64: music sequence table musicsampletbl: 63 sequences, 1016 bytes\n'
                         'ge: lp64: bg tables bg/bg_dam_all_p.seg: 139 room records, 195 portal entries, '
                         '8688 bytes of the stage pool\n')

        # Act
        status, record, _ = self.smoke(CLEAN_LOG + informational)

        # Assert
        self.assertEqual((status, record['passed']), (0, True))

    def test_each_failing_lp64_line_fails_the_mission(self):
        # Arrange: the lines a run that exits 0 can still print. The others end the process with a
        # non-zero status, which fails the mission without a marker: `stopping:` (exit 5), the
        # address check's lines (exit 4), `converter scratch` and `converter map: cannot reserve`
        # (EXIT_FAILURE). `converter map: a second map` is followed by the `cannot convert` line.
        failing = (POOL_LINE, REFUSAL_LINE)
        results = []

        # Act
        for line in failing:
            status, record, _ = self.smoke(CLEAN_LOG + line + '\n')
            results.append((status, record['passed'], record['refusal']))

        # Assert
        self.assertEqual(results, [(1, False, line) for line in failing])

    def test_a_stop_fails_the_mission_by_its_exit_status(self):
        # Arrange
        stop = 'ge: lp64: stopping: asset class "models" has no 64-bit converter yet (LP64-004)\n'

        # Act
        status, record, _ = self.smoke('GE_STAGE direct launch: dam\n' + stop, returncode=5)

        # Assert
        self.assertEqual((status, record['passed'], record['exit']), (1, False, 5))

    def test_markers_are_the_boot_checks(self):
        # Act
        markers = audit.REFUSAL_MARKERS

        # Assert
        self.assertIs(markers, audit.lp64_boot.REFUSAL_MARKERS)


if __name__ == '__main__':
    unittest.main()
