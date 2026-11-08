#!/usr/bin/env python3
"""Growth soak audit (Windows): play every solo stage several times in one
process and report memory and renderer resources lap over lap.

    python scripts/port/soak.py --exe build_win/IronSights.exe --rom baserom.u.z64 \
        [--stages all] [--frames 600] [--laps 4] [--texture-pack <zip>] [--out build_win/soak]

The game's GE_SOAK harness (port/src/port_soak.c) logs "ge: memstats:" at each
stage's end and in the menus after it. The first lap is warm-up (caches fill,
shaders compile); after it, a counter that keeps rising from lap to lap at the
same point is growth worth a look. The run uses an isolated copy of the
executable and settings; the logs stay under --out.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LINE = re.compile(r"ge: memstats: (\w+) lap=(\d+) stage=(-?\d+) private=(\d+)KB resident=(\d+)KB heap=(\d+)KB "
                  r"texcache=(\d+) freeids=(\d+) gltex=(\d+) fb=(\d+) programs=(\d+) hd=(\d+)KB")
FIELDS = ("private", "resident", "heap", "texcache", "freeids", "gltex", "fb", "programs", "hd")


def parse(text):
    samples = []
    for m in LINE.finditer(text):
        sample = {"where": m.group(1), "lap": int(m.group(2)), "stage": int(m.group(3))}
        sample.update({name: int(value) for name, value in zip(FIELDS, m.groups()[3:])})
        samples.append(sample)
    return samples


def growth(samples, laps):
    """Per counter: the change from lap 1 (after warm-up) to the last lap, at
    each comparable point, averaged over points; plus the worst point."""
    points = {}
    for s in samples:
        key = (s["where"], s["stage"])
        points.setdefault(key, {})[s["lap"]] = s
    report = {}
    for field in FIELDS:
        deltas = []
        for key, by_lap in points.items():
            if 1 in by_lap and laps - 1 in by_lap and laps - 1 > 1:
                deltas.append((by_lap[laps - 1][field] - by_lap[1][field], key))
        if deltas:
            mean = sum(d for d, _ in deltas) / len(deltas)
            worst = max(deltas)
            report[field] = {"mean_per_point": round(mean, 1), "per_lap": round(mean / (laps - 2), 1),
                             "worst": worst[0], "worst_at": "%s stage %d" % worst[1]}
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", default=str(ROOT / "build_win/IronSights.exe"))
    parser.add_argument("--rom", default=str(ROOT / "baserom.u.z64"))
    parser.add_argument("--stages", default="all")
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--laps", type=int, default=4)
    parser.add_argument("--texture-pack", help="a pack ZIP to hard-link into the run's Data/TexturePacks")
    parser.add_argument("--ini", action="append", default=[], help="extra [display] line, e.g. msaa=4")
    parser.add_argument("--window", default="1280x720")
    parser.add_argument("--out", default=str(ROOT / "build_win/soak"))
    parser.add_argument("--analyze", help="report on an existing log (the browser soak's, say) instead of running")
    options = parser.parse_args()
    if options.analyze:
        samples = parse(Path(options.analyze).read_text(errors="replace"))
        laps = max((s["lap"] for s in samples), default=0) + 1
        print(json.dumps({"samples": len(samples), "laps": laps, "growth_after_warmup": growth(samples, laps)}, indent=1))
        return 0

    out = Path(options.out)
    work = out / "run"
    shutil.rmtree(work, ignore_errors=True)
    (work / "Data/TexturePacks").mkdir(parents=True)
    exe = work / Path(options.exe).name
    shutil.copy2(options.exe, exe)
    for dll in Path(options.exe).parent.glob("*.dll"):
        shutil.copy2(dll, work / dll.name)
    if options.texture_pack:
        os.link(options.texture_pack, work / "Data/TexturePacks" / Path(options.texture_pack).name)
    nl = chr(10)
    (work / "midas.ini").write_text(nl.join(["[display]", "fullscreen=window"] + options.ini) + nl)
    env = {k: v for k, v in os.environ.items() if not k.startswith("GE_")}
    env.update(GE_ROM=str(Path(options.rom).resolve()), GE_WINDOW=options.window, GE_NOINPUT="1",
               GE_DRIVE="1", GE_CHEATS="invuln", GE_SEED="1", GE_NOINISAVE="1",
               GE_SOAK="%s:%d:%d" % (options.stages, options.frames, options.laps))
    started = time.time()
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    try:
        with (out / "soak.log").open("wb") as log:
            code = subprocess.run([str(exe)], cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT,
                                  creationflags=flags, timeout=4 * 3600).returncode
    finally:
        exe.unlink(missing_ok=True)
    text = (out / "soak.log").read_text(errors="replace")
    samples = parse(text)
    report = {"exit": code, "seconds": round(time.time() - started), "complete": "ge: soak: complete" in text,
              "samples": len(samples), "growth_after_warmup": growth(samples, options.laps)}
    (out / "samples.json").write_text(json.dumps(samples, indent=1))
    (out / "report.json").write_text(json.dumps(report, indent=1))
    print(json.dumps(report, indent=1))
    return 0 if report["complete"] and code == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
