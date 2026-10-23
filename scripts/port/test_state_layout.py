"""ROM-free coverage of the Windows archive and Linux linker-map formats."""
from pathlib import Path
import struct
import tempfile
import unittest
from embed_state_layout import regions, embed, FOOTER


class LayoutTests(unittest.TestCase):
    def test_ownership_and_local_host_caches(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            (root/'src/game').mkdir(parents=True)
            (root/'assets').mkdir()
            lines=[]
            for i in range(55):
                (root/f'src/game/world{i}.c').touch()
                owner=f'CMakeFiles/ge.dir/objects.a(world{i}.c.obj)' if i%2 else f'CMakeFiles/ge.dir/src/game/world{i}.c.o'
                lines.append(f' .bss 0x{0x100000+i*0x10000:x} 0x1000 {owner}')
            for name in ('options.c','file2.c','boss.c'):
                (root/'src'/name).touch()
                lines.append(f' .data 0x900000 0x1000 objects.a({name}.obj)')
            lines += [' .data 0xa00000 0x1000 objects.a(SDL.c.obj)',
                      ' COMMON 0xb00000 0x1000 objects.a(world0.c.obj)',
                      ' .data 0xc00000 0x1000 asset_obj/setup.o',
                      ' .bss 0xd00000 0x1000 objects.a(romdata.c.obj)',
                      ' .bss 0xe00000 0x1000 objects.a(romfile_bg.c.obj)']
            syms='00100000 b _game_value\n00100004 b _saved.0\n00100010 b _world_value\n'
            result=regions('\n'.join(lines),root,syms)
            self.assertIn((0x100000,4),result)
            self.assertIn((0x100010,0xff0),result)
            self.assertIn((0xb00000,0x1000),result)
            self.assertIn((0xc00000,0x1000),result)
            self.assertIn((0xd00000,0x1000),result)
            self.assertIn((0xe00000,0x1000),result)
            self.assertFalse(any(addr in (0x900000,0xa00000) for addr,_ in result))
            with self.assertRaises(ValueError): regions('',root,'')

    def test_overlay_idempotent_and_bounded(self):
        original=b'pretend executable\x00'*100
        chunks=[(0x100000,0x2000),(0x200000,0x1000)]
        first=embed(original,chunks)
        self.assertEqual(embed(first,chunks),first)
        self.assertEqual(first[:len(original)],original)
        self.assertNotEqual(embed(original+b'changed',chunks),first)
        with self.assertRaises(ValueError): embed(original+FOOTER+struct.pack('<I',0xffffffff),chunks)


if __name__=='__main__': unittest.main()
