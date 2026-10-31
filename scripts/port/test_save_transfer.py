"""Exercise v1 interoperability, validation and interrupted-restore recovery."""
import base64
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

DRIVER = str(Path(sys.argv.pop(1)).resolve())


def backup(files):
    return json.dumps(dict(format='project-midas-saves', version=1, files=[
        dict(path=path, data=base64.b64encode(data).decode(), sha256=hashlib.sha256(data).hexdigest())
        for path, data in files.items()]))


class Transfer(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.old = {'Data/Saves/eeprom.bin': bytes(range(256)) * 2,
                    'midas.ini': b'[display]\nalways_health=0\n',
                    'Data/Profiles/Old.geprofile': b'old-profile'}
        for path, data in self.old.items():
            self.put(path, data)
        self.new = {'Data/Saves/eeprom.bin': bytes(reversed(range(256))) * 2,
                    'midas.ini': b'[display]\nalways_health=1\n',
                    'Data/Profiles/Web!.geprofile': b'web-profile\x00\xff',
                    'Data/Devices/kbm.gedevice': b'[binds]\n',
                    'gamecontrolleruser.txt': b'controller-map\n'}

    def put(self, path, data):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)

    def run_driver(self, command, *args, success=True):
        result = subprocess.run([DRIVER, command, *map(str, args)], capture_output=True)
        self.assertEqual(result.returncode == 0, success, result.stderr.decode())
        return result.stdout

    def stage(self, files):
        self.put('input.json', backup(files).encode())
        self.run_driver('stage', self.root, self.root / 'input.json')

    def assert_files(self, files):
        actual = json.loads(self.run_driver('export', self.root))
        self.assertEqual({record['path']: base64.b64decode(record['data']) for record in actual['files']}, files)

    def test_export_checksums_and_allowlist(self):
        # Arrange: unrelated private data must never enter an export.
        self.put('Data/Saves/States/private.bin', b'suspend')
        self.put('Data/Profiles/Old.gestats', b'local-stats')
        self.put('baserom.u.z64', b'private-rom')
        # Act
        result = json.loads(self.run_driver('export', self.root))
        # Assert
        self.assertEqual(result['format'], 'project-midas-saves')
        self.assertEqual(result['version'], 1)
        self.assertEqual(len(result['files']), len(self.old))
        for record in result['files']:
            data = self.old[record['path']]
            self.assertEqual(base64.b64decode(record['data']), data)
            self.assertEqual(record['sha256'], hashlib.sha256(data).hexdigest())

    def test_staging_preserves_live_data_then_replaces_on_startup(self):
        # Arrange
        self.put('Data/Profiles/Old.gestats', b'keep')
        # Act / Assert
        self.stage(self.new)
        self.assert_files(self.old)
        self.run_driver('apply', self.root)
        self.assert_files(self.new)
        self.assertEqual((self.root / 'Data/Profiles/Old.gestats').read_bytes(), b'keep')
        self.assertFalse((self.root / '.pending-save-restore.json').exists())
        self.run_driver('apply', self.root)
        self.assert_files(self.new)

    def test_interrupted_restore_recovers_preimage_before_retry(self):
        # Arrange: an interrupted write, plus a durable pre-image and pending input.
        self.stage(self.new)
        self.put('.previous-save-restore.json', backup(self.old).encode())
        self.put('.save-restore-in-progress', b'restore\n')
        self.put('Data/Saves/eeprom.bin', b'partial')
        # Act
        self.run_driver('apply', self.root)
        # Assert
        self.assert_files(self.new)
        self.assertFalse((self.root / '.save-restore-in-progress').exists())

    def test_write_failure_rolls_back(self):
        # Arrange: an incoming path cannot be created beneath a regular file.
        self.stage(self.new)
        self.put('Data/Devices', b'blocked')
        # Act
        self.run_driver('apply', self.root, success=False)
        # Assert (inspect bytes directly because this malformed directory blocks collection)
        for path, data in self.old.items():
            self.assertEqual((self.root / path).read_bytes(), data)

    @unittest.skipUnless(sys.platform == 'win32', 'Windows read-only files prevent deletion')
    def test_partial_replacement_failure_keeps_recovery_copy(self):
        # Arrange: Windows refuses deletion after earlier save files were removed.
        self.stage(self.new)
        locked = self.root / 'Data/Profiles/Old.geprofile'
        locked.chmod(0o444)
        try:
            # Act: both replacement and immediate rollback hit the read-only file.
            self.run_driver('apply', self.root, success=False)
            # Assert: a complete pre-image remains for the next startup.
            self.assertTrue((self.root / '.save-restore-in-progress').exists())
            saved = json.loads((self.root / '.previous-save-restore.json').read_text())
            self.assertEqual({r['path']: base64.b64decode(r['data']) for r in saved['files']}, self.old)
        finally:
            locked.chmod(0o666)
        self.run_driver('apply', self.root)
        self.assert_files(self.new)
        self.assertFalse((self.root / '.save-restore-in-progress').exists())

    def test_invalid_inputs_preserve_local_files(self):
        valid = json.loads(backup(self.new))
        cases = ['{}', '{', '[]', backup({'../escape': b'bad'}),
                 backup({'Data/Saves/eeprom.bin': b'short'}),
                 backup({'Data/Profiles/CON.geprofile': b'bad'}),
                 backup({'Data/Profiles/none.geprofile': b'bad'}),
                 backup({'Data/Profiles/TOO-LONG-PROFILE-NAME-12345.geprofile': b'bad'})]
        for field, value in [('sha256', 'bad'), ('data', 'AA=A'), ('data', 'AB==')]:
            changed = json.loads(json.dumps(valid))
            changed['files'][0][field] = value
            cases.append(json.dumps(changed))
        changed = json.loads(json.dumps(valid))
        changed['files'].append(changed['files'][0])
        cases.append(json.dumps(changed))
        cases += [backup({'midas.ini': b'x' * (256 * 1024 + 1)}),
                  backup({f'Data/Devices/{i}.gedevice': b'' for i in range(257)}),
                  '[' * 10000 + ']' * 10000, backup(self.new) + '{}']
        for text in cases:
            with self.subTest(text=text[:80]):
                self.put('bad.json', text.encode())
                self.run_driver('stage', self.root, self.root / 'bad.json', success=False)
                self.assert_files(self.old)
                self.assertFalse((self.root / '.pending-save-restore.json').exists())

    def test_second_pending_import_is_refused(self):
        self.stage(self.new)
        self.put('second.json', backup(self.old).encode())
        self.run_driver('stage', self.root, self.root / 'second.json', success=False)
        self.run_driver('apply', self.root)
        self.assert_files(self.new)

    def test_case_conflicts_rejected(self):
        self.put('bad.json', backup({'Data/Profiles/A.geprofile': b'a', 'Data/Profiles/a.geprofile': b'b'}).encode())
        self.run_driver('validate', self.root / 'bad.json', success=False)

    @unittest.skipUnless(sys.platform == 'win32', 'Windows Unicode filename comparison')
    def test_unicode_case_conflicts_rejected_before_staging(self):
        self.put('bad.json', backup({'Data/Profiles/René.geprofile': b'a',
                                     'Data/Profiles/RENÉ.geprofile': b'b'}).encode())
        self.run_driver('stage', self.root, self.root / 'bad.json', success=False)
        self.assert_files(self.old)
        self.assertFalse((self.root / '.pending-save-restore.json').exists())

    def test_uncommitted_marker_temps_do_not_block_restart(self):
        self.stage(self.new)
        for name in ['.pending-save-restore.json', '.previous-save-restore.json', '.save-restore-in-progress']:
            self.put(name + '.tmp', b'partial')
        self.run_driver('apply', self.root)
        self.assert_files(self.new)
        self.assertFalse(list(self.root.glob('.*.tmp')))

    def test_size_and_format_boundaries(self):
        cases = [backup({'midas.ini': b'x' * (256 * 1024)}), backup({})]
        for text in cases:
            self.put('boundary.json', text.encode())
            self.run_driver('validate', self.root / 'boundary.json')
        invalid = json.loads(backup(self.new))
        invalid['version'] = 2
        self.put('boundary.json', json.dumps(invalid).encode())
        self.run_driver('validate', self.root / 'boundary.json', success=False)
        self.put('boundary.json', backup({f'Data/Devices/{i}.gedevice': b'x' * (256 * 1024)
                                        for i in range(17)}).encode())
        self.run_driver('validate', self.root / 'boundary.json', success=False)
        self.assert_files(self.old)

    @unittest.skipIf(sys.platform == 'win32', 'Creating Windows symlinks requires optional privileges')
    def test_linked_save_directories_cannot_escape_root(self):
        outside = self.root / 'outside'
        outside.mkdir()
        (outside / 'kbm.gedevice').write_bytes(b'private')
        (self.root / 'Data/Devices').symlink_to(outside, target_is_directory=True)
        self.run_driver('export', self.root, success=False)
        self.put('input.json', backup(self.new).encode())
        self.run_driver('stage', self.root, self.root / 'input.json')
        self.run_driver('apply', self.root, success=False)
        self.assertEqual((outside / 'kbm.gedevice').read_bytes(), b'private')
        for path, data in self.old.items():
            self.assertEqual((self.root / path).read_bytes(), data)


if __name__ == '__main__':
    unittest.main()
