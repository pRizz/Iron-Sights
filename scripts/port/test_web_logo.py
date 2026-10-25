"""ROM-free checks for generated title storage and guarded payload conversion."""
from pathlib import Path
import unittest

from web_logo import ROOT, audit_wasm_payloads, generate


class LogoGeneration(unittest.TestCase):
    def setUp(self):
        self.source = (ROOT / 'assets/rarewarelogo.c').read_text(encoding='utf-8')

    def test_payloads_become_uninitialized_backing_arrays(self):
        output, blocks = generate(self.source)
        self.assertEqual(len(blocks), 38)
        self.assertEqual(sum(block.size for block in blocks), 23496)
        self.assertIn('u32 imgRAre_0x0020[686];', output)
        self.assertIn('Vtx verts2B18[16];', output)
        self.assertNotIn('0xED0FCC4F', output)
        self.assertIn('gsSPVertex(verts2B18', output)
        self.assertEqual(output, generate(self.source)[0])

    def test_new_payload_kind_requires_a_generator_audit(self):
        with self.assertRaisesRegex(ValueError, 'Unrecognized initialized'):
            generate(self.source + '\nu16 surprise[] = {0x1234};\n')

    def test_missing_block_fails_closed(self):
        with self.assertRaises(ValueError):
            generate(self.source.replace('u8 rgba0000[4]', 'u8 renamed[4]'))

    def test_wrong_dimension_fails(self):
        with self.assertRaisesRegex(ValueError, 'dimension'):
            generate(self.source.replace('u8 rgba0000[4]', 'u8 rgba0000[5]'))

    def test_binary_audit_detects_an_included_payload(self):
        source = 'u32 image[] = {' + ', '.join(hex(index) for index in range(32)) + '};'
        leaked = b''.join(index.to_bytes(4, 'little') for index in range(32))
        with self.assertRaisesRegex(ValueError, 'payload remains'):
            audit_wasm_payloads(source, leaked)
        self.assertEqual(audit_wasm_payloads(source, b'no embedded logo'), 1)


if __name__ == '__main__':
    unittest.main()
