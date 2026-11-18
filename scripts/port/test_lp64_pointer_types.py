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
