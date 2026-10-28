"""Record inputs without publishing a private ROM URL or any game assets."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


def command(*args):
    return subprocess.check_output(args, text=True).strip()


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    info=dict(schema=1, runner_image=os.environ.get("ImageOS",platform.platform()),
              runner_version=os.environ.get("ImageVersion","unknown"),
              compiler=command("i686-w64-mingw32-gcc","--version"),
              compiler_cpp=command("i686-w64-mingw32-g++","--version"),
              cmake=command("cmake","--version"), python=platform.python_version(),
              rom="No ROM used to compile or package; supplied by the player at runtime",
              sdl2_version=os.environ["SDL2_VERSION"],sdl2_sha256=os.environ["SDL2_SHA256"],
              workflow=".github/workflows/windows-release.yml",
              generated_icons="Prepared on the player's Windows PC; not embedded in the release")
    fonts = Path("port/fast3d/fonts")
    info["sentry"] = json.loads(Path("build_win/sentry-build.json").read_text())
    source = json.loads((fonts/"sources.json").read_text())
    info["enhanced_fonts"] = dict(source=source["source"], commit=source["commit"], license=source["license"],
        masks_sha256=hashlib.sha256((fonts/"gepd_font_data.inc").read_bytes()).hexdigest())
    dependencies = os.environ.get("MIDAS_DEPENDENCIES_FILE")
    if dependencies:
        info["dependency_packages"] = json.loads(Path(dependencies).read_text())
    info["compiler_cache"] = dict(
        enabled=os.environ.get("MIDAS_CLEAN_BUILD") != "true",
        version=command("ccache", "--version"),
        compiler_check="content",
        statistics=Path("build_win/ccache-stats.txt").read_text())
    args.output.write_text(json.dumps(info,indent=2)+"\n",encoding="utf-8")
