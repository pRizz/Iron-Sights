"""Build the i686 game and reporting runtime inside linux-release.Dockerfile."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

from sentry_tools import checkout_source, licenses, run, CONFIG

ROOT = Path(__file__).resolve().parents[2]
SDL_VERSION = "2.30.11"
SDL_SHA256 = "8b8d4aef2038533da814965220f88f77d60dfa0f32685f80ead65e501337da7f"


def download(url, digest, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest() != digest:
        with urllib.request.urlopen(url, timeout=120) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError("Download SHA256 mismatch: " + url)
        target.write_bytes(data)
    return target


def sdl(work):
    source, build, prefix = work / "sdl-source", work / "sdl-build", work / "sdl"
    archive = download(f"https://github.com/libsdl-org/SDL/releases/download/release-{SDL_VERSION}/SDL2-{SDL_VERSION}.tar.gz",
                       SDL_SHA256, work / "downloads/sdl.tar.gz")
    if not source.exists():
        with tarfile.open(archive) as stream:
            stream.extractall(work / "sdl-unpack")
        (work / "sdl-unpack" / f"SDL2-{SDL_VERSION}").rename(source)
    run("cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache",
        "-DCMAKE_C_FLAGS=-m32", "-DCMAKE_CXX_FLAGS=-m32", "-DCMAKE_SHARED_LINKER_FLAGS=-m32",
        "-DCMAKE_INSTALL_PREFIX=" + str(prefix), "-DCMAKE_INSTALL_LIBDIR=lib/i386-linux-gnu",
        "-DSDL_SHARED=ON", "-DSDL_STATIC=OFF", "-DSDL_TESTS=OFF", "-DSDL_WAYLAND=OFF",
        "-DSDL_IBUS=OFF", "-DSDL_FCITX=OFF", "-DSDL_DBUS=OFF", "-DSDL_JACK=OFF",
        "-DSDL_SNDIO=OFF", "-DSDL_LIBSAMPLERATE=OFF", "-DSDL_X11=ON", "-DSDL_X11_SHARED=ON",
        "-DSDL_OPENGL=ON", "-DSDL_ALSA=ON", "-DSDL_ALSA_SHARED=ON",
        "-DSDL_PULSEAUDIO=ON", "-DSDL_PULSEAUDIO_SHARED=ON")
    run("cmake", "--build", build, "--parallel", "4")
    run("cmake", "--install", build)
    return source, prefix


def sdk(work, output):
    source, build = checkout_source(work / "sdk")
    run("cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
        "-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache",
        "-DSENTRY_BUILD_FORCE32=ON", "-DSENTRY_BACKEND=native", "-DSENTRY_BUILD_SHARED_LIBS=ON",
        "-DSENTRY_BUILD_TESTS=OFF", "-DSENTRY_BUILD_EXAMPLES=OFF", "-DSENTRY_LINK_CURL=OFF",
        "-DCMAKE_BUILD_RPATH=$ORIGIN/lib;$ORIGIN", "-DCMAKE_INSTALL_RPATH=$ORIGIN/lib;$ORIGIN")
    run("cmake", "--build", build, "--parallel", "4")
    (output / "lib").mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / "libsentry.so", output / "lib/libsentry.so")
    shutil.copy2(build / "sentry-crash", output / "sentry-crash")
    licenses(source, output)
    with (output / "Sentry-LICENSE.txt").open("a") as notice:
        notice.write("\nVendored libunwind\n" + (source / "vendor/libunwind/COPYING").read_text())
    return source


def split_symbols(path, symbols):
    target = symbols / (path.name + ".debug")
    run("objcopy", "--only-keep-debug", path, target)


def build(work, output):
    if os.environ.get("MIDAS_CLEAN_BUILD") == "true":
        os.environ["CCACHE_DISABLE"] = "1"
    source, prefix = sdl(work)
    sentry_source = sdk(work, output)
    shutil.copy2(prefix / "lib/i386-linux-gnu/libSDL2-2.0.so.0", output / "lib/libSDL2-2.0.so.0")
    shutil.copy2(source / "LICENSE.txt", output / "SDL2-LICENSE.txt")
    run("cmake", "-S", ROOT, "-B", output, "-DCMAKE_BUILD_TYPE=Debug", "-DGE_VIDEO=ON",
        "-DGE_ROMDATA_FALLBACK=OFF", "-DSDL2_LINUX_ROOT=" + str(prefix),
        "-DMIDAS_SENTRY_INCLUDE_DIR=" + str(sentry_source / "include"),
        "-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
    run("cmake", "--build", output, "--parallel", "4")
    symbols = output / "symbols"
    symbols.mkdir(exist_ok=True)
    for path in (output / "IronSights", output / "sentry-crash", output / "lib/libsentry.so"):
        split_symbols(path, symbols)
    # Remove build-host RPATHs. Bundle lookup is relative to the loaded ELF.
    run("patchelf", "--set-rpath", "$ORIGIN/lib", output / "IronSights")
    run("patchelf", "--set-rpath", "$ORIGIN/lib", output / "sentry-crash")
    run("patchelf", "--set-rpath", "$ORIGIN", output / "lib/libsentry.so")
    hashes = {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in (output / "lib/libsentry.so", output / "sentry-crash")}
    info = dict(sdk_version=CONFIG["sdk_version"], sdk_commit=CONFIG["sdk_commit"], architecture="i686",
                backend="native", compiler=subprocess.check_output(["gcc", "--version"], text=True).splitlines()[0],
                sha256=hashes, sdl_version=SDL_VERSION, sdl_sha256=SDL_SHA256)
    (output / "sentry-build.json").write_text(json.dumps(info, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, default=ROOT / "build_linux/deps")
    parser.add_argument("--output", type=Path, default=ROOT / "build_linux")
    args = parser.parse_args()
    build(args.work.resolve(), args.output.resolve())
