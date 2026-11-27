"""Allow-listed Linux tarball, type-2 AppImage and Steam Deck ZIP; no player data is swept in.

`--arch i686` (the default) packages linux_build.py's i686 tree; `--arch x86_64` its x86_64 tree.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tarfile
import zipfile
from typing import NamedTuple

from linux_build import download

ROOT = Path(__file__).resolve().parents[2]
RUNTIME_SOURCE = "dd6cebedcbddde9c82f89b011e8e1d40b6e43868"
RUNTIME_LICENSE_SHA256 = "aa154fc9070614bbe7921f89db11efd1dba7a1f3a41685958110e2230f9c0ca1"
SYSTEM_LIBRARIES = re.compile(r"^(ld-linux|lib(c|m|dl|pthread|rt|resolv|util)\.so|libnss_|lib(GL|EGL|GLX|GLdispatch|OpenGL)\.)")
PLAYER_FILES = ("IronSights", "sentry-crash", "Sentry-LICENSE.txt", "sentry-build.json", "SDL2-LICENSE.txt",
                "gamecontrollerdb.txt", "gamecontrollerdefault.txt", "midas_ini_help.txt", "Data/shaders_base.txt")
STEAM_ART = tuple(ROOT / "scripts/port/linux/steam-art" / name for name in ("cover.png", "banner.png", "hero.png", "logo.png"))
# A library resolved from one of these folders is 32-bit; an x86_64 package never loads one.
FOREIGN_LIBDIR = re.compile(r"/(i386[^/]*|lib32)/")
TRUE = ("ON", "TRUE", "YES", "Y", "1")
# Cache options a shipped build must not have on, with what such a build is.
# A system GL driver built with a newer GCC loads into the game's process and needs the system's newer
# C++ runtime (Mesa 25.2's libLLVM needs GLIBCXX_3.4.32); a bundled older copy loaded first breaks it.
SYSTEM_CXX_RUNTIME = ("libstdc++.so.6", "libgcc_s.so.1")
UNSHIPPABLE_OPTIONS = (("GE_SANITIZE", "a sanitizer build"), ("GE_LP64_ORACLE", "an oracle build"),
                       ("GE_HEADLESS", "a headless build"),
                       ("GE_ROMDATA_FALLBACK", "a build with compiled-in asset copies"))


class Target(NamedTuple):
    """What differs between the Linux packages of one architecture."""
    suffix: str
    elf_class: int
    elf_machine: int
    system_libdir: str
    runtime_url: str
    runtime_sha256: str
    dockerfile: str
    baseline: str
    start_here: str
    steam_deck: str
    sums_name: str
    info_prefix: str
    # Libraries this target takes from the player's system rather than bundling, beyond SYSTEM_LIBRARIES.
    system_libraries: tuple


TARGETS = {
    "i686": Target("i686", 1, 3, "/usr/lib/i386-linux-gnu",
                   "https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-i686",
                   "e72ea0b140a0a16e680713238a6f30aad278b62c4ca17919c554864124515498",
                   "scripts/port/linux-release.Dockerfile", "Ubuntu 22.04 / glibc 2.35",
                   "scripts/port/linux/START-HERE.txt", "scripts/port/linux/STEAM-DECK.txt",
                   "SHA256SUMS-linux.txt", "linux", SYSTEM_CXX_RUNTIME),
    "x86_64": Target("x86_64", 2, 62, "/usr/lib/x86_64-linux-gnu",
                     "https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64",
                     "2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d",
                     "scripts/port/linux-release-x86_64.Dockerfile", "Ubuntu 22.04 / glibc 2.35",
                     "scripts/port/linux/START-HERE-x86_64.txt", "scripts/port/linux/STEAM-DECK-x86_64.txt",
                     "SHA256SUMS-linux-x86_64.txt", "linux-x86_64", SYSTEM_CXX_RUNTIME),
}


def output_names(target):
    """Return the file and folder names the packages of TARGET are written under."""
    linux, deck = "Iron-Sights-linux-" + target.suffix, "Iron-Sights-steamdeck-" + target.suffix
    return dict(appdir=linux + ".AppDir", folder=linux, tarball=linux + ".tar.gz", appimage=linux + ".AppImage",
                steamdeck=deck + ".zip", steamdeck_folder=deck, runtime="runtime-" + target.suffix,
                sums=target.sums_name, build_info=target.info_prefix + "-build-info.json",
                build_inputs=target.info_prefix + "-build-inputs.json")


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def resolved_libraries(ldd, target):
    """Return the non-system libraries named in one `ldd` text, refusing what TARGET cannot load."""
    if "not found" in ldd:
        raise ValueError("Unresolved runtime dependency: " + ldd)
    paths = {}
    for line in ldd.splitlines():
        match = re.match(r"\s*(\S+) => (/\S+)", line)
        if not match:
            continue
        if target.elf_class == 2 and FOREIGN_LIBDIR.search(match[2]):
            raise ValueError(f"{match[1]} resolves from a 32-bit library folder: {match[2]}")
        if not SYSTEM_LIBRARIES.match(match[1]) and match[1] not in target.system_libraries:
            paths[match[1]] = Path(match[2])
    return paths


def bundle_libraries(build, destination, target):
    system = Path(target.system_libdir)
    roots = [build / "IronSights", build / "sentry-crash", build / "lib/libsentry.so",
             build / "lib/libSDL2-2.0.so.0", system / "libcurl.so.4",
             system / "libasound.so.2", system / "libpulse.so.0"]
    paths = {p.name: p for p in roots[2:]}
    for root in roots:
        paths.update(resolved_libraries(subprocess.check_output(["ldd", str(root)], text=True), target))
    for name, path in paths.items():
        data = path.read_bytes()
        if data[:5] != b"\x7fELF" + bytes([target.elf_class]):
            raise ValueError(f"Expected an ELF{32 * target.elf_class} runtime: " + str(path))
        bundled = destination / "lib" / name
        copy(path, bundled)
        subprocess.run(["strip", "--strip-debug", str(bundled)], check=True)
        subprocess.run(["patchelf", "--set-rpath", "$ORIGIN", str(bundled)], check=True)
    return paths


def checksums(directory):
    files = sorted(p for p in directory.rglob("*") if p.is_file() and p.name != "SHA256SUMS.txt")
    (directory / "SHA256SUMS.txt").write_text("".join(
        f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(directory).as_posix()}\n" for p in files))


def package_steamdeck(appdir, output, target):
    names = output_names(target)
    archive = output / names["steamdeck"]
    extras = ((ROOT / "scripts/port/linux/steamdeck-run.sh", "run.sh"),
              (ROOT / target.steam_deck, "STEAM-DECK.txt"),
              (ROOT / "scripts/port/linux/set-steam-art.py", "set-steam-art.py"),
              *((path, "steam-art/" + path.name) for path in STEAM_ART))
    prefix = names["steamdeck_folder"] + "/"
    sums = (appdir / "SHA256SUMS.txt").read_text()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as stream:
        for path in sorted(appdir.rglob("*")):
            if path.is_file() and path != appdir / "SHA256SUMS.txt":
                stream.write(path, prefix + path.relative_to(appdir).as_posix())
        for source, name in extras:
            stream.write(source, prefix + name)
            sums += f"{hashlib.sha256(source.read_bytes()).hexdigest()}  {name}\n"
        stream.writestr(prefix + "SHA256SUMS.txt", sums)
    return archive


def cache_options(build):
    """Return the options of BUILD's CMakeCache.txt as a name -> value mapping; empty without a cache."""
    cache = build / "CMakeCache.txt"
    if not cache.is_file():
        return {}
    options = {}
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        name, separator, value = line.partition("=")
        if separator and not line.startswith(("#", "//")):
            options[name.split(":")[0]] = value.strip()
    return options


def has_symbol(path, name):
    """True when the ELF file at PATH names NAME in its symbol table or its dynamic one."""
    data = path.read_bytes()
    wide = data[4] == 2
    order = "<" if data[5] == 1 else ">"
    shoff, = struct.unpack_from(order + ("Q" if wide else "I"), data, 0x28 if wide else 0x20)
    shentsize, shnum = struct.unpack_from(order + "HH", data, 0x3A if wide else 0x2E)
    layout = order + ("IIQQQQIIQQ" if wide else "IIIIIIIIII")
    sections = [struct.unpack_from(layout, data, shoff + i * shentsize) for i in range(shnum)]
    wanted = name.encode() + b"\0"
    for kind, offset, size, link, entsize in ((s[1], s[4], s[5], s[6], s[9]) for s in sections):
        if kind not in (2, 11):  # SHT_SYMTAB, SHT_DYNSYM
            continue
        strings = data[sections[link][4]:sections[link][4] + sections[link][5]]
        for at in range(offset, offset + size, entsize):
            index, = struct.unpack_from(order + "I", data, at)
            if strings[index:index + len(wanted)] == wanted:
                return True
    return False


def refuse_unshippable_build(build, arch):
    """Raise when BUILD is not a release tree of ARCH: a test-only configuration or the other width."""
    refuse_comparison_build(build)
    options = cache_options(build)
    for option, kind in UNSHIPPABLE_OPTIONS:
        if options.get(option, "OFF").upper() in TRUE:
            raise ValueError(f"{build} is {kind} ({option}=ON): it is never shipped")
    if options and options.get("GE_VIDEO", "OFF").upper() not in TRUE:
        raise ValueError(f"{build} is a build without video (GE_VIDEO=OFF): it is never shipped")
    target = TARGETS[arch]
    header = (build / "IronSights").read_bytes()[:20]
    if header[:4] != b"\x7fELF" or header[4] != target.elf_class or header[18:20] != target.elf_machine.to_bytes(2, "little"):
        raise ValueError(f"{build / 'IronSights'} is not an {arch} executable")
    # CMake names the architecture in build-info.json and the compiler's macros name it in the program;
    # the two must agree with the target.
    info = build / "generated/build-info.json"
    named = json.loads(info.read_text()).get("architecture") if info.is_file() else None
    if named != target.suffix:
        raise ValueError(f"{info} names architecture {named!r}, not {arch}")


def refuse_comparison_build(build):
    """Raise when `build` was configured with -DGE_COMPARE=ON: a comparison build is never shipped."""
    cache = build / "CMakeCache.txt"
    if not cache.is_file():
        return
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        name, _, value = line.partition("=")
        if name.split(":")[0] == "GE_COMPARE" and value.strip().upper() in ("ON", "TRUE", "YES", "Y", "1"):
            raise ValueError(f"{build} is a comparison build (GE_COMPARE=ON, LP64-006): it is never shipped")


def package(build, output, arch="i686"):
    refuse_unshippable_build(build, arch)
    target, names = TARGETS[arch], output_names(TARGETS[arch])
    output.mkdir(parents=True, exist_ok=True)
    appdir = output / names["appdir"]
    if appdir.exists():
        raise ValueError("Use a fresh packaging directory; refusing to reuse an AppDir")
    for name in PLAYER_FILES:
        copy(build / name, appdir / name)
    for source, name in (("LICENSE", "LICENSE.txt"), ("port/include/icon.png", "iron-sights.png"),
                         ("scripts/port/crash_reporting.md", "Crash-Reporting.md"),
                         (target.start_here, "START-HERE.txt"),
                         ("scripts/port/linux/AppRun", "AppRun"),
                         ("scripts/port/linux/iron-sights.desktop", "iron-sights.desktop"),
                         ("scripts/port/release/DATA-README.txt", "Data/README.txt"),
                         ("port/fast3d/third_party/save-backup-LICENSE.txt", "Save-Transfer-LICENSE.txt"),
                         ("port/fast3d/open_assets/LICENSE", "Open-Assets-LICENSE.txt")):
        copy(ROOT / source, appdir / name)
    copy(build / "generated/build-info.json", appdir / "build-info.json")
    paths = bundle_libraries(build, appdir, target)
    for name in ("IronSights", "sentry-crash"):
        subprocess.run(["strip", "--strip-debug", str(appdir / name)], check=True)
        subprocess.run(["objcopy", "--remove-section", ".gnu_debuglink", "--add-gnu-debuglink=" + str(build / "symbols" / (name + ".debug")), str(appdir / name)], check=True)
    subprocess.run(["objcopy", "--remove-section", ".gnu_debuglink", "--add-gnu-debuglink=" + str(build / "symbols/libsentry.so.debug"), str(appdir / "lib/libsentry.so")], check=True)
    notice = download(f"https://raw.githubusercontent.com/AppImage/type2-runtime/{RUNTIME_SOURCE}/LICENSE",
                      RUNTIME_LICENSE_SHA256, build / "downloads/AppImage-LICENSE.txt")
    copy(notice, appdir / "AppImage-LICENSE.txt")
    reporting = json.loads((appdir / "sentry-build.json").read_text())
    reporting["sha256"] = {name: hashlib.sha256((appdir / name).read_bytes()).hexdigest()
                           for name in reporting["sha256"]}
    (appdir / "sentry-build.json").write_text(json.dumps(reporting, indent=2) + "\n")
    for name in ("IronSights", "sentry-crash", "AppRun"):
        (appdir / name).chmod(0o755)
    # Include distro copyright notices for bundled transitive dependencies.
    for path in Path("/usr/share/doc").glob("*/copyright"):
        copy(path, appdir / "licenses" / path.parent.name / "copyright")
    packages = subprocess.check_output(["dpkg-query", "-W", "-f=${binary:Package}\t${Version}\n"], text=True)
    info = dict(baseline=target.baseline, architecture=target.suffix,
                baseline_dockerfile_sha256=hashlib.sha256((ROOT / target.dockerfile).read_bytes()).hexdigest(),
                packages=packages.splitlines(),
                bundled_libraries={name: hashlib.sha256((appdir / "lib" / name).read_bytes()).hexdigest() for name in paths},
                appimage_runtime_sha256=target.runtime_sha256,
                appimage_runtime_source=RUNTIME_SOURCE,
                play_harness=has_symbol(build / "IronSights", "portPlayPlayerTick"),
                reporting=json.loads((build / "sentry-build.json").read_text()))
    (appdir / "build-inputs.json").write_text(json.dumps(info, indent=2) + "\n")
    checksums(appdir)
    archive = output / names["tarball"]
    with tarfile.open(archive, "w:gz") as stream:
        stream.add(appdir, arcname=names["folder"])
    steamdeck = package_steamdeck(appdir, output, target)
    runtime = download(target.runtime_url, target.runtime_sha256, build / "downloads" / names["runtime"])
    squashfs = output / "payload.squashfs"
    subprocess.run(["mksquashfs", str(appdir), str(squashfs), "-root-owned", "-noappend", "-comp", "gzip", "-processors", "2"], check=True)
    appimage = output / names["appimage"]
    with appimage.open("wb") as stream:
        stream.write(runtime.read_bytes())
        with squashfs.open("rb") as payload:
            shutil.copyfileobj(payload, stream)
    appimage.chmod(0o755)
    squashfs.unlink()
    copy(appdir / "build-info.json", output / names["build_info"])
    copy(appdir / "build-inputs.json", output / names["build_inputs"])
    (output / names["sums"]).write_text("".join(
        f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in (archive, appimage, steamdeck)))
    print(archive)
    print(appimage)
    print(steamdeck)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=sorted(TARGETS), default="i686")
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    package(args.build.resolve(), args.output.resolve(), args.arch)
