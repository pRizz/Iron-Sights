"""ROM-free tests for the overlay lister: every record read through another type (LP64-005 Task 3)."""
import contextlib
import io
from pathlib import Path
import re
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_overlays as overlays  # noqa: E402

HEADER = """
typedef struct ChrRecord { struct { u8 padding[160]; } act_bytes; } ChrRecord;
typedef union Param { s32 i; void *p; } Param;
struct fontchar { s32 index; s32 height; };
typedef struct Model { s32 unk00; } Model;
struct holder {
    Gfx dl[2];
    u32 field_10;
    u32 field_14;
    u32 named;
};
"""

# One site of each of the four kinds the C files hold.
SAMPLE = """
u32 sample(PropRecord *prop, void *header)
{
    ChrRecord *v = (ChrRecord *) prop->voidp;
    s32 n = ((u16 *) header)[2];
    s32 m = *(s32 *) ((u8 *) header + 8);
    return v->act_bytes.padding[84] + n + m;
}
"""


def keys(text, path='src/game/a.c', header=HEADER):
    """Return the site keys `sites_in` finds in `text`, with the record types of `header`."""
    types = overlays.record_types({'src/bondtypes.h': header})
    return [site.key for site in overlays.sites_in(path, text, types)]


def wrap(body):
    """Return `body` as the statements of a function `f`."""
    return 'void f(void *x)\n{\n' + body + '\n}\n'


INDEX = 'n = ((u16 *) x)[2];'
INDEX_KEY = 'src/game/a.c:f:index:((u16*)x)[2]'


class Kinds(unittest.TestCase):
    def test_each_kind_of_a_c_file_is_found_once_in_the_sample(self):
        # Act
        found = keys(SAMPLE)
        # Assert
        self.assertEqual(sorted(found), sorted([
            'src/game/a.c:sample:record:(ChrRecord*)prop->voidp',
            'src/game/a.c:sample:index:((u16*)header)[2]',
            'src/game/a.c:sample:offset:*(s32*)((u8*)header+8)',
            'src/game/a.c:sample:opaque:v->act_bytes.padding[84]',
        ]))

    def test_a_run_is_a_record_member_followed_by_filler_members(self):
        # Arrange
        types = overlays.record_types({'src/bondtypes.h': HEADER})
        # Act
        found = [site.key for site in overlays.runs_in('src/bondtypes.h', HEADER, types)]
        # Assert
        self.assertEqual(found, ['src/bondtypes.h:holder:run:Gfx dl[2]; +2 fillers'])

    def test_a_record_member_ends_a_run_and_starts_its_own(self):
        # Arrange
        header = HEADER + 'struct pair { Model unk00; s32 unk04; Model unk08; s32 unk0C; };\n'
        types = overlays.record_types({'src/bondtypes.h': header})
        # Act
        found = [site.key for site in overlays.runs_in('src/bondtypes.h', header, types)][1:]
        # Assert
        self.assertEqual(found, ['src/bondtypes.h:pair:run:Model unk00; +1 fillers',
                                 'src/bondtypes.h:pair:run:Model unk08; +1 fillers'])

    def test_a_dereference_of_a_cast_plus_a_constant_is_an_offset(self):
        # The shape of src/sched.c's read of a client's third word.
        self.assertEqual(keys(wrap('if (*((s32*)client + 2) == 0) {}')), ['src/game/a.c:f:offset:*((s32*)client+2)'])

    def test_a_parenthesised_cast_of_a_sum_is_an_offset(self):
        self.assertEqual(keys(wrap('n = *((s32 *) ((u8 *) x + 8));')), ['src/game/a.c:f:offset:*((s32*)((u8*)x+8))'])

    def test_a_cast_of_a_computed_offset_is_an_offset(self):
        # The shape of propobj.c's vertex copy, `*((Word4 *) (((u8 *) p) + offset))`.
        self.assertEqual(keys(wrap('w = *((Word4 *)(((u8 *) t) + o));')),
                         ['src/game/a.c:f:offset:*((Word4*)(((u8*)t)+o))'])

    def test_an_integer_cast_between_the_pointer_cast_and_the_sum_is_an_offset(self):
        self.assertEqual(keys(wrap('n = *(u16 *)(uintptr_t)(timg - 8);')),
                         ['src/game/a.c:f:offset:*(u16*)(uintptr_t)(timg-8)'])

    def test_the_parenthesised_texture_number_read_is_an_offset(self):
        # The shape of bg.c's and propobj.c's `texnum = *((u16 *) (uintptr_t) (timg - 8))`.
        self.assertEqual(keys(wrap('n = *((u16 *) (uintptr_t) (timg - 8));')),
                         ['src/game/a.c:f:offset:*((u16*)(uintptr_t)(timg-8))'])

    def test_a_dereference_of_a_parenthesised_cast_plus_a_constant_is_an_offset(self):
        self.assertEqual(keys(wrap('n = *(((u16 *) h) + 2);')), ['src/game/a.c:f:offset:*(((u16*)h)+2)'])

    def test_an_index_through_doubled_parentheses_is_an_index(self):
        self.assertEqual(keys(wrap('n = (((u16 *) h))[2];')), ['src/game/a.c:f:index:(((u16*)h))[2]'])

    def test_a_dereference_after_an_if_condition_or_else_is_an_offset(self):
        # Arrange
        text = wrap('if (c) *(s32 *)((u8 *)x + 4) = 0;\nelse *(s32 *)((u8 *)x + 8) = 0;')
        # Act / Assert
        self.assertEqual(keys(text), ['src/game/a.c:f:offset:*(s32*)((u8*)x+4)',
                                      'src/game/a.c:f:offset:*(s32*)((u8*)x+8)'])

    def test_a_multiplication_by_a_cast_sum_is_not_an_offset(self):
        self.assertEqual(keys(wrap('n = (a) * (int)(x + 1); m = b * (int)(x + 1);')), [])

    def test_a_dereference_of_a_cast_address_is_an_offset(self):
        # The shapes of rle.c's `*(u16 *)&src[2]` and prop.c's `*(s32 *) &door->maxFrac`.
        self.assertEqual(keys(wrap('a = *(u16 *)&src[2]; b = *(s32 *) &door->maxFrac;')),
                         ['src/game/a.c:f:offset:*(u16*)&src[2]', 'src/game/a.c:f:offset:*(s32*)&door->maxFrac'])

    def test_a_dereference_of_a_plain_cast_is_not_an_offset(self):
        self.assertEqual(keys(wrap('a = *(u32 *) p;')), [])

    def test_a_union_and_a_struct_tag_are_records(self):
        # Act
        found = keys(wrap('a = (Param *) x; b = (struct fontchar *) x;'))
        # Assert
        self.assertEqual(found, ['src/game/a.c:f:record:(Param*)x', 'src/game/a.c:f:record:(struct fontchar*)x'])

    def test_a_cast_to_a_type_not_declared_in_the_record_headers_is_not_a_record(self):
        self.assertEqual(keys(wrap('a = (Other *) x; b = (u8 *) x;')), [])

    def test_a_prototype_a_sizeof_and_a_null_pointer_are_not_casts(self):
        # Arrange
        text = 'void g(ChrRecord *);\nvoid (*cb)(ChrRecord *);\n' + wrap('n = sizeof(ChrRecord *); p = (ChrRecord *) NULL;')
        # Act / Assert
        self.assertEqual(keys(text), [])

    def test_a_return_of_a_cast_is_a_cast(self):
        self.assertEqual(keys(wrap('return (ChrRecord *) x;')), ['src/game/a.c:f:record:(ChrRecord*)x'])

    def test_a_scalar_cast_without_an_index_or_dereference_is_not_listed(self):
        self.assertEqual(keys(wrap('p = (u16 *) x + 2; q = (u8 *) x;')), [])

    def test_a_variable_index_into_a_padding_member_is_not_opaque(self):
        self.assertEqual(keys(wrap('a = x->padding[i]; b = x->unk10[3];')), ['src/game/a.c:f:opaque:x->unk10[3]'])

    def test_a_member_whose_name_only_starts_with_pad_is_not_opaque(self):
        self.assertEqual(keys(wrap('a = x->pads[0]; b = x->pad_1C[2]; c = x->pad2[1];')),
                         ['src/game/a.c:f:opaque:x->pad_1C[2]', 'src/game/a.c:f:opaque:x->pad2[1]'])

    def test_text_in_comments_and_strings_is_not_a_site(self):
        self.assertEqual(keys(wrap('/* ((u16 *) x)[2] */ // ((u16 *) x)[2]\nprintf("((u16 *) x)[2]");')), [])

    def test_a_record_cast_through_a_pointer_to_pointer_is_a_record(self):
        self.assertEqual(keys(wrap('a = (ChrRecord **) x;')), ['src/game/a.c:f:record:(ChrRecord**)x'])

    def test_a_pointer_array_read_through_a_void_cast_is_an_index(self):
        self.assertEqual(keys(wrap('a = ((void **) x)[3];')), ['src/game/a.c:f:index:((void**)x)[3]'])


class Regions(unittest.TestCase):
    def listed(self, condition, then='', otherwise=None):
        """Return whether a site under `condition` and one under its `#else` are listed."""
        text = f'#{condition}\n{INDEX}\n'
        if otherwise is not None:
            text += f'#{otherwise}\nn = ((u16 *) x)[3];\n'
        found = keys(wrap(text + '#endif'))
        return INDEX_KEY in found, any(key.endswith('[3]') for key in found)

    def test_a_site_inside_ifdef_target_n64_is_not_listed_and_its_else_is(self):
        self.assertEqual(self.listed('ifdef TARGET_N64', otherwise='else'), (False, True))

    def test_a_site_inside_ifndef_target_n64_is_listed_and_its_else_is_not(self):
        self.assertEqual(self.listed('ifndef TARGET_N64', otherwise='else'), (True, False))

    def test_the_else_of_if_is_64_bit_is_the_32_bit_branch_and_is_not_listed(self):
        self.assertEqual(self.listed('if IS_64_BIT', otherwise='else'), (True, False))

    def test_a_site_inside_if_not_is_64_bit_is_not_listed_and_its_else_is(self):
        self.assertEqual(self.listed('if !IS_64_BIT', otherwise='else'), (False, True))

    def test_a_site_inside_if_0_is_not_listed_and_its_else_is(self):
        self.assertEqual(self.listed('if 0', otherwise='else'), (False, True))

    def test_an_elif_after_a_taken_branch_is_not_listed(self):
        self.assertEqual(self.listed('ifndef TARGET_N64', otherwise='elif defined(GE_VIDEO)'), (True, False))

    def test_an_elif_after_a_branch_left_out_is_judged_on_its_own(self):
        self.assertEqual(self.listed('ifdef TARGET_N64', otherwise='elif IS_64_BIT'), (False, True))

    def test_an_elif_after_an_unknown_branch_is_kept_and_the_else_after_it_is_not(self):
        # Arrange
        text = wrap(f'#ifdef GE_VIDEO\n{INDEX}\n#elif IS_64_BIT\nn = ((u16 *) x)[3];\n#else\nn = ((u16 *) x)[4];\n#endif')
        # Act / Assert
        self.assertEqual(keys(text), [INDEX_KEY, 'src/game/a.c:f:index:((u16*)x)[3]'])

    def test_a_condition_with_arithmetic_it_does_not_evaluate_is_kept(self):
        self.assertEqual(self.listed('if 0 + GE_VIDEO', otherwise='else'), (True, True))

    def test_an_extern_c_block_leaves_its_functions_named(self):
        # Arrange
        text = '#ifdef __cplusplus\nextern "C" {\n#endif\n' + wrap(INDEX) + '#ifdef __cplusplus\n}\n#endif\n'
        # Act / Assert
        self.assertEqual(keys(text), [INDEX_KEY])

    def test_a_symbol_a_pc_build_may_define_keeps_both_branches(self):
        self.assertEqual(self.listed('ifdef GE_VIDEO', otherwise='else'), (True, True))

    def test_a_version_no_pc_build_defines_is_left_out(self):
        self.assertEqual(self.listed('if defined(VERSION_JP) || defined(VERSION_EU)', otherwise='else'), (False, True))

    def test_debug_is_left_out(self):
        self.assertEqual(self.listed('ifdef DEBUG', otherwise='else'), (False, True))

    def test_a_symbol_every_pc_build_defines_is_taken(self):
        self.assertEqual(self.listed('ifndef VERSION_US', otherwise='else'), (False, True))

    def test_a_platform_the_64_bit_build_refuses_is_left_out(self):
        self.assertEqual(self.listed('ifdef _WIN32', otherwise='else'), (False, True))

    def test_the_64_bit_build_refuses_the_platforms_left_out(self):
        # Arrange
        root = Path(__file__).resolve().parents[2]
        build = (root/'CMakeLists.txt').read_text()
        # Act
        refusals = re.findall(r'if\((WIN32|EMSCRIPTEN)\)\s*message\(FATAL_ERROR', build)
        # Assert
        self.assertEqual(overlays.UNDEFINED_AT_64, ('_WIN32', '__EMSCRIPTEN__', 'GE_WEB'))
        self.assertEqual(sorted(refusals), ['EMSCRIPTEN', 'WIN32'])
        self.assertNotIn('GE_WEB', build)

    def test_a_conjunction_with_target_n64_is_left_out(self):
        self.assertEqual(self.listed('if defined(GE_VIDEO) && defined(TARGET_N64)'), (False, False))

    def test_a_disjunction_with_is_64_bit_is_taken(self):
        self.assertEqual(self.listed('if IS_64_BIT || defined(GE_LP64_ORACLE)', otherwise='else'), (True, False))

    def test_a_kept_region_nested_in_one_left_out_is_left_out(self):
        # Arrange
        text = wrap(f'#ifdef TARGET_N64\n#ifndef TARGET_N64\n{INDEX}\n#endif\n#else\nn = ((u16 *) x)[3];\n#endif')
        # Act / Assert
        self.assertEqual(keys(text), ['src/game/a.c:f:index:((u16*)x)[3]'])

    def test_a_region_left_out_nested_in_an_unknown_one_ends_at_its_own_endif(self):
        # Arrange
        text = wrap(f'#ifdef GE_VIDEO\n#if 0\n{INDEX}\n#endif\nn = ((u16 *) x)[3];\n#endif')
        # Act / Assert
        self.assertEqual(keys(text), ['src/game/a.c:f:index:((u16*)x)[3]'])

    def test_the_symbol_lists_agree_with_the_build(self):
        # Arrange
        root = Path(__file__).resolve().parents[2]
        build = (root/'CMakeLists.txt').read_text() + ''.join(p.read_text() for p in (root/'cmake').glob('*.cmake'))
        definitions = re.search(r'add_compile_definitions\(\s*([^)]*)\)', build).group(1).split()
        # Act
        named = [symbol for symbol in overlays.UNDEFINED_ON_PC if re.search(rf'\b{symbol}\b', build)]
        # Assert
        self.assertEqual(named, [])
        self.assertEqual(sorted(overlays.DEFINED_ON_PC), sorted(definitions))


class Identity(unittest.TestCase):
    def test_a_site_outside_any_function_is_owned_by_the_object_it_initializes(self):
        self.assertEqual(keys('static s32 table[] = { ((s32 *) &x)[1], 2 };\nvoid *p = (ChrRecord *) &y;'),
                         ['src/game/a.c:table:index:((s32*)&x)[1]', 'src/game/a.c:p:record:(ChrRecord*)&y'])

    def test_a_definition_spread_over_lines_is_named_by_its_function(self):
        # Arrange
        text = 'void proto(void);\nstatic u32\nspread(\n    s32 a,\n    void *x)\n{\n' + INDEX + '\n}\n'
        # Act / Assert
        self.assertEqual(keys(text), ['src/game/a.c:spread:index:((u16*)x)[2]'])

    def test_a_k_and_r_definition_is_named_by_its_function(self):
        self.assertEqual(keys('int kr(a, x)\nint a;\nvoid *x;\n{\n' + INDEX + '\n}\n'),
                         ['src/game/a.c:kr:index:((u16*)x)[2]'])

    def test_a_cast_in_a_macro_definition_is_owned_by_the_macro(self):
        # Arrange
        text = '#define CHR(p) \\\n    ((ChrRecord *) (p))\nvoid f(void) { }\n'
        # Act / Assert
        self.assertEqual(keys(text, 'src/game/a.h'), ['src/game/a.h:#define CHR:record:(ChrRecord*)(p)'])

    def test_whitespace_does_not_change_a_key(self):
        self.assertEqual(keys(wrap('n = (  ( u16* )x ) [ 2 ];')), [INDEX_KEY])

    def test_a_cast_split_across_lines_is_found_with_the_same_key(self):
        self.assertEqual(keys(wrap('n = ((u16\n      *)\n   x)\n  [2];')), [INDEX_KEY])

    def test_moving_a_site_to_another_line_changes_no_key(self):
        self.assertEqual(keys('\n\n\n' + wrap('\n\n' + INDEX)), keys(wrap(INDEX)))

    def test_the_same_text_twice_in_one_function_is_two_sites(self):
        self.assertEqual(keys(wrap(INDEX + '\n' + INDEX)), [INDEX_KEY, INDEX_KEY + ' #2'])

    def test_a_change_to_the_text_is_a_new_site(self):
        self.assertNotEqual(keys(wrap(INDEX)), keys(wrap('n = ((u16 *) x)[3];')))

    def test_an_unbalanced_file_is_an_error_not_a_silent_misreading(self):
        with self.assertRaises(overlays.SourceError):
            keys('void f(void)\n{\n' + INDEX + '\n')


class Scope(unittest.TestCase):
    def test_the_tree_scope_takes_the_c_sources_and_leaves_the_loaders_out(self):
        # Arrange
        paths = ['src/game/a.c', 'src/libultra/io/b.c', 'port/src/c.c', 'port/include/d.h', 'src/game/e.h',
                 'port/src/romconv_x.c', 'port/src/romfile_y.c', 'port/src/romdata.c', 'port/include/romdata.h',
                 'port/fast3d/g.cpp', 'port/fast3d/third_party/h.c', 'port/web/i.c', 'src/j.s', 'include/PR/k.h']
        # Act
        taken = [path for path in paths if overlays.in_scope(path)]
        # Assert
        self.assertEqual(taken, ['src/game/a.c', 'src/libultra/io/b.c', 'port/src/c.c', 'port/include/d.h',
                                 'src/game/e.h'])

    def test_an_included_inl_file_is_read(self):
        self.assertTrue(overlays.in_scope('port/src/port_hud_audit.inl'))


class Check(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        (self.root/'src/game').mkdir(parents=True)
        (self.root/'src/bondtypes.h').write_text(HEADER)
        (self.root/'src/game/a.c').write_text(wrap(INDEX))
        self.files = ['src/bondtypes.h', 'src/game/a.c']

    def tearDown(self):
        self.directory.cleanup()

    def run_check(self, reviewed):
        """Return (status, output) of `check` against `reviewed`."""
        sites = overlays.list_sites(self.root, self.files)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = overlays.report(sites, overlays.parse_reviewed(reviewed))
        return status, out.getvalue()

    def reviewed_text(self, *keys_):
        return '# header\n' + ''.join(f'{key}\ttyped\tthe object is of that type\n' for key in keys_)

    def test_every_site_reviewed_passes(self):
        # Arrange
        reviewed = self.reviewed_text('src/bondtypes.h:holder:run:Gfx dl[2]; +2 fillers', INDEX_KEY)
        # Act
        status, out = self.run_check(reviewed)
        # Assert
        self.assertEqual((status, out.strip()), (0, 'OK: 2 sites, all reviewed'))

    def test_a_site_missing_from_the_list_fails(self):
        # Act
        status, out = self.run_check(self.reviewed_text('src/bondtypes.h:holder:run:Gfx dl[2]; +2 fillers'))
        # Assert
        self.assertEqual(status, 1)
        self.assertIn(f'unreviewed: {INDEX_KEY}', out)

    def test_a_stale_line_fails(self):
        # Arrange
        reviewed = self.reviewed_text('src/bondtypes.h:holder:run:Gfx dl[2]; +2 fillers', INDEX_KEY,
                                      'src/game/a.c:gone:index:((u8*)x)[1]')
        # Act
        status, out = self.run_check(reviewed)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('stale: src/game/a.c:gone:index:((u8*)x)[1]', out)

    def test_a_changed_text_is_unreviewed_and_its_old_line_stale(self):
        # Arrange
        reviewed = self.reviewed_text('src/bondtypes.h:holder:run:Gfx dl[2]; +2 fillers', INDEX_KEY)
        (self.root/'src/game/a.c').write_text(wrap('n = ((u16 *) x)[3];'))
        # Act
        status, out = self.run_check(reviewed)
        # Assert
        self.assertEqual(status, 1)
        self.assertIn('unreviewed: src/game/a.c:f:index:((u16*)x)[3]', out)
        self.assertIn(f'stale: {INDEX_KEY}', out)

    def test_a_repeated_key_in_the_listing_is_refused(self):
        # Arrange
        site = overlays.list_sites(self.root, self.files)[0]
        # Act
        with contextlib.redirect_stdout(io.StringIO()):
            status = overlays.report([site, site], {site.key: ('typed', 'r')})
        # Assert
        self.assertEqual(status, 2)

    def test_no_sites_is_not_ok(self):
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(overlays.report([], {}), 2)

    def test_a_run_declared_in_a_c_file_is_listed(self):
        # Arrange
        (self.root/'src/game/a.c').write_text('struct local { Model m; s32 unk04; };\n' + wrap(INDEX))
        # Act
        found = [site.key for site in overlays.list_sites(self.root, self.files)]
        # Assert
        self.assertIn('src/game/a.c:local:run:Model m; +1 fillers', found)

    def test_a_line_with_an_unknown_verdict_is_refused(self):
        with self.assertRaises(ValueError):
            overlays.parse_reviewed(f'{INDEX_KEY}\tfixed\tgone\n')

    def test_a_line_without_a_reason_is_refused(self):
        with self.assertRaises(ValueError):
            overlays.parse_reviewed(f'{INDEX_KEY}\ttyped\t\n')

    def test_the_checked_in_list_parses(self):
        # Act / Assert
        overlays.parse_reviewed(overlays.REVIEWED.read_text())


class Locals(unittest.TestCase):
    def test_a_scalar_pointer_cast_stored_in_a_local_or_member_is_reported(self):
        # Arrange
        text = wrap('u16 *p = (u16 *) anim;\nq->bytes = (u8 *) rec;\nr = (u16 *) NULL;\nn = p[2];')
        # Act
        found = [aid.key for aid in overlays.locals_in('src/game/a.c', text)]
        # Assert
        self.assertEqual(found, ['src/game/a.c:f:p=(u16*)anim', 'src/game/a.c:f:q->bytes=(u8*)rec'])


if __name__ == '__main__':
    unittest.main()
