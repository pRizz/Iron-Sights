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


if __name__ == '__main__':
    unittest.main()
