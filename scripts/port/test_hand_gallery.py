"""ROM-free tests for gallery pixels, completeness, provenance and commit safety."""
import contextlib
import importlib.util
import io
import os
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
    (root/'port/fast3d').mkdir(parents=True)
    (root/'port/fast3d/example.cpp').write_bytes(b'int renderer = 1;\n')
    command(root, 'add', 'src/example.c', 'port/fast3d/example.cpp')


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
    manifest = dict(schema=2, recipe=gallery.RECIPE, inputs_sha256=gallery.source_fingerprint(root),
                    strict_inputs_sha256=gallery.source_fingerprint(root, strict=True),
                    rom_sha256='fixture', renderer={'renderer': 'Synthetic test'},
                    repeat_verified=True, independent_pass_verified=True, cases=rows, icons=icons)
    (directory/'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True)+'\n')
    (directory/'index.html').write_text(gallery.page(manifest), encoding='utf-8', newline='\n')
    return manifest


def staged_references(root):
    fixture(root)
    manifest = references(root)
    command(root, 'add', 'review/hand-gallery')
    return manifest


def change_and_stage(root, name, text):
    (root/name).write_text(text)
    command(root, 'add', name)


def switch_branch(root, name):
    """Point HEAD at a branch; works before the first commit, as in the fixture."""
    command(root, 'symbolic-ref', 'HEAD', 'refs/heads/'+name)


def commit_index(root):
    """A commit of the index, made without running any commit hook; returns its ID."""
    tree = command(root, 'write-tree').decode().strip()
    identity = ('-c', 'user.name=Gallery test', '-c', 'user.email=gallery@example.invalid')
    return command(root, *identity, 'commit-tree', tree, '-m', 'fixture').decode().strip()


def detach(root):
    """Detach HEAD at a commit of the index, without running any commit hook."""
    command(root, 'update-ref', '--no-deref', 'HEAD', commit_index(root))


def strict_change_with_defer(root):
    """A deferring clone with a staged strict-tier change over a fresh gallery."""
    staged_references(root)
    change_and_stage(root, 'port/fast3d/example.cpp', 'int renderer = 2;\n')
    command(root, 'config', 'midas.galleryDefer', 'true')


STALE_ON_WORK = ('Hand gallery is STALE on branch work: strict inputs changed. '
                 'Regenerate on a machine that can render before this reaches main.')


def run_hook(root):
    """Run pre_commit with generation failing as on a clone without a ROM; returns (error, stdout)."""
    output = io.StringIO()
    error = None
    with contextlib.redirect_stdout(output), mock.patch.object(gallery, 'generate', side_effect=OSError('no ROM')):
        try:
            gallery.pre_commit(root)
        except (ValueError, OSError) as raised:
            error = raised
    return error, output.getvalue()


def run_main(root, *args):
    """Run the CLI against a temporary repository; returns (exit code, stdout)."""
    output = io.StringIO()
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(io.StringIO()):
        code = gallery.main(list(args), root)
    return code, output.getvalue()


@contextlib.contextmanager
def no_global_git_config(folder):
    """Hide the machine's own Git configuration from the code under test."""
    empty = Path(folder)/'empty.gitconfig'
    empty.write_text('')
    with mock.patch.dict(os.environ, GIT_CONFIG_GLOBAL=str(empty), GIT_CONFIG_NOSYSTEM='1'):
        yield


class Gallery(unittest.TestCase):
    def test_default_windows_build_refuses_a_running_player(self):
        # Arrange: the process check reports an active player. No builder may run.
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            with mock.patch.object(gallery.os, 'name', 'nt'), mock.patch.object(gallery.subprocess, 'run',
                    return_value=mock.Mock(returncode=1)) as process_check:
                # Act / Assert
                with self.assertRaisesRegex(ValueError, 'Player executable is running'):
                    gallery.build(root, root)
                self.assertEqual(process_check.call_count, 1)

    def test_build_templates_and_live_asset_objects_are_render_inputs(self):
        # Arrange
        paths = ['CMakePresets.json', 'port/web/sampler.glsl', 'port/web/sampler_shader.h.in',
                 'assets/oddtextures.c', 'assets/rarewarelogo.c', 'assets/font_dl.c']
        # Act / Assert
        for path in paths:
            self.assertTrue(gallery.input_path(path), path)
        self.assertFalse(gallery.input_path('web/README.md'))
        self.assertFalse(gallery.input_path('build/web/ProjectMidas.wasm'))

    def test_matrix_coordinates(self):
        rows = gallery.cases()
        self.assertEqual(len(rows), 108)
        self.assertEqual(len({row['file'] for row in rows}), 108)
        for i, row in enumerate(rows):
            self.assertEqual(row['id'], i)
            # The game's joyPortHandGallery uses the same case arithmetic.
            hip = i >= 72
            self.assertEqual(row['wardrobe'], gallery.WARDROBES[(i-72)//6 if hip else i//12])
            self.assertEqual(row['weapon_id'], (4, 8, 7)[(i-72)//2 % 3 if hip else i//4 % 3])
            self.assertEqual(row['mode'], 'hip_arms' if hip else ('regular', 'ads')[i//2 % 2])
            self.assertEqual(row['variant'], ('stock', 'fork')[i % 2])
        self.assertEqual(sum(name is not None for name in gallery.ICON_NAMES[1:]), 38)

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

    def test_strict_tier_membership(self):
        # Arrange / Act / Assert
        for name in gallery.STRICT_FILES:
            self.assertTrue(gallery.input_path(name), name)
            self.assertTrue(gallery.strict_path(name), name)
            self.assertTrue((gallery.ROOT/name).is_file(), name)
        self.assertTrue(gallery.strict_path('port/fast3d/gfx_pc.cpp'))
        for name in ('src/game/front.c', 'port/src/romdata.c', 'CMakeLists.txt',
                     '.github/workflows/windows-release.yml', 'notes.md'):
            self.assertFalse(gallery.strict_path(name), name)

    def test_strict_fingerprint_ignores_deferred_files(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            fixture(root)
            strict = gallery.source_fingerprint(root, strict=True)
            full = gallery.source_fingerprint(root)
            # Act
            (root/'src/example.c').write_bytes(b'int image = 2;\n')
            # Assert
            self.assertEqual(strict, gallery.source_fingerprint(root, strict=True))
            self.assertNotEqual(full, gallery.source_fingerprint(root))

    def test_deferred_change_is_pending(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            manifest = staged_references(root)
            change_and_stage(root, 'src/example.c', 'int image = 3;\n')
            # Act / Assert
            self.assertEqual(gallery.freshness(manifest, root, True), 'pending')
            with self.assertRaisesRegex(ValueError, 'stale'):
                gallery.verify(root, True)
            gallery.verify(root, True, allow_pending=True)

    def test_strict_change_is_stale_even_when_pending_allowed(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            manifest = staged_references(root)
            change_and_stage(root, 'port/fast3d/example.cpp', 'int renderer = 2;\n')
            # Act / Assert
            self.assertEqual(gallery.freshness(manifest, root, True), 'stale')
            with self.assertRaisesRegex(ValueError, 'strict'):
                gallery.verify(root, True, allow_pending=True)

    def test_hook_without_the_clone_setting_tries_to_generate(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            staged_references(root)
            change_and_stage(root, 'src/example.c', 'int image = 3;\n')
            self.assertFalse(gallery.defer_enabled(root))
            with mock.patch.object(gallery, 'generate', side_effect=OSError('no ROM')) as generate:
                # Act / Assert
                with self.assertRaisesRegex(ValueError, 'midas.galleryDefer'):
                    gallery.pre_commit(root)
                generate.assert_called_once()

    def test_hook_with_the_clone_setting_passes_pending(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            staged_references(root)
            change_and_stage(root, 'src/example.c', 'int image = 3;\n')
            command(root, 'config', 'midas.galleryDefer', 'true')
            self.assertTrue(gallery.defer_enabled(root))
            with mock.patch.object(gallery, 'generate') as generate:
                # Act
                gallery.pre_commit(root)
                # Assert
                generate.assert_not_called()

    def test_defer_setting_outside_the_clone_is_ignored(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)/'repo'
            root.mkdir()
            fixture(root)
            global_config = Path(folder)/'global.gitconfig'
            global_config.write_text('[midas]\n\tgalleryDefer = true\n')
            with mock.patch.dict(os.environ, GIT_CONFIG_GLOBAL=str(global_config)):
                # Act / Assert
                self.assertFalse(gallery.defer_enabled(root))

    def test_cli_verify_allow_pending_accepts_pending(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            staged_references(root)
            change_and_stage(root, 'src/example.c', 'int image = 3;\n')
            # Act
            code, output = run_main(root, 'verify', '--allow-pending')
            # Assert
            self.assertEqual(code, 0)
            self.assertIn('Gallery pending: strict inputs fresh; deferred inputs changed since the last render.', output)

    def test_cli_verify_allow_pending_rejects_strict_stale(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            staged_references(root)
            change_and_stage(root, 'port/fast3d/example.cpp', 'int renderer = 2;\n')
            # Act
            code, output = run_main(root, 'verify', '--allow-pending')
            # Assert
            self.assertEqual(code, 1)
            self.assertNotIn('Gallery pending', output)

    def test_cli_plain_verify_rejects_pending(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            staged_references(root)
            change_and_stage(root, 'src/example.c', 'int image = 3;\n')
            # Act
            code, output = run_main(root, 'verify')
            # Assert
            self.assertEqual(code, 1)
            self.assertNotIn('Gallery pending', output)

    def test_install_hook_defer_sets_the_local_setting(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)/'repo'
            root.mkdir()
            fixture(root)
            with no_global_git_config(folder):
                # Act
                gallery.install_hook(root, defer=True)
                # Assert
                self.assertEqual(command(root, 'config', '--local', 'midas.galleryDefer').strip(), b'true')

    def test_install_hook_without_defer_leaves_the_setting_unset(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)/'repo'
            root.mkdir()
            fixture(root)
            with no_global_git_config(folder):
                # Act
                gallery.install_hook(root)
                # Assert
                self.assertFalse(gallery.defer_enabled(root))
                self.assertEqual(command(root, 'config', '--local', 'core.hooksPath').strip(), b'.githooks')

    def test_schema_1_manifest_must_be_regenerated(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            manifest = staged_references(root)
            manifest.update(schema=1)
            del manifest['strict_inputs_sha256']
            (root/gallery.OUTPUT/'manifest.json').write_text(json.dumps(manifest))
            # Act / Assert
            with self.assertRaisesRegex(ValueError, 'regenerate'):
                gallery.verify(root)

    def test_repeat_capture_rejects_missing_and_changing_pixels(self):
        with mock.patch.object(gallery, 'bmp_pixels', return_value=b'fixed'):
            log = ''
            for row in gallery.cases():
                for sample in (1, 2):
                    log += (f"ge: hand-gallery capture case={row['id']} sample={sample} item={row['weapon_id']} "
                            f"wardrobe={gallery.WARDROBES.index(row['wardrobe'])} ads={int(row['mode']=='ads')} stock={int(row['variant']=='stock')} "
                            f"hip={int(row['mode']=='hip_arms')}\n"
                            f"gfx: wrote shot_{row['id']*2+sample:05}.bmp\n")
            log += 'ge: hand-gallery complete cases=108 samples=216\n'
            self.assertEqual(len(gallery.capture_pass(log, Path('.'))), 108)
            with self.assertRaises(ValueError):
                gallery.capture_pass(log.replace('sample=1 item=4', 'sample=2 item=4', 1), Path('.'))
            with mock.patch.object(gallery, 'bmp_pixels', side_effect=[b'a', b'b']+[b'a']*214):
                with self.assertRaisesRegex(ValueError, 'Non-deterministic'):
                    gallery.capture_pass(log, Path('.'))

    def test_deferring_clone_commits_strict_change_on_a_branch(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            strict_change_with_defer(root)
            switch_branch(root, 'work')
            self.assertEqual(gallery.head_ref(root), 'refs/heads/work')
            # Act
            error, output = run_hook(root)
            # Assert
            self.assertIsNone(error)
            self.assertIn(STALE_ON_WORK, output)

    def test_deferring_clone_refuses_strict_change_on_main(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            strict_change_with_defer(root)
            self.assertEqual(gallery.head_ref(root), 'refs/heads/main')
            # Act
            error, output = run_hook(root)
            # Assert
            self.assertIsInstance(error, OSError)
            self.assertNotIn('STALE on branch', output)

    def test_deferring_clone_refuses_strict_change_on_detached_head(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            strict_change_with_defer(root)
            detach(root)
            self.assertIsNone(gallery.head_ref(root))
            # Act
            error, output = run_hook(root)
            # Assert
            self.assertIsInstance(error, OSError)
            self.assertNotIn('STALE on branch', output)

    def test_rendering_clone_on_a_branch_still_tries_to_generate(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            staged_references(root)
            change_and_stage(root, 'port/fast3d/example.cpp', 'int renderer = 2;\n')
            switch_branch(root, 'work')
            self.assertFalse(gallery.defer_enabled(root))
            with mock.patch.object(gallery, 'generate', side_effect=OSError('no ROM')) as generate:
                # Act / Assert
                with self.assertRaisesRegex(ValueError, 'midas.galleryDefer'):
                    gallery.pre_commit(root)
                generate.assert_called_once()

    def test_branch_exception_still_rejects_a_corrupted_image(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            strict_change_with_defer(root)
            switch_branch(root, 'work')
            path = root/gallery.OUTPUT/gallery.cases()[0]['file']
            path.write_bytes(path.read_bytes()[:-1])
            command(root, 'add', 'review/hand-gallery')
            # Act
            error, output = run_hook(root)
            # Assert
            self.assertIsNotNone(error)
            self.assertIn('Gallery check on this branch: Truncated PNG', output)
            self.assertNotIn('STALE on branch', output)

    def test_deferring_clone_refuses_strict_change_on_main_beside_a_main_tag(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange: with a tag named main, `symbolic-ref --short` prints heads/main.
            root = Path(folder)
            strict_change_with_defer(root)
            commit = commit_index(root)
            command(root, 'update-ref', 'refs/heads/main', commit)
            command(root, 'tag', 'main', commit)
            # Act
            error, output = run_hook(root)
            # Assert
            self.assertIsInstance(error, OSError)
            self.assertNotIn('STALE on branch', output)

    def test_deferring_clone_refuses_strict_change_on_a_differently_cased_main(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange: refs/heads/Main is the main ref on a case-insensitive volume.
            root = Path(folder)
            strict_change_with_defer(root)
            switch_branch(root, 'Main')
            # Act
            error, output = run_hook(root)
            # Assert
            self.assertIsInstance(error, OSError)
            self.assertNotIn('STALE on branch', output)

    def test_branches_that_only_contain_main_are_ordinary(self):
        for name in ('feature/main', 'maintenance'):
            with self.subTest(branch=name), tempfile.TemporaryDirectory() as folder:
                # Arrange
                root = Path(folder)
                strict_change_with_defer(root)
                switch_branch(root, name)
                # Act
                error, output = run_hook(root)
                # Assert
                self.assertIsNone(error)
                self.assertIn(f'Hand gallery is STALE on branch {name}: strict inputs changed.', output)

    def test_cli_verify_rejects_strict_stale_on_a_branch(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root = Path(folder)
            strict_change_with_defer(root)
            switch_branch(root, 'work')
            # Act
            code, output = run_main(root, 'verify', '--allow-pending')
            # Assert
            self.assertEqual(code, 1)
            self.assertNotIn('STALE on branch', output)

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
