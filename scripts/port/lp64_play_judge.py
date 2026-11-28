"""Judge one lp64_play.py run by what its log shows (LP64-005 Task 1).

A case fails on a status other than the one it expects, a timeout, any sanitizer marker,
`ge: fatal signal`, `ASSERTION FAULT`, a failing `ge: lp64:` line (lp64_boot.py's default-deny
rule) and on each line it requires that the log lacks. The markers and the `ge: lp64:` rule are
lp64_boot.py's own, so the two tools cannot drift apart. A run with video (`run --video`) must
also have opened its window and GL context and drawn at least the case's `pictures`, and fails on
a render fault (RENDER_FAULTS): a shader that did not compile or link, a GL function not found.
"""
import re

from lp64_boot import FATAL_SIGNAL, SANITIZER_MARKERS, failing_lp64_lines
from lp64_play_cases import DEMO_START, Case, Ordered, Required
from lp64_play_route import PLAY_EVENT, PLAY_GUN, PLAY_MARK, PLAY_TICK

# port/src/video.c, once SDL has made the window and its GL context: "ge: video: window + GL context up\n"
VIDEO_UP = re.compile(r'ge: video: window \+ GL context up')
# port/src/pcsched.c counts the pictures the renderer was handed in two lines: "ge: frame limit
# reached: frames=%u gfxtasks=%u\n" at the limit, and the heartbeat "ge: frame %u | gfx tasks
# submitted %u\n" once a second, the only count of a case that ends itself (the soak). A case that
# must reach its limit requires the frame-limit line among its own lines.
GFXTASKS = re.compile(r'ge: frame (?:limit reached: frames=\d+ gfxtasks=|\d+ \| gfx tasks submitted )(\d+)')
GFXTASKS_LABEL = 'ge: frame ... gfxtasks=N (the frame-limit line or the heartbeat)'


def pictures_drawn(text: str) -> int | None:
    """The pictures a run with video drew, from the last picture count of its log TEXT; None when
    the log has none."""
    counts = GFXTASKS.findall(text)
    return int(counts[-1]) if counts else None


def video_required(case: Case) -> tuple[Required, ...]:
    """What a run with video must log besides the case's own lines: the GL line, and the pictures
    drawn unless the case asks for none (a stop ends before it has drawn many)."""
    up = Required('ge: video: window + GL context up', VIDEO_UP)
    if case.pictures == 0:
        return (up,)
    return (up, Required(GFXTASKS_LABEL, GFXTASKS, at_least=case.pictures))


# What a video case's log must not hold (MAC-002 Task 4): the renderer's own reports of a shader it
# could not compile or link and of a GL function the context did not give it
# (port/fast3d/gfx_opengl.cpp). The game carries on after each, so nothing else fails the case; the
# aarch64 Linux video logs hold none. A headless build has no renderer and is not judged by these.
RENDER_FAULTS = (re.compile(r'Failed to compile'), re.compile(r'Failed to link'),
                 re.compile(r'GL: could not find function'))


def render_fault(lines: list[str]) -> str | None:
    """The first of LINES that reports a render fault, or None."""
    return next((line for line in lines if any(pattern.search(line) for pattern in RENDER_FAULTS)), None)


# include/assert.h and the game's own asserts: "\n--- ASSERTION FAULT - %s - %s, line %d\n\n".
ASSERTION_FAULT = 'ASSERTION FAULT'


def first_line_with(lines: list[str], markers: tuple[str, ...]) -> str | None:
    return next((line for line in lines if any(marker in line for marker in markers)), None)


def first_playback(lines: list[str]) -> list[str]:
    """The lines of the log's first demo playback: from its first `ge: demo start:` line up to
    its second (all the rest when there is no second); none when no demo started."""
    starts = [index for index, line in enumerate(lines) if DEMO_START.search(line)]
    if not starts:
        return []
    return lines[starts[0]:starts[1]] if len(starts) > 1 else lines[starts[0]:]


def torn_play_lines(lines: list[str]) -> list[str]:
    """The lines that hold `ge: play:` but not a whole play log line: another stream's output cut
    them (the play log writes each line whole, after flushing what stdout holds)."""
    return [line for line in lines
            if PLAY_MARK in line
            and not any(pattern.search(line) for pattern in (PLAY_EVENT, PLAY_TICK, PLAY_GUN))]


def ordered_problem(lines: list[str], required: Ordered) -> str | None:
    """Why LINES do not hold REQUIRED's lines in order, or None when they do."""
    at = 0
    for index, (label, pattern) in enumerate(required.steps):
        found = next((number for number in range(at, len(lines)) if pattern.search(lines[number])), None)
        if found is None:
            if index > 0 and any(pattern.search(line) for line in lines[:at]):
                return f'out of order: {label} only before {required.steps[index - 1][0]}'
            return f'missing line: {label}'
        at = found + 1
    return None


def requirement_problem(lines: list[str], required: Required | Ordered) -> str | None:
    """Why LINES do not satisfy REQUIRED, or None when they do."""
    if isinstance(required, Ordered):
        return ordered_problem(lines, required)
    if required.first_playback:
        lines = first_playback(lines)
    matches = [match for match in map(required.pattern.search, lines) if match is not None]
    if len(matches) < required.count:
        if not matches and required.near is not None:
            near = next((line for line in lines if required.near.search(line)), None)
            if near is not None:
                return f'missing line: {required.label}; the log has: {near.strip()}'
        if required.count > 1:
            return f'missing line: {required.label} ({len(matches)} of {required.count})'
        return f'missing line: {required.label}'
    values = [int(match.group(1)) for match in matches] if (required.at_least is not None
                                                            or required.equals is not None) else []
    if required.equals is not None and required.equals not in values:
        return f'{matches[0].group(0)}: expected {required.equals}'
    if required.at_least is not None and max(values) < required.at_least:
        best = matches[values.index(max(values))].group(0)
        return f'{best}: expected at least {required.at_least}'
    return None


def problems(case: Case, status: int | None, text: str, timeout: float, video: bool = False) -> list[str]:
    """Every way the run of CASE differs from what it must do, in the order the first is reported:
    the status (None for a timeout, negative for a signal), then the forbidden lines (with VIDEO,
    the first render fault among them), then the required ones (with VIDEO, the video lines after the case's own). Empty when the case passes."""
    lines = text.splitlines()
    found = []
    if status is None:
        last = lines[-1].strip() if lines else '(empty log)'
        found.append(f'timed out after {timeout:.0f} s; last line: {last}')
    elif status < 0:
        found.append(f'killed by signal {-status}')
    elif status != case.status:
        found.append(f'exit status {status}, expected {case.status}')
    for markers in (SANITIZER_MARKERS, (FATAL_SIGNAL,), (ASSERTION_FAULT,)):
        line = first_line_with(lines, markers)
        if line is not None:
            found.append(line.strip())
    for pattern in case.forbidden:
        line = next((line for line in lines if pattern.search(line)), None)
        if line is not None:
            found.append(line.strip())
    if video:
        line = render_fault(lines)
        if line is not None:
            found.append(line.strip())
    refusals = failing_lp64_lines(lines)
    if refusals:
        found.append(refusals[0].strip())
    torn = torn_play_lines(lines)
    if torn:
        found.append(f'torn line (another stream cut it): {torn[0].strip()}')
    for required in case.required + (video_required(case) if video else ()):
        problem = requirement_problem(lines, required)
        if problem is not None:
            found.append(problem)
    return found
