"""ROM-free tests for the container runner that builds and runs the game per architecture."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('lp64_env', Path(__file__).with_name('lp64_env.py'))
env = importlib.util.module_from_spec(spec)
spec.loader.exec_module(env)


class DockerCommand(unittest.TestCase):
    def test_i686_and_x86_64_share_the_amd64_image(self):
        for arch in ('i686', 'x86_64'):
            # Arrange / Act
            argv = env.docker_command(arch, Path('/repo'), None, ['true'])
            # Assert
            self.assertIn('linux/amd64', argv)
            self.assertIn('goldeneye', argv)

    def test_aarch64_uses_the_native_image(self):
        # Arrange / Act
        argv = env.docker_command('aarch64', Path('/repo'), None, ['true'])
        # Assert
        self.assertIn('linux/arm64', argv)
        self.assertIn('midas-lp64-arm64', argv)

    def test_checkout_is_mounted_as_the_working_directory(self):
        # Arrange / Act
        argv = env.docker_command('i686', Path('/repo'), None, ['true'])
        # Assert
        self.assertIn('/repo:/home/dev/project', argv)
        self.assertEqual(argv[argv.index('-w') + 1], '/home/dev/project')

    def test_rom_with_spaces_and_brackets_is_one_read_only_argument(self):
        # Arrange
        rom = Path('/repo/GoldenEye 007 (U) [!].z64')
        # Act
        argv = env.docker_command('i686', Path('/repo'), rom, ['true'])
        # Assert
        self.assertIn('/repo/GoldenEye 007 (U) [!].z64:/rom/baserom.u.z64:ro', argv)
        self.assertIn('GE_ROM=/rom/baserom.u.z64', argv)

    def test_no_rom_means_no_rom_mount(self):
        # Arrange / Act
        argv = env.docker_command('i686', Path('/repo'), None, ['true'])
        # Assert
        self.assertFalse(any('/rom/' in part for part in argv))

    def test_command_is_passed_through_unchanged(self):
        # Arrange / Act
        argv = env.docker_command('aarch64', Path('/repo'), None, ['sh', '-c', 'echo "a b"'])
        # Assert
        self.assertEqual(argv[-3:], ['sh', '-c', 'echo "a b"'])

    def test_unknown_arch_is_rejected(self):
        # Arrange / Act / Assert
        with self.assertRaises(ValueError):
            env.docker_command('mips', Path('/repo'), None, ['true'])


class FindRom(unittest.TestCase):
    def test_ge_rom_wins_over_files_in_root(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder, tempfile.TemporaryDirectory() as elsewhere:
            root = Path(folder)
            (root/'a.z64').write_bytes(b'')
            rom = Path(elsewhere).resolve()/'x.z64'
            rom.write_bytes(b'')
            # Act
            found = env.find_rom(root, {'GE_ROM': str(rom)})
        # Assert
        self.assertEqual(found, rom)

    def test_a_relative_ge_rom_becomes_an_absolute_path(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder:
            here = Path(folder).resolve()
            (here/'x.z64').write_bytes(b'')
            previous = os.getcwd()
            os.chdir(here)
            self.addCleanup(os.chdir, previous)
            # Act
            found = env.find_rom(Path('/repo'), {'GE_ROM': 'x.z64'})
        # Assert
        self.assertEqual(found, here/'x.z64')

    def test_a_missing_ge_rom_is_refused_naming_the_path(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder:
            missing = Path(folder).resolve()/'typo.z64'
            # Act / Assert
            with self.assertRaises(ValueError) as caught:
                env.find_rom(Path('/repo'), {'GE_ROM': str(missing)})
        self.assertIn(str(missing), str(caught.exception))

    def test_a_directory_as_ge_rom_is_refused(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder:
            # Act / Assert
            with self.assertRaises(ValueError) as caught:
                env.find_rom(Path('/repo'), {'GE_ROM': folder})
        self.assertIn(str(Path(folder).resolve()), str(caught.exception))

    def test_run_with_a_bad_ge_rom_exits_2_without_calling_docker(self):
        # Arrange
        with mock.patch.dict(os.environ, {'GE_ROM': '/no/such/rom.z64'}), \
                mock.patch.object(env.subprocess, 'run') as docker, \
                contextlib.redirect_stderr(io.StringIO()) as errors:
            # Act
            status = env.main(['run', '--arch', 'x86_64', '--', 'true'])
        # Assert
        self.assertEqual(status, 2)
        docker.assert_not_called()
        self.assertIn('/no/such/rom.z64', errors.getvalue())

    def test_single_z64_in_root_is_found(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root/'GoldenEye 007 (U) [!].z64').write_bytes(b'')
            # Act
            found = env.find_rom(root, {})
        # Assert
        self.assertEqual(found, root/'GoldenEye 007 (U) [!].z64')

    def test_two_z64_files_are_ambiguous(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root/'a.z64').write_bytes(b'')
            (root/'b.z64').write_bytes(b'')
            # Act / Assert
            with self.assertRaises(ValueError):
                env.find_rom(root, {})

    def test_no_z64_means_none(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder:
            # Act
            found = env.find_rom(Path(folder), {})
        # Assert
        self.assertIsNone(found)


if __name__ == '__main__':
    unittest.main()
