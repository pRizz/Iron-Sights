"""ROM-free tests for lp64_setup_scan.py: setups and prop models built here from invented numbers (LP64-005 Task 3)."""
import contextlib
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_setup_scan as scan  # noqa: E402
from test_lp64_model_scan import compress, invented_gun  # noqa: E402

KINDS = '''static const struct setupkinds kSetupPropdefKinds[SETUP_PROPDEF_TYPES] = {
    /*  1 DOOR    */ {   4, { 4, 2, 1, 1 } },
    /*  3 PROP    */ {   3, { 4, 2, 1 } },
    /* 10 MONITOR */ {   4, { 4, 2, 1, 1 } },
    /* 41 UNK41   */ {   1, { 1 } },
    /* 48 END     */ {   1, { 4 } },
};
static const struct setupkinds kSetupIntroKinds[SETUP_INTRO_TYPES] = {
    /*  1 ITEM    */ {   4, { 1, 1, 1, 1 } },
};
'''


def record(kind, model=0, *words):
    return [kind, model << 16] + list(words)


def invented_setup(end=48):
    """DOOR naming model 0, PROP naming model 1, MONITOR whose word 3 names the command two back, UNK41, END."""
    words = record(1, 0, 0, 0) + record(3, 1, 0) + record(10, 0, 0, -2) + [41] + [end]
    return struct.pack('>4i', 0, 0, 0, 16) + struct.pack('>%di' % len(words), *words)


class Fixture(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.write(invented_setup())
        assets = self.root/'assets'
        prop = assets/'prop'
        (prop/'foo').mkdir(parents=True)
        (prop/'bar').mkdir()
        (prop/'propItemModelFileRecord.inc.c').write_text(
            'ItemModelFileRecord PitemZ_entries[] = {\n'
            '    #include <assets/obseg/prop/foo/propFileRecord.inc.c>\n'
            '    #include <assets/obseg/prop/bar/propFileRecord.inc.c>\n'
            '    {0, "", 1.0}\n};\n')
        for name, skeleton in (('foo', 'door'), ('bar', 'standard_object')):
            (prop/name/'propFileRecord.inc.c').write_text('PROPFILERECORD(%s, 0.1)\n' % name)
            (prop/name/'ModelFileHeader.inc.c').write_text(
                'MODELFILEHEADER(%s, 0, &SKELETON(%s), 0, 3, 1, 12.5, 0, 1)\n' % (name, skeleton))
        (self.root/'kinds.h').write_text(KINDS)
        self.args = ['--rom', str(self.root/'rom.z64'), '--filelist', str(self.root/'filelist.csv'),
                     '--assets', str(assets), '--kinds', str(self.root/'kinds.h')]

    def write(self, setup):
        model, packed = compress(invented_gun()), compress(setup)
        (self.root/'rom.z64').write_bytes(bytes(64) + model + model + packed)
        at = 64 + 2 * len(model)
        (self.root/'filelist.csv').write_text(
            '64,%d,assets/obseg/prop/PfooZ.bin,1,1\n%d,%d,assets/obseg/prop/PbarZ.bin,1,1\n'
            '%d,%d,assets/obseg/setup/Ump_setupbarZ.bin,1,0\n'
            % (len(model), 64 + len(model), len(model), at, len(packed)))

    def tearDown(self):
        self.directory.cleanup()

    def run_main(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = scan.main(self.args + list(argv))
        return status, out.getvalue(), err.getvalue()


class Commands(Fixture):
    def test_types_counts_each_record_type(self):
        # Act
        status, out, _ = self.run_main('types')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('setups: 1', out)
        for line in ('DOOR: 1', 'PROP: 1', 'MONITOR: 1', 'UNK41: 1'):
            self.assertIn(line, out)

    def test_types_with_a_skeleton_counts_only_records_naming_such_a_model(self):
        # Act
        status, out, _ = self.run_main('types', '--skeleton', 'door')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('DOOR: 1', out)
        self.assertIn('MONITOR: 1', out)
        self.assertNotIn('PROP', out)

    def test_models_names_each_model_with_its_skeleton(self):
        # Act
        status, out, _ = self.run_main('models', '--type', 'PROP')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('bar  standard_object: 1', out)

    def test_refs_names_the_type_of_the_command_a_word_offsets_to(self):
        # Act
        status, out, _ = self.run_main('refs', '--type', 'MONITOR', '--word', '3')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('DOOR: 1', out)

    def test_refs_attached_counts_only_records_with_a_negative_pad_outside_an_object(self):
        # Arrange: the MONITOR's pad is -1 (attached) and a second MONITOR's flags say it is inside an object
        words = record(1, 0, 0, 0) + [10, (0 << 16) | 0xffff, 0, -1] + [10, 0xffff, 0x8000, -1] + [48]
        self.write(struct.pack('>4i', 0, 0, 0, 16) + struct.pack('>%di' % len(words), *words))
        # Act
        status, out, _ = self.run_main('refs', '--type', 'MONITOR', '--word', '3', '--attached')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('DOOR: 1', out)
        self.assertNotIn('MONITOR', out)

    def test_a_mask_keeps_only_records_with_one_of_its_bits_in_that_word(self):
        # Arrange: a second DOOR whose word 2 has bit 0x4
        words = record(1, 0, 0, 0) + record(1, 1, 4, 0) + [48]
        self.write(struct.pack('>4i', 0, 0, 0, 16) + struct.pack('>%di' % len(words), *words))
        # Act
        status, out, _ = self.run_main('models', '--type', 'DOOR', '--mask', '2:0x4')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('bar  standard_object: 1', out)
        self.assertNotIn('foo', out)

    def test_slots_reads_the_models_records_of_a_type_name(self):
        # Act
        status, out, _ = self.run_main('slots', '--type', 'DOOR', '--slots', '0-1')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('0  2 GROUP: 1', out)
        self.assertIn('1  empty: 1', out)

    def test_path_follows_child_and_next_from_the_root(self):
        # Act
        status, out, _ = self.run_main('path', '--type', 'PROP', '--path', 'child.next')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('24 DLCOLLISION: 1', out)

    def test_a_record_type_with_no_size_is_unreadable_and_fails(self):
        # Arrange
        self.write(invented_setup(end=99))
        # Act
        status, out, err = self.run_main('types')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('unreadable (Ump_setupbarZ.bin)', out)
        self.assertIn('Ump_setupbarZ.bin', err)

    def test_a_type_no_record_has_fails(self):
        # Act
        status, _, err = self.run_main('models', '--type', 'UNK41')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('no record', err)


if __name__ == '__main__':
    unittest.main()
