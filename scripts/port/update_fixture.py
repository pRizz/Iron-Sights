"""Fake releases and installs for the in-game updater's tests (FORK-095, UPD-002).

`python3 update_fixture.py DIR` writes, under DIR, release folders laid out as
the release workflow publishes them (packages, SHA256SUMS files, build-info
files) and install folders as the packagers lay a package out, built with
package_identity.py so they match the real packages' identity files.
test_update.c reads them through a fake transport that maps
https://example.test/RELEASE/releases/... to DIR/RELEASE/....

`python3 update_fixture.py serve DIR PORT` serves DIR on 127.0.0.1 for an
end-to-end run of a GE_UPDATE_TEST build (see self_update_plan.md).
"""
import argparse
import functools
import hashlib
import http.server
import io
import json
from pathlib import Path
import sys
import tarfile
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import package_identity  # noqa: E402

INSTALLED_BUILD = 100
NEW_BUILD = 200
TAG = "build-200.1-0123456789ab"
TAR_FOLDER = "Iron-Sights-linux-x86_64"
# Past tar's 100-byte name field, as a bundled library's licence path can be: a pax header.
LONG_PATH = "licenses/" + "a-long-bundled-package-name-" * 5 + "/copyright"


def files_of(version, flavor, extra=None):
    """A package's files: the program files of VERSION, package.txt and SHA256SUMS.txt."""
    files = {"IronSights.exe": f"{version} exe".encode(), "SDL2.dll": f"{version} dll".encode(),
             "Data/README.txt": f"{version} readme".encode(), "Data/shaders_base.txt": f"{version} shaders".encode(),
             "gamecontrollerdefault.txt": f"{version} defaults".encode(),
             package_identity.NAME: package_identity.text(flavor)}
    files.update(extra or {})
    files[package_identity.SUMS] = package_identity.sums(files).encode()
    return files


def zip_bytes(files, folder=""):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo((folder + "/" if folder else "") + name, (2026, 1, 1, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = (0o100755 if name == "IronSights.exe" else 0o100644) << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)
    return stream.getvalue()


def tar_bytes(files, folder):
    stream = io.BytesIO()
    with tarfile.open(fileobj=stream, mode="w:gz") as archive:
        top = tarfile.TarInfo(folder)
        top.type, top.mode = tarfile.DIRTYPE, 0o755
        archive.addfile(top)
        for name, data in sorted(files.items()):
            entry = tarfile.TarInfo(folder + "/" + name)
            entry.size, entry.mode = len(data), 0o755 if name == "IronSights.exe" else 0o644
            archive.addfile(entry, io.BytesIO(data))
    return stream.getvalue()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def release(root, name, build=NEW_BUILD, ci=True, windows=None, tarball=None, appimage=b"new appimage"):
    """One release folder: the Windows x86 zip, the x86_64 tarball and AppImage, their sums and build info."""
    folder = root / name
    windows = windows if windows is not None else zip_bytes(files_of("new", "windows-x86-zip",
                                                                     {"new-only.txt": b"added"}))
    tarball = tarball if tarball is not None else tar_bytes(files_of("new", "linux-x86_64-tar",
                                                                     {"lib/libnew.so": b"new library",
                                                                                LONG_PATH: b"licence"}), TAR_FOLDER)
    write(folder / "Iron-Sights-windows-x86.zip", windows)
    write(folder / "Iron-Sights-linux-x86_64.tar.gz", tarball)
    write(folder / "Iron-Sights-linux-x86_64.AppImage", appimage)
    write(folder / "SHA256SUMS.txt", f"{sha(windows)}  Iron-Sights-windows-x86.zip\n".encode())
    write(folder / "SHA256SUMS-linux-x86_64.txt", (f"{sha(tarball)}  Iron-Sights-linux-x86_64.tar.gz\n"
                                                   f"{sha(appimage)}  Iron-Sights-linux-x86_64.AppImage\n").encode())
    info = dict(schema=1, build_number=build, ci=ci, dirty=False,
                release_url=f"https://example.test/{name}/releases/tag/{TAG}")
    for build_info in ("build-info.json", "linux-x86_64-build-info.json"):
        write(folder / build_info, (json.dumps(info, indent=2) + "\n").encode())


def install(root, name, flavor):
    """One install folder of the old build, with player data and an edited controller file."""
    folder = root / name
    extra = {"old-only.txt": b"obsolete"} if flavor != "linux-x86_64-appimage" else {}
    for path, data in files_of("old", flavor, extra).items():
        write(folder / path, data)
    (folder / "gamecontrollerdefault.txt").write_bytes(b"edited by the player")
    write(folder / "midas.ini", b"[updates]\nmode=automatic\n")
    write(folder / "Data/Saves/eeprom.bin", b"player save")
    if flavor == "linux-x86_64-appimage":
        write(folder / "Iron-Sights.AppImage", b"old appimage")


def make(root):
    root = Path(root)
    release(root, "good")
    release(root, "notci", ci=False)
    release(root, "older", build=INSTALLED_BUILD - 1)
    bad = files_of("new", "windows-x86-zip")
    bad["IronSights.exe"] = b"tampered after its sums were made"
    release(root, "badinner", windows=zip_bytes(bad))
    unsafe = files_of("new", "windows-x86-zip", {"../evil.txt": b"outside"})
    release(root, "unsafe", windows=zip_bytes(unsafe))
    release(root, "wrongflavor", windows=zip_bytes(files_of("new", "windows-x64-zip")))
    saves = files_of("new", "windows-x86-zip", {"Data/Saves/eeprom.bin": b"a package must not replace saves"})
    release(root, "saves", windows=zip_bytes(saves))
    for name in ("zip-install", "zip-rollback", "zip-failmid", "zip-throttle", "zip-bad", "zip-interrupted"):
        install(root, name, "windows-x86-zip")
    install(root, "tar-install", "linux-x86_64-tar")
    install(root, "appimage-install", "linux-x86_64-appimage")
    write(root / "flavors.txt", "".join(f"{name} {asset} {sums} {info}\n" for name, (asset, sums, info)
                                        in sorted(package_identity.FLAVORS.items())).encode())


def serve(root, port):
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(root))
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), handler) as server:
        server.serve_forever()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("args", nargs="+", help="DIR, or serve DIR PORT")
    args = parser.parse_args().args
    if args[0] == "serve":
        serve(Path(args[1]), int(args[2]))
    else:
        make(args[0])
