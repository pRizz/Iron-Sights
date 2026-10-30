"""Allow-listed Linux tarball and type-2 AppImage; no player data is swept in."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile

from linux_build import download
from texture_pack_distribution import hd_provenance, texture_files

ROOT = Path(__file__).resolve().parents[2]
RUNTIME_URL = "https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-i686"
RUNTIME_SHA256 = "e72ea0b140a0a16e680713238a6f30aad278b62c4ca17919c554864124515498"
RUNTIME_SOURCE = "dd6cebedcbddde9c82f89b011e8e1d40b6e43868"
RUNTIME_LICENSE_SHA256 = "aa154fc9070614bbe7921f89db11efd1dba7a1f3a41685958110e2230f9c0ca1"
SYSTEM_LIBRARIES = re.compile(r"^(ld-linux|lib(c|m|dl|pthread|rt|resolv|util)\.so|libnss_|lib(GL|EGL|GLX|GLdispatch|OpenGL)\.)")
PLAYER_FILES = ("IronSights", "sentry-crash", "Sentry-LICENSE.txt", "sentry-build.json", "SDL2-LICENSE.txt",
                "gamecontrollerdb.txt", "gamecontrollerdefault.txt", "midas_ini_help.txt", "Data/shaders_base.txt")


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def bundle_libraries(build, destination):
    roots = [build / "IronSights", build / "sentry-crash", build / "lib/libsentry.so",
             build / "lib/libSDL2-2.0.so.0", Path("/usr/lib/i386-linux-gnu/libcurl.so.4"),
             Path("/usr/lib/i386-linux-gnu/libasound.so.2"), Path("/usr/lib/i386-linux-gnu/libpulse.so.0")]
    paths = {p.name: p for p in roots[2:]}
    for root in roots:
        result = subprocess.check_output(["ldd", str(root)], text=True)
        if "not found" in result:
            raise ValueError("Unresolved runtime dependency: " + result)
        for line in result.splitlines():
            match = re.match(r"\s*(\S+) => (/\S+)", line)
            if match and not SYSTEM_LIBRARIES.match(match[1]):
                paths[match[1]] = Path(match[2])
    for name, path in paths.items():
        data = path.read_bytes()
        if data[:5] != b"\x7fELF\x01":
            raise ValueError("Expected an ELF32 runtime: " + str(path))
        target = destination / "lib" / name
        copy(path, target)
        subprocess.run(["strip", "--strip-debug", str(target)], check=True)
        subprocess.run(["patchelf", "--set-rpath", "$ORIGIN", str(target)], check=True)
    return paths


def checksums(directory):
    files = sorted(p for p in directory.rglob("*") if p.is_file() and p.name != "SHA256SUMS.txt")
    (directory / "SHA256SUMS.txt").write_text("".join(
        f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(directory).as_posix()}\n" for p in files))


def package(build, output):
    textures = build / "Data/TexturePacks"
    artwork = texture_files(textures)
    output.mkdir(parents=True, exist_ok=True)
    appdir = output / "Iron-Sights-linux-i686.AppDir"
    if appdir.exists():
        raise ValueError("Use a fresh packaging directory; refusing to reuse an AppDir")
    for name in PLAYER_FILES:
        copy(build / name, appdir / name)
    for name, path in artwork.items():
        copy(path, appdir / "Data/TexturePacks" / name)
    for source, name in (("LICENSE", "LICENSE.txt"), ("port/include/icon.png", "iron-sights.png"),
                         ("scripts/port/crash_reporting.md", "Crash-Reporting.md"),
                         ("scripts/port/linux/START-HERE.txt", "START-HERE.txt"),
                         ("scripts/port/linux/AppRun", "AppRun"),
                         ("scripts/port/linux/iron-sights.desktop", "iron-sights.desktop"),
                         ("scripts/port/release/DATA-README.txt", "Data/README.txt"),
                         ("port/fast3d/fonts/LICENSE.txt", "Enhanced-Fonts-LICENSE.txt"),
                         ("port/fast3d/fonts/NOTICE.txt", "Enhanced-Fonts-NOTICE.txt")):
        copy(ROOT / source, appdir / name)
    copy(build / "generated/build-info.json", appdir / "build-info.json")
    paths = bundle_libraries(build, appdir)
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
    info = dict(baseline="Ubuntu 22.04 / glibc 2.35", architecture="i686",
                baseline_dockerfile_sha256=hashlib.sha256((ROOT / "scripts/port/linux-release.Dockerfile").read_bytes()).hexdigest(),
                packages=packages.splitlines(),
                bundled_libraries={name: hashlib.sha256((appdir / "lib" / name).read_bytes()).hexdigest() for name in paths},
                appimage_runtime_sha256=RUNTIME_SHA256,
                appimage_runtime_source=RUNTIME_SOURCE,
                hd_textures=hd_provenance(textures),
                reporting=json.loads((build / "sentry-build.json").read_text()))
    (appdir / "build-inputs.json").write_text(json.dumps(info, indent=2) + "\n")
    checksums(appdir)
    archive = output / "Iron-Sights-linux-i686.tar.gz"
    with tarfile.open(archive, "w:gz") as stream:
        stream.add(appdir, arcname="Iron-Sights-linux-i686")
    runtime = download(RUNTIME_URL, RUNTIME_SHA256, build / "downloads/runtime-i686")
    squashfs = output / "payload.squashfs"
    subprocess.run(["mksquashfs", str(appdir), str(squashfs), "-root-owned", "-noappend", "-comp", "gzip", "-processors", "2"], check=True)
    appimage = output / "Iron-Sights-linux-i686.AppImage"
    with appimage.open("wb") as target:
        target.write(runtime.read_bytes())
        with squashfs.open("rb") as payload:
            shutil.copyfileobj(payload, target)
    appimage.chmod(0o755)
    squashfs.unlink()
    copy(appdir / "build-info.json", output / "linux-build-info.json")
    copy(appdir / "build-inputs.json", output / "linux-build-inputs.json")
    (output / "SHA256SUMS-linux.txt").write_text("".join(
        f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in (archive, appimage)))
    print(archive)
    print(appimage)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    package(args.build.resolve(), args.output.resolve())
