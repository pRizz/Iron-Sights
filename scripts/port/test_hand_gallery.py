"""ROM-free tests for gallery pixels, completeness, provenance and commit safety."""
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest import mock
import zlib

spec = importlib.util.spec_from_file_location('gallery', Path(__file__).with_name('hand_gallery.py'))
gallery = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gallery)


def command(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], stderr=subprocess.PIPE)


def fixture(root):
    command(root, 'init', '--initial-branch=main')
    (root/'src').mkdir()
    (root/'src/example.c').write_bytes(b'int image = 1;\n')
    command(root, 'add', 'src/example.c')


def references(root):
    directory = root/gallery.OUTPUT
    rows = []
    raw = b'\x30\x34\x3c' * (960*540)
    for case in gallery.cases():
        rows.append(dict(case, **gallery.asset(case['file'], 960, 540, 3, raw, directory)))
    icons = []
    for slot, name in enumerate(gallery.ICON_NAMES):
        if slot and name:
            icons.append(dict(id=slot, name=name, **gallery.asset(
                f'images/icons/{slot:02d}-{name}.png', 512, 160, 4, b'\x11\x22\x33\xff'*(512*160), directory)))
    manifest = dict(schema=1, recipe=gallery.RECIPE, inputs_sha256=gallery.source_fingerprint(root),
                    rom_sha256='fixture', renderer={'renderer': 'Synthetic test'},
                    repeat_verified=True, independent_pass_verified=True, cases=rows, icons=icons)
    (directory/'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True)+'\n')
    (directory/'index.html').write_text(gallery.page(manifest), encoding='utf-8', newline='\n')
    return manifest


class Gallery(unittest.TestCase):
    def test_matrix_coordinates(self):
        rows = gallery.cases()
        self.assertEqual(len(rows), 72)
        self.assertEqual(len({row['file'] for row in rows}), 72)
        for i, row in enumerate(rows):
            self.assertEqual(row['id'], i)
            self.assertEqual(row['wardrobe'], gallery.WARDROBES[i//12])
            self.assertEqual(row['weapon_id'], (4, 8, 7)[i//4 % 3])
            self.assertEqual(row['mode'], ('regular', 'ads')[i//2 % 2])
            self.assertEqual(row['variant'], ('stock', 'fork')[i % 2])
        self.assertEqual(sum(name is not None for name in gallery.ICON_NAMES[1:]), 35)

    def test_canonical_png(self):
        for channels in (3, 4):
            raw = bytes(i % 256 for i in range(15*11*channels))
            encoded = gallery.png_encode(15, 11, raw, channels)
            self.assertEqual(encoded, gallery.png_encode(15, 11, raw, channels))
            self.assertEqual(gallery.png_decode(encoded), (15, 11, channels, raw))
            for bad in (encoded[:-1], encoded[:45]+b'x'+encoded[46:]):
                with self.assertRaises(ValueError):
                    gallery.png_decode(bad)
        with self.assertRaises(ValueError):
            gallery.png_encode(1, 1, b'abc', 4)

    def test_bmp_color_stride_and_orientation(self):
        width, height = 960, 540
        top, bottom = b'\x12\x34\x56'*width, b'\xab\xcd\xef'*width
        bgr = bytearray(top*(height-1)+bottom)
        bgr[0::3], bgr[2::3] = bgr[2::3], bgr[0::3]
        rows = [bytes(bgr[y*width*3:(y+1)*width*3]) for y in range(height)]
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'capture.bmp'
            for signed_height in (height, -height):
                pixels = b''.join(reversed(rows) if signed_height > 0 else rows)
                header = b'BM'+struct.pack('<IHHI', 54+len(pixels), 0, 0, 54)
                header += struct.pack('<IiiHHIIiiII', 40, width, signed_height, 1, 24, 0, len(pixels), 0, 0, 0, 0)
                path.write_bytes(header+pixels)
                self.assertEqual(gallery.bmp_pixels(path), top*(height-1)+bottom)

    def test_fingerprint_and_partial_staging(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            fixture(root)
            initial = gallery.source_fingerprint(root)
            self.assertEqual(initial, gallery.source_fingerprint(root, True))
            (root/'src/example.c').write_bytes(b'int image = 1;\r\n')
            self.assertEqual(initial, gallery.source_fingerprint(root))
            (root/'notes.md').write_text('Documentation does not affect rendered pixels.')
            self.assertEqual(initial, gallery.source_fingerprint(root))
            (root/'src/example.c').write_bytes(b'int image = 2;\n')
            self.assertNotEqual(initial, gallery.source_fingerprint(root))
            with mock.patch.object(gallery, 'generate') as generate:
                with self.assertRaisesRegex(ValueError, 'unstaged'):
                    gallery.pre_commit(root)
                generate.assert_not_called()

    def test_manifest_integrity_and_hook_review(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            fixture(root)
            references(root)
            command(root, 'add', 'review/hand-gallery')
            gallery.verify(root)
            gallery.verify(root, True)
            with mock.patch.object(gallery, 'generate') as generate:
                gallery.pre_commit(root)
                generate.assert_not_called()
            original_index = command(root, 'show', ':review/hand-gallery/manifest.json')
            (root/'src/example.c').write_text('int image = 3;\n')
            command(root, 'add', 'src/example.c')
            with mock.patch.object(gallery, 'generate', side_effect=lambda r: references(r)) as generate:
                with self.assertRaisesRegex(ValueError, 'Nothing was automatically staged'):
                    gallery.pre_commit(root)
                generate.assert_called_once()
            self.assertEqual(command(root, 'show', ':review/hand-gallery/manifest.json'), original_index)
            command(root, 'add', 'review/hand-gallery')
            gallery.verify(root, True)
            path = root/gallery.OUTPUT/gallery.cases()[0]['file']
            path.write_bytes(path.read_bytes()[:-1])
            with self.assertRaises(ValueError):
                gallery.verify(root)

    def test_repeat_capture_rejects_missing_and_changing_pixels(self):
        with mock.patch.object(gallery, 'bmp_pixels', return_value=b'fixed'):
            log = ''
            for row in gallery.cases():
                for sample in (1, 2):
                    log += (f"ge: hand-gallery capture case={row['id']} sample={sample} item={row['weapon_id']} "
                            f"wardrobe={row['id']//12} ads={int(row['mode']=='ads')} stock={int(row['variant']=='stock')}\n"
                            f"gfx: wrote shot_{row['id']*2+sample:05}.bmp\n")
            log += 'ge: hand-gallery complete cases=72 samples=144\n'
            self.assertEqual(len(gallery.capture_pass(log, Path('.'))), 72)
            with self.assertRaises(ValueError):
                gallery.capture_pass(log.replace('sample=1 item=4', 'sample=2 item=4', 1), Path('.'))
            with mock.patch.object(gallery, 'bmp_pixels', side_effect=[b'a', b'b']+[b'a']*142):
                with self.assertRaisesRegex(ValueError, 'Non-deterministic'):
                    gallery.capture_pass(log, Path('.'))

    def test_installer_keeps_existing_hooks(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            fixture(root)
            hook = root/'.git/hooks/pre-commit'
            hook.write_text('#!/bin/sh\nexit 0\n')
            with self.assertRaisesRegex(ValueError, 'existing pre-commit'):
                gallery.install_hook(root)
            self.assertEqual(hook.read_text(), '#!/bin/sh\nexit 0\n')


if __name__ == '__main__':
    unittest.main()
