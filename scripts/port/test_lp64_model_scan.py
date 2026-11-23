"""ROM-free tests for lp64_model_scan.py: models built here from invented numbers (LP64-005 Task 3)."""
import contextlib
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_model_scan as scan  # noqa: E402

SEG = 0x05000000


def ref(offset):
    return SEG | offset if offset is not None else 0


def node(opcode, data=None, nxt=None, child=None):
    return struct.pack('>HHIIIII', opcode, 0, ref(data), 0, ref(nxt), 0, ref(child))


def invented_gun():
    """Three switch slots, one texture entry, then a root header node at 24.

    Tree: root(HEADER) -child-> shadow(13) -next-> collision(24) -child-> group(2).
    Slots: 0 the group, 1 empty, 2 the collision node. The shadow names the root; the collision
    record has two vertices, the first linking the collision node, the second none.
    """
    root, shadow, collision, group = 24, 48, 72, 96
    shadow_rec, coll_rec, verts = 120, 152, 184
    data = struct.pack('>III', ref(group), 0, ref(collision)) + bytes(12)
    data += node(1, child=shadow)
    data += node(13, data=shadow_rec, nxt=collision)
    data += node(24, data=coll_rec, child=group)
    data += node(2)
    data += bytes(0x14) + struct.pack('>I', ref(root)) + bytes(8)        # shadow record, Header at 0x14
    data += bytes(0x0e) + struct.pack('>hI', 2, ref(verts)) + bytes(12)  # collision record
    data += bytes(8) + struct.pack('>I', ref(collision)) + bytes(4)      # vertex 0: linked
    data += bytes(16)                                                    # vertex 1: no link
    return data


def invented_setup():
    words = [0, 0, 16, 0] + [0, 1, 2] + [1, 1, -1, 0] + [5, 9, 9] + [9]  # SPAWN (3 words), ITEM (4), CUFF (2), END
    return struct.pack('>%di' % len(words), *words)


def compress(data):
    deflate = zlib.compressobj(9, zlib.DEFLATED, -15)
    return b'\x11\x72' + deflate.compress(data) + deflate.flush()


class Fixture(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        root = Path(self.directory.name)
        gun, setup = compress(invented_gun()), compress(invented_setup())
        rom = bytes(64) + gun + setup
        (root/'rom.z64').write_bytes(rom)
        (root/'filelist.csv').write_text('64,%d,assets/obseg/gun/GfooZ.bin,1,1\n%d,%d,assets/obseg/setup/Ump_setupbarZ.bin,1,0\n'
                                         % (len(gun), 64 + len(gun), len(setup)))
        (root/'assets/gun/foo').mkdir(parents=True)
        (root/'assets/gun/foo/ModelFileHeader.inc.c').write_text(
            'MODELFILEHEADER(foo, 0, &SKELETON(standard_gun), 0, 3, 1, 12.5, 0, 1)\n')
        self.args = ['--rom', str(root/'rom.z64'), '--filelist', str(root/'filelist.csv'), '--assets', str(root/'assets')]
        self.models = scan.load_models(rom, root/'filelist.csv', root/'assets')

    def tearDown(self):
        self.directory.cleanup()

    def run_main(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = scan.main(self.args + list(argv))
        return status, out.getvalue(), err.getvalue()


class Walk(Fixture):
    def test_loads_the_model_with_its_header_counts(self):
        # Act
        (model,) = self.models
        # Assert
        self.assertEqual((model.cls, model.name, model.num_switches, model.root), ('gun', 'foo', 3, 24))

    def test_root_opcode(self):
        self.assertEqual(scan.count_roots(self.models), {('gun', '1 HEADER'): 1})

    def test_slot_opcodes_and_an_empty_slot(self):
        # Act
        counts = scan.count_slots(self.models, [0, 1, 2])
        # Assert
        self.assertEqual(counts, {('gun', 0, '2 GROUP'): 1, ('gun', 1, 'empty'): 1, ('gun', 2, '24 DLCOLLISION'): 1})

    def test_a_slot_past_num_switches_is_out_of_range_not_empty(self):
        # Act
        counts = scan.count_slots(self.models, [3])
        # Assert
        self.assertEqual(counts, {('gun', 3, 'out of range'): 1})

    def test_walk_reaches_every_node_by_child_and_next(self):
        self.assertEqual(sorted(self.models[0].nodes()), [24, 48, 72, 96])

    def test_shadow_header_target(self):
        self.assertEqual(scan.count_targets(self.models, scan.SHADOW, scan.shadow_header), {('gun', '1 HEADER'): 1})

    def test_collision_links_skip_vertices_without_one(self):
        counts = scan.count_targets(self.models, scan.DLCOLLISION, scan.collision_links)
        self.assertEqual(counts, {('gun', '24 DLCOLLISION'): 1})

    def test_a_texture_entry_holding_a_number_is_not_inline(self):
        self.assertEqual(scan.count_textures(self.models), {('gun', 'no inline entry'): 1})

    def test_a_texture_entry_holding_a_segment_word_is_inline(self):
        # Arrange
        model = self.models[0]
        model.data = bytearray(model.data)
        struct.pack_into('>I', model.data, 12, SEG | 96)  # the one texture entry, after three slots
        # Act
        counts = scan.count_textures([model])
        # Assert
        self.assertEqual(counts, {('gun', 'foo', 'inline 1 of 1'): 1})

    def test_the_first_read_write_node_in_the_walk_is_the_root_header(self):
        self.assertEqual(scan.count_rw_first(self.models), {('gun', '1 HEADER'): 1})

    def test_the_first_read_write_node_skips_nodes_without_a_record(self):
        # Arrange: the root becomes a group, so the walk's first record is the collision node's
        model = self.models[0]
        model.data = bytearray(model.data)
        struct.pack_into('>H', model.data, 24, 2)
        # Act
        counts = scan.count_rw_first([model])
        # Assert
        self.assertEqual(counts, {('gun', '24 DLCOLLISION'): 1})

    def test_a_reference_outside_the_file_is_unreadable_not_a_count(self):
        # Arrange
        model = self.models[0]
        model.data = bytearray(model.data)
        struct.pack_into('>I', model.data, 8, SEG | 0xfff0)  # slot 2 points past the end
        # Act
        counts = scan.count_slots([model], [2])
        # Assert
        self.assertEqual(counts, {('gun', 2, 'unreadable (foo)'): 1})


class Command(Fixture):
    def rewrite(self, gun=None, extra_rows='', header=None):
        """Replace the fixture's gun file, add filelist rows, or replace its header line."""
        root = Path(self.directory.name)
        if gun is not None:
            packed = compress(bytes(gun))
            (root/'rom.z64').write_bytes(bytes(64) + packed)
            (root/'filelist.csv').write_text('64,%d,assets/obseg/gun/GfooZ.bin,1,1\n' % len(packed))
        if extra_rows:
            with open(root/'filelist.csv', 'a') as handle:
                handle.write(extra_rows)
        if header is not None:
            (root/'assets/gun/foo/ModelFileHeader.inc.c').write_text(header)

    def test_slots_command_prints_counts(self):
        # Act
        status, out, _ = self.run_main('slots', '--slots', '0-2')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('models: 1 (gun 1)', out)
        self.assertIn('gun  2  24 DLCOLLISION: 1', out)

    def test_intros_counts_record_types(self):
        # Act
        status, out, _ = self.run_main('intros')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('setups: 1', out)
        self.assertIn('SPAWN: 1', out)
        self.assertIn('CUFF: 1', out)

    def test_intros_items_counts_each_item_record_and_records_per_setup(self):
        # Act
        status, out, _ = self.run_main('intros', '--items')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('ITEM right=1 left=-1 demo=0: 1', out)
        self.assertIn('setups with 1 ITEM: 1', out)

    def test_textures_command_prints_counts(self):
        # Act
        status, out, _ = self.run_main('textures')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('gun  no inline entry: 1', out)

    def test_opcodes_counts_each_node_by_its_opcode(self):
        # Act
        counts = scan.count_opcodes(self.models)
        # Assert
        self.assertEqual(counts, {('gun', '1 HEADER'): 1, ('gun', '2 GROUP'): 1, ('gun', '13 SHADOW'): 1,
                                  ('gun', '24 DLCOLLISION'): 1})

    def test_opcodes_command_names_the_opcodes_no_model_holds(self):
        # Act
        status, out, _ = self.run_main('opcodes')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('gun  13 SHADOW: 1', out)
        self.assertIn('no model holds: 0 NULL, 3 OP03, 4 DL,', out)
        self.assertIn(' 17 OP17,', out)

    def test_a_filter_matching_no_model_fails(self):
        # Act
        status, out, err = self.run_main('roots', '--class', 'chr')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('no model found', err)

    def test_a_missing_rom_fails(self):
        # Act
        status, _, err = self.run_main('--rom', '/nonexistent/rom.z64', 'roots')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('no ROM', err)

    def test_an_unreadable_model_fails_the_command_naming_it(self):
        # Arrange
        gun = bytearray(invented_gun())
        struct.pack_into('>I', gun, 8, SEG | 0xfff0)  # slot 2 points past the end
        self.rewrite(gun=gun)
        # Act
        status, out, err = self.run_main('slots', '--slots', '2')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('unreadable (foo)', out)
        self.assertIn('foo', err)

    def test_a_header_line_with_spaces_after_the_parenthesis_is_read(self):
        # Arrange
        self.rewrite(header='MODELFILEHEADER(   foo,  0, &SKELETON(standard_gun),  0,  3,  1,  12.5,  0,  1)\n')
        # Act
        status, out, _ = self.run_main('roots')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('models: 1 (gun 1)', out)

    def test_a_model_without_a_header_fails_naming_it(self):
        # Arrange
        self.rewrite(extra_rows='0,64,assets/obseg/prop/PbarZ.bin,1,1\n')
        # Act
        status, _, err = self.run_main('roots')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('bar', err)

    def test_an_allowance_naming_the_skipped_model_passes(self):
        # Arrange
        self.rewrite(extra_rows='0,64,assets/obseg/prop/PbarZ.bin,1,1\n')
        # Act
        status, out, _ = self.run_main('--allow-missing', 'bar', 'roots')
        # Assert
        self.assertEqual(status, 0)
        self.assertIn('models: 1 (gun 1)', out)

    def test_an_allowance_naming_no_skipped_model_fails(self):
        # Act
        status, _, err = self.run_main('--allow-missing', 'bar', 'roots')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('bar', err)

    def test_two_files_defining_one_header_name_fail_naming_both(self):
        # Arrange
        other = Path(self.directory.name)/'assets/gun/copy'
        other.mkdir()
        (other/'ModelFileHeader.inc.c').write_text('MODELFILEHEADER(foo, 0, &SKELETON(standard_gun), 0, 5, 1, 12.5, 0, 1)\n')
        # Act
        status, _, err = self.run_main('roots')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('foo', err)
        self.assertIn('copy', err)

    def test_a_filelist_without_models_fails(self):
        # Arrange
        Path(self.args[3]).write_text('0,64,assets/other.bin,0,0\n')
        # Act
        status, _, err = self.run_main('links')
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('no model found', err)


if __name__ == '__main__':
    unittest.main()
