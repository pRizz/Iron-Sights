"""Create an allow-listed Windows player package; never sweep build directories.

--arch x86 (the default) packages the 32-bit exe as Iron-Sights-windows-x86.zip;
--arch x64 the 64-bit one as Iron-Sights-windows-x64.zip (WIN64-004). Every
.exe and .dll packed must be the target's PE machine, and its Sentry runtime
that architecture."""
import argparse
from collections import namedtuple
import hashlib
import json
from pathlib import Path
import zipfile

import package_identity
from pe_machine import AMD64, I386, pe_machine

ROOT = Path(__file__).resolve().parents[2]
# A target's zip, the checksum and build-info files written beside it (the x64 ones carry their own
# names so both targets' files fit in one release), its PE machine and its Sentry runtime's
# architecture, and its package.txt flavor (UPD-001, package_identity.py).
Target = namedtuple("Target", "zip sums build_info machine sentry_architecture flavor")
TARGETS = {
    "x86": Target("Iron-Sights-windows-x86.zip", "SHA256SUMS.txt", "build-info.json", I386, "x86",
                  "windows-x86-zip"),
    "x64": Target("Iron-Sights-windows-x64.zip", "SHA256SUMS-windows-x64.txt", "windows-x64-build-info.json",
                  AMD64, "x64", "windows-x64-zip"),
}
NAME = TARGETS["x86"].zip
FILES = {"IronSights.exe": "IronSights.exe", "SDL2.dll": "SDL2.dll",
         "sentry.dll": "sentry.dll", "sentry-crash.exe": "sentry-crash.exe", "sentry-wer.dll": "sentry-wer.dll",
         "Sentry-LICENSE.txt": "Sentry-LICENSE.txt", "sentry-build.json": "sentry-build.json",
         "gamecontrollerdb.txt": "gamecontrollerdb.txt", "gamecontrollerdefault.txt": "gamecontrollerdefault.txt",
         "midas_ini_help.txt": "midas_ini_help.txt", "Data/shaders_base.txt": "Data/shaders_base.txt"}


def refuse_comparison_build(build):
    """Raise when `build` was configured with -DGE_COMPARE=ON: a comparison build is never shipped."""
    cache = build/"CMakeCache.txt"
    if not cache.is_file():
        return
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        name, _, value = line.partition("=")
        if name.split(":")[0] == "GE_COMPARE" and value.strip().upper() in ("ON", "TRUE", "YES", "Y", "1"):
            raise ValueError(f"{build} is a comparison build (GE_COMPARE=ON, LP64-006): it is never shipped")


def refuse_wrong_machine(build, target):
    """Raise when a packed .exe or .dll is not TARGET's PE machine, or the Sentry runtime is not
    TARGET's architecture: an x64 zip must not carry a 32-bit DLL, nor the reverse."""
    for source in FILES:
        if source.endswith((".exe", ".dll")) and pe_machine(build/source) != target.machine:
            raise ValueError(f"{build/source} is not a {target.sentry_architecture} PE file")
    info = json.loads((build/"sentry-build.json").read_text(encoding="utf-8"))
    if info.get("architecture") != target.sentry_architecture:
        raise ValueError(f"{build/'sentry-build.json'} is a {info.get('architecture')} Sentry runtime, "
                         f"not {target.sentry_architecture}")


def package(build, output, root=ROOT, sdl_license=None, arch="x86"):
    refuse_comparison_build(build)
    target = TARGETS[arch]
    refuse_wrong_machine(build, target)
    info = json.loads((build/"generated/build-info.json").read_text(encoding="utf-8"))
    output.mkdir(parents=True, exist_ok=True)
    inputs = {dest: (build/source).read_bytes() for source, dest in FILES.items()}
    inputs.update({"LICENSE.txt": (root/"LICENSE").read_bytes(),
                   "START-HERE.txt": (root/"scripts/port/release/START-HERE.txt").read_bytes(),
                   "Data/README.txt": (root/"scripts/port/release/DATA-README.txt").read_bytes(),
                   "VERIFY.txt": (root/"scripts/port/release/VERIFY.txt").read_bytes(),
                   "build-info.json": (json.dumps(info,indent=2)+"\n").encode(),
                   package_identity.NAME: package_identity.text(target.flavor)})
    if sdl_license is None:
        raise ValueError("The SDL2 license is required")
    inputs["SDL2-LICENSE.txt"] = sdl_license.read_bytes()
    inputs["Save-Transfer-LICENSE.txt"] = (root/"port/fast3d/third_party/save-backup-LICENSE.txt").read_bytes()
    inputs["Open-Assets-LICENSE.txt"] = (root/"port/fast3d/open_assets/LICENSE").read_bytes()
    inputs[package_identity.SUMS] = package_identity.sums(inputs).encode("ascii")
    path = output/target.zip
    with zipfile.ZipFile(path,"w",compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for name, data in sorted(inputs.items()):
            entry = zipfile.ZipInfo(name,(2026,1,1,0,0,0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            archive.writestr(entry,data)
    (output/target.sums).write_text(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {target.zip}\n",
                                    encoding="ascii")
    (output/target.build_info).write_text(json.dumps(info,indent=2)+"\n",encoding="utf-8")
    print(path)
    return path


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--sdl-license",type=Path,required=True)
    parser.add_argument("--arch",choices=sorted(TARGETS),default="x86")
    args=parser.parse_args()
    package(args.build,args.output,sdl_license=args.sdl_license,arch=args.arch)
