"""ROM-free checks for authoritative Clang declaration recovery."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from web_prototypes import declaration, json_records, mismatch_owners, recover, write_header


class WebPrototypeTests(unittest.TestCase):
    def test_mismatch_owners_keep_both_translation_units(self):
        # Arrange
        log = ('wasm-ld: warning: function signature mismatch: callback\n'
               '>>> defined in CMakeFiles/ge.dir/src/game/one.c.o\n'
               '>>> defined in CMakeFiles/ge.dir/src/game/two.c.o\n'
               '>>> defined in CMakeFiles/ge.dir/src/game/one.c.o\n')
        # Act
        owners = mismatch_owners(log)
        # Assert
        self.assertEqual(owners, {'callback': ['src/game/one.c', 'src/game/two.c']})

    def test_empty_parameter_list_becomes_explicit_void(self):
        # Arrange / Act
        result = declaration('get_value', 'float ()')
        # Assert
        self.assertEqual(result, 'float get_value(void);')

    def test_function_pointer_parameter_preserves_its_exact_signature(self):
        # Arrange / Act
        result = declaration('set_allocator', 'void (struct Vertex *(*)(int))')
        # Assert
        self.assertEqual(result, 'void set_allocator(struct Vertex *(*)(int));')

    def test_unsupported_type_is_rejected_instead_of_guessed(self):
        # Arrange / Act / Assert
        with self.assertRaisesRegex(ValueError, 'Unsupported function declaration'):
            declaration('unknown', 'int')

    def test_consecutive_ast_records_are_not_treated_as_one_json_object(self):
        # Arrange
        text = ' {"name":"one"}\n {"name":"two"} '
        # Act
        records = list(json_records(text))
        # Assert
        self.assertEqual(records, [{'name': 'one'}, {'name': 'two'}])

    def test_recovery_requires_a_definition_from_the_compile_database(self):
        # Arrange
        entries = {'/src/two.c': {'file': '/src/two.c'}}
        with patch('web_prototypes.definition_type', return_value='float (int)'):
            # Act
            result = recover('get_value', ['missing.c', 'two.c'], entries)
        # Assert
        self.assertEqual(result, ('get_value', 'float (int)', 'two.c'))

    def test_unresolved_definition_fails_closed(self):
        # Arrange / Act / Assert
        with self.assertRaisesRegex(ValueError, 'No authoritative definition'):
            recover('get_value', ['missing.c'], {})

    def test_generated_report_and_header_keep_source_and_struct_contracts(self):
        # Arrange
        definitions = [('vertices', 'struct Vertex * (int)', 'src/game/chr.c')]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'types.h'
            # Act
            write_header(definitions, path)
            # Assert
            self.assertIn('struct Vertex;', path.read_text())
            self.assertIn('struct Vertex * vertices(int);', path.read_text())
            self.assertEqual(json.loads(path.with_suffix('.json').read_text())[0][2],
                             'src/game/chr.c')


if __name__ == '__main__':
    unittest.main()
