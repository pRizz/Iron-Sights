"""ROM-free tests for the Linux release build's per-architecture configure arguments."""
import contextlib
import importlib.util
import io
from pathlib import Path
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("linux_build", HERE / "linux_build.py")
linux_build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(linux_build)

SOURCE, BUILD, PREFIX, OUTPUT, INCLUDE = (Path(name) for name in ("SOURCE", "BUILD", "PREFIX", "OUTPUT", "INCLUDE"))

# The argument lists linux_build.py passed to run() at f04e0a7, with its paths as placeholders.
I686_SDL = [
    "cmake", "-S", "SOURCE", "-B", "BUILD", "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache",
    "-DCMAKE_C_FLAGS=-m32", "-DCMAKE_CXX_FLAGS=-m32", "-DCMAKE_SHARED_LINKER_FLAGS=-m32",
    "-DCMAKE_INSTALL_PREFIX=PREFIX", "-DCMAKE_INSTALL_LIBDIR=lib/i386-linux-gnu",
    "-DSDL_SHARED=ON", "-DSDL_STATIC=OFF", "-DSDL_TESTS=OFF", "-DSDL_WAYLAND=OFF",
    "-DSDL_IBUS=OFF", "-DSDL_FCITX=OFF", "-DSDL_DBUS=OFF", "-DSDL_JACK=OFF",
    "-DSDL_SNDIO=OFF", "-DSDL_LIBSAMPLERATE=OFF", "-DSDL_X11=ON", "-DSDL_X11_SHARED=ON",
    "-DSDL_OPENGL=ON", "-DSDL_ALSA=ON", "-DSDL_ALSA_SHARED=ON",
    "-DSDL_PULSEAUDIO=ON", "-DSDL_PULSEAUDIO_SHARED=ON"]
I686_SDK = [
    "cmake", "-S", "SOURCE", "-B", "BUILD", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
    "-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache",
    "-DSENTRY_BUILD_FORCE32=ON", "-DSENTRY_BACKEND=native", "-DSENTRY_BUILD_SHARED_LIBS=ON",
    "-DSENTRY_BUILD_TESTS=OFF", "-DSENTRY_BUILD_EXAMPLES=OFF", "-DSENTRY_LINK_CURL=OFF",
    "-DCMAKE_BUILD_RPATH=$ORIGIN/lib;$ORIGIN", "-DCMAKE_INSTALL_RPATH=$ORIGIN/lib;$ORIGIN"]
I686_GAME = [
    "cmake", "-S", str(linux_build.ROOT), "-B", "OUTPUT", "-DCMAKE_BUILD_TYPE=Debug", "-DGE_VIDEO=ON",
    "-DGE_ROMDATA_FALLBACK=OFF", "-DSDL2_LINUX_ROOT=PREFIX",
    "-DMIDAS_SENTRY_INCLUDE_DIR=INCLUDE",
    "-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache"]


def strings(arguments):
    """Return the arguments as run() hands them to the process."""
    return [str(argument) for argument in arguments]


class I686Unchanged(unittest.TestCase):
    def test_i686_sdl_arguments_are_unchanged(self):
        # Arrange / Act
        arguments = linux_build.sdl_configure_args("i686", SOURCE, BUILD, PREFIX)
        # Assert
        self.assertEqual(strings(arguments), I686_SDL)

    def test_i686_sdk_arguments_are_unchanged(self):
        # Arrange / Act
        arguments = linux_build.sdk_configure_args("i686", SOURCE, BUILD)
        # Assert
        self.assertEqual(strings(arguments), I686_SDK)

    def test_i686_game_arguments_are_unchanged(self):
        # Arrange / Act
        arguments = linux_build.game_configure_args("i686", OUTPUT, PREFIX, INCLUDE)
        # Assert
        self.assertEqual(strings(arguments), I686_GAME)


class X86_64(unittest.TestCase):
    def test_x86_64_game_asks_for_64_bit_pointers(self):
        # Arrange / Act
        arguments = strings(linux_build.game_configure_args("x86_64", OUTPUT, PREFIX, INCLUDE))
        # Assert
        self.assertIn("-DGE_POINTER_BITS=64", arguments)
        self.assertFalse([a for a in arguments if "m32" in a or "i386" in a])

    def test_x86_64_sdl_is_64_bit_in_the_linked_directory(self):
        # Arrange / Act
        arguments = strings(linux_build.sdl_configure_args("x86_64", SOURCE, BUILD, PREFIX))
        # Assert
        self.assertIn("-DCMAKE_INSTALL_LIBDIR=lib", arguments)
        self.assertFalse([a for a in arguments if "m32" in a or "i386" in a])

    def test_x86_64_sdk_is_not_forced_to_32_bits(self):
        # Arrange / Act
        arguments = strings(linux_build.sdk_configure_args("x86_64", SOURCE, BUILD))
        # Assert
        self.assertFalse([a for a in arguments if "FORCE32" in a])


class CommandLine(unittest.TestCase):
    def test_unknown_architecture_is_refused(self):
        # Arrange
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as caught:
            # Act
            linux_build.main(["--arch", "mips"])
        # Assert
        self.assertEqual(caught.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
