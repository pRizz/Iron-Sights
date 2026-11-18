#!/usr/bin/env python3
"""Tests for lp64_pointer_types.py: which record types it counts as reached only through pointers."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_pointer_types as tool  # noqa: E402

ROOT = '/checkout'


def unit(code: str, origin: str = 'src/bondtypes.h') -> tool.Unit:
    return tool.Unit(f'# 1 "{ROOT}/{origin}"\n{code}\n', ROOT)


def pointer_only(*units: tool.Unit) -> list[str]:
    types = tool.Types()
    for each in units:
        types.add_unit(each)
    types.resolve()
    skip, uses = set(), []
    for each in units:
        uses += tool.uses_in(each, types, skip)
    return [line.split('\t')[0] for line in tool.report(types, uses, tool.Physical(Path(ROOT)))]


HEADER = 'typedef struct Door { int a; int *b; } Door;\nstruct Tag { int x; };\n'


class Uses(unittest.TestCase):
    def test_a_type_used_only_through_pointers_is_printed(self):
        # Arrange
        code = HEADER + 'Door *d; struct Tag *t;'
        # Act
        found = pointer_only(unit(code))
        # Assert
        self.assertEqual(found, ['Door', 'Tag'])

    def test_a_declared_object_is_not_printed(self):
        # Arrange
        code = HEADER + 'Door d; struct Tag t[4];'
        # Act
        found = pointer_only(unit(code))
        # Assert
        self.assertEqual(found, [])

    def test_a_sizeof_is_not_printed(self):
        # Arrange
        code = HEADER + 'int n = sizeof(Door) + sizeof(struct Tag);'
        # Act
        found = pointer_only(unit(code))
        # Assert
        self.assertEqual(found, [])

    def test_an_embedded_member_is_not_printed(self):
        # Arrange
        code = HEADER + 'struct Outer { Door door; struct Tag *t; } outer;'
        # Act
        found = pointer_only(unit(code))
        # Assert
        self.assertEqual(found, ['Tag'])

    def test_an_alias_names_the_same_type(self):
        # Arrange
        code = HEADER + 'typedef struct Tag TagAlias; TagAlias alias; Door *d;'
        # Act
        found = pointer_only(unit(code))
        # Assert
        self.assertEqual(found, ['Door'])

    def test_a_cast_to_a_pointer_is_a_pointer_use(self):
        # Arrange
        code = HEADER + 'void f(void *p) { ((Door *) p)->a = 1; (struct Tag *) p; }'
        # Act
        found = pointer_only(unit(code))
        # Assert
        self.assertEqual(found, ['Door', 'Tag'])

    def test_a_type_outside_the_record_files_is_not_listed(self):
        # Arrange
        code = 'struct Local { int a; };\nstruct Local *p;'
        # Act
        found = pointer_only(unit(code, 'tools/other.c'))
        # Assert
        self.assertEqual(found, [])

    def test_an_object_in_another_unit_counts(self):
        # Arrange
        first, second = unit(HEADER + 'Door *d;'), unit(HEADER + 'Door here;', 'src/game/door.c')
        # Act
        found = pointer_only(first, second)
        # Assert
        self.assertEqual(found, ['Tag'])


    def test_a_type_embedded_only_in_a_pointer_only_type_is_printed(self):
        # Arrange
        code = HEADER + 'struct Box { Door door; int n; };\nstruct Box *b;'
        # Act
        found = pointer_only(unit(code))
        # Assert
        self.assertEqual(found, ['Door', 'Tag', 'Box'])

    def test_a_type_embedded_in_an_object_through_two_levels_is_not_printed(self):
        # Arrange
        code = HEADER + 'struct Box { Door door; };\nstruct Crate { struct Box box; } crate;\nstruct Tag *t;'
        # Act
        found = pointer_only(unit(code))
        # Assert
        self.assertEqual(found, ['Tag'])

    def test_a_type_name_in_a_string_literal_is_not_an_object(self):
        # Arrange
        code = HEADER + 'const char *s = "Door Tag";\nDoor *d;'
        # Act
        found = pointer_only(unit(code))
        # Assert
        self.assertEqual(found, ['Door', 'Tag'])

    def test_a_declarator_named_like_its_type_is_not_an_object(self):
        # Arrange
        code = HEADER + 'void f(Door *Door) { Door->a = 1; }\nstruct Tag *t;'
        # Act
        found = pointer_only(unit(code))
        # Assert
        self.assertEqual(found, ['Door', 'Tag'])


class Pinned(unittest.TestCase):
    def test_an_added_and_a_removed_type_are_named(self):
        # Act
        problems = tool.compare(['Door', 'Tag'], ['Door', 'Box'])
        # Assert
        self.assertEqual(len(problems), 2)
        self.assertIn('Box', problems[0])
        self.assertIn('FORK-089', problems[0])
        self.assertIn('Tag', problems[1])

    def test_the_same_list_has_no_problem(self):
        self.assertEqual(tool.compare(['Door'], ['Door']), [])

    def test_the_pinned_list_is_read_without_comments(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root)/'list.txt'
            path.write_text('# header\nDoor\nTag\n')
            self.assertEqual(tool.read_pinned(path), ['Door', 'Tag'])

class Lines(unittest.TestCase):
    def test_a_line_directive_maps_to_the_physical_line(self):
        # Arrange
        with tempfile.TemporaryDirectory() as root:
            (Path(root)/'a.c').write_text('int a;\n#line 100\nint b;\nint c;\n')
            place = tool.Physical(Path(root))
            # Act
            found = place(('a.c', 101))
        # Assert
        self.assertEqual(found, 'a.c:4')


if __name__ == '__main__':
    unittest.main()
