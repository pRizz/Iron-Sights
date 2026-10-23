"""Create an allow-listed Windows player package; never sweep build directories."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[2]
NAME = "Project-Midas-ADS-windows-x86.zip"
FILES = {"ProjectMidas.exe": "ProjectMidas.exe", "SDL2.dll": "SDL2.dll",
         "gamecontrollerdb.txt": "gamecontrollerdb.txt", "gamecontrollerdefault.txt": "gamecontrollerdefault.txt",
         "midas_ini_help.txt": "midas_ini_help.txt", "Data/shaders_base.txt": "Data/shaders_base.txt"}


def package(build, output, root=ROOT, sdl_license=None):
    info = json.loads((build/"generated/build-info.json").read_text(encoding="utf-8"))
    output.mkdir(parents=True, exist_ok=True)
    inputs = {dest: (build/source).read_bytes() for source, dest in FILES.items()}
    inputs.update({"LICENSE.txt": (root/"LICENSE").read_bytes(),
                   "START-HERE.txt": (root/"scripts/port/release/START-HERE.txt").read_bytes(),
                   "Data/README.txt": (root/"scripts/port/release/DATA-README.txt").read_bytes(),
                   "VERIFY.txt": (root/"scripts/port/release/VERIFY.txt").read_bytes(),
                   "build-info.json": (json.dumps(info,indent=2)+"\n").encode()})
    if sdl_license is None:
        raise ValueError("The SDL2 license is required")
    inputs["SDL2-LICENSE.txt"] = sdl_license.read_bytes()
    hashes = "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n" for name, data in sorted(inputs.items()))
    inputs["SHA256SUMS.txt"] = hashes.encode("ascii")
    path = output/NAME
    with zipfile.ZipFile(path,"w",compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for name, data in sorted(inputs.items()):
            entry = zipfile.ZipInfo(name,(2026,1,1,0,0,0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            archive.writestr(entry,data)
    (output/"SHA256SUMS.txt").write_text(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {NAME}\n",encoding="ascii")
    (output/"build-info.json").write_text(json.dumps(info,indent=2)+"\n",encoding="utf-8")
    print(path)
    return path


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--sdl-license",type=Path,required=True)
    args=parser.parse_args()
    package(args.build,args.output,sdl_license=args.sdl_license)
