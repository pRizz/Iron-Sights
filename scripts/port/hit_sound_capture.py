#!/usr/bin/env python3
"""Measure the scripted hits in a GE_HITSOUND_PROBE capture (FORK-043).

Each probe hit logs its distance, the Hit Sounds setting and the capture frame
it started at. The nearest hit with the setting off is the point-blank
reference: with Hit Sounds on every hit should match it, and with it off the
farther hits should still fade.
"""
from __future__ import annotations

import argparse
import array
import math
import re
import sys
import wave
from dataclasses import dataclass
from pathlib import Path

RATE = 22050
WINDOW = int(0.15 * RATE)  # the flesh impact is over within 160 ms
TOLERANCE = 0.1  # a raised hit lands within 10% of the reference peak
FADED = 0.9  # an unraised far hit stays below 90% of it
EVENT = re.compile(r"ge: hitprobe hit=\d+ metres=([\d.]+) hit_sounds=([01]) native=(\d+) frame=(\d+)")


@dataclass(frozen=True)
class Hit:
    metres: float
    enabled: bool
    native: int
    frame: int


@dataclass(frozen=True)
class Level:
    hit: Hit
    peak: float


def parse_hits(log: str) -> list[Hit]:
    return [Hit(float(m), e == "1", int(n), int(f)) for m, e, n, f in EVENT.findall(log)]


def stereo_peak(left: array.array, right: array.array, start: int) -> float:
    """Peak combined level; the equal-power pan law keeps it the same wherever a
    hit is panned, so a near hit beside the camera still compares fairly."""
    end = min(len(left), start + WINDOW)
    return max((math.hypot(left[i], right[i]) for i in range(start, end)), default=0.0)


def evaluate(levels: list[Level]) -> list[str]:
    """Return the failures; an empty list means the levels are as intended."""
    off = [level for level in levels if not level.hit.enabled]
    if not off or not any(level.hit.enabled for level in levels):
        return ["the capture needs hits with Hit Sounds both off and on"]
    reference = min(off, key=lambda level: level.hit.metres)
    if reference.peak <= 0:
        return ["the point-blank reference hit is silent"]
    failures = []
    for level in levels:
        ratio = level.peak / reference.peak
        name = f"{level.hit.metres:g} m with Hit Sounds {'on' if level.hit.enabled else 'off'}"
        if level.hit.enabled and abs(ratio - 1) > TOLERANCE:
            failures.append(f"{name} peaks at {ratio:.0%} of point blank")
        if not level.hit.enabled and level.hit.native < reference.hit.native and ratio >= FADED:
            failures.append(f"{name} did not fade ({ratio:.0%} of point blank)")
    return failures


def decibels(peak: float, reference: float) -> str:
    return "silent" if peak <= 0 else f"{20 * math.log10(peak / reference):+.1f} dB"


def read_capture(path: Path) -> tuple[array.array, array.array]:
    with wave.open(str(path)) as capture:
        if capture.getframerate() != RATE or capture.getnchannels() != 2:
            raise SystemExit(f"{path}: expected {RATE} Hz stereo")
        samples = array.array("h")
        samples.frombytes(capture.readframes(capture.getnframes()))
    return samples[0::2], samples[1::2]


def write_comparison(path: Path, left: array.array, right: array.array, hits: list[Hit]) -> None:
    """Each distance off then on, a quarter second apart, for listening."""
    gap = array.array("h", bytes(int(0.25 * RATE) * 4))
    out = array.array("h")
    for hit in hits:
        clip = range(hit.frame, min(len(left), hit.frame + int(0.3 * RATE)))
        for i in clip:
            out.extend((left[i], right[i]))
        out.extend(gap)
    with wave.open(str(path), "wb") as comparison:
        comparison.setnchannels(2)
        comparison.setsampwidth(2)
        comparison.setframerate(RATE)
        comparison.writeframes(out.tobytes())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("capture", type=Path)
    parser.add_argument("log", type=Path)
    parser.add_argument("--clips", type=Path, help="write hits_compare.wav here")
    args = parser.parse_args()
    hits = parse_hits(args.log.read_text(encoding="utf-8", errors="replace"))
    left, right = read_capture(args.capture)
    levels = [Level(hit, stereo_peak(left, right, hit.frame)) for hit in hits]
    failures = evaluate(levels)
    reference = min((l for l in levels if not l.hit.enabled), key=lambda l: l.hit.metres, default=None)
    for level in levels:
        relative = decibels(level.peak, reference.peak) if reference and reference.peak > 0 else "-"
        print(f"{level.hit.metres:5g} m  Hit Sounds {'on ' if level.hit.enabled else 'off'}"
              f"  native {level.hit.native:5d}  peak {level.peak:7.0f}  {relative} vs point blank")
    if args.clips:
        write_comparison(args.clips / "hits_compare.wav", left, right, hits)
        print(f"comparison: {args.clips / 'hits_compare.wav'}")
    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
