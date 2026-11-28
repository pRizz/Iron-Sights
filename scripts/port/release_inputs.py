"""Record inputs without publishing a private ROM URL or any game assets."""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess


def command(*args):
    return subprocess.check_output(args, text=True).strip()


# Each Windows target's compiler prefix and build folder (WIN64-004).
ARCHITECTURES = {"x86": ("i686-w64-mingw32", "build_win"), "x64": ("x86_64-w64-mingw32", "build_win64")}


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--arch",choices=sorted(ARCHITECTURES),default="x86")
    args=parser.parse_args()
    triple, build = ARCHITECTURES[args.arch]
    info=dict(schema=1, runner_image=os.environ.get("ImageOS",platform.platform()),
              runner_version=os.environ.get("ImageVersion","unknown"),
              architecture=args.arch,
              compiler=command(triple+"-gcc","--version"),
              compiler_cpp=command(triple+"-g++","--version"),
              cmake=command("cmake","--version"), python=platform.python_version(),
              rom="No ROM used to compile or package; supplied by the player at runtime",
              sdl2_version=os.environ["SDL2_VERSION"],sdl2_sha256=os.environ["SDL2_SHA256"],
              workflow=".github/workflows/windows-release.yml",
              generated_icons="Prepared on the player's Windows PC; not embedded in the release")
    info["sentry"] = json.loads(Path(build, "sentry-build.json").read_text())
    assets = json.loads(Path("port/fast3d/open_assets/manifest.json").read_text())
    info["open_assets"] = dict(repository=assets["repository"], tag=assets["tag"], license=assets["license"],
                               archives=assets["archives"], files=assets["files"])
    dependencies = os.environ.get("MIDAS_DEPENDENCIES_FILE")
    if dependencies:
        info["dependency_packages"] = json.loads(Path(dependencies).read_text())
    info["compiler_cache"] = dict(
        enabled=os.environ.get("MIDAS_CLEAN_BUILD") != "true",
        version=command("ccache", "--version"),
        compiler_check="content",
        statistics=Path(build, "ccache-stats.txt").read_text())
    args.output.write_text(json.dumps(info,indent=2)+"\n",encoding="utf-8")
